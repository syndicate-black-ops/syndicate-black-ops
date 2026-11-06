//=========================================================
// SBO 2.0: game_playvideo, the client's Ogg Theora / Vorbis decoder (video_decoder.h, docs/VIDEO_PLAYER.md).
//
// Follows Aura-CL's CTheoraDecoder (cl_dll/video/theora_decoder.cpp): Ogg pages from libogg's sync layer, the Theora
// headers with th_decode_headerin, frames with th_decode_packetin / th_decode_ycbcr_out, Y'CbCr to RGBA (BT.601).
// Changes: the file is read from memory (the engine's file system) instead of fopen; frame times count frames
// (frame n is shown at n / frame rate) and every frame up to the time is decoded, so a slow frame never drops
// reference frames; 4:2:2 and 4:4:4 files and the picture region (pic_x / pic_y) are handled; the picture size is
// limited (MAX_SIZE). The sound is the file's own Vorbis track (all its pages copied into one Ogg stream when the
// file is opened) or a separate file, decoded by stb_vorbis's memory reader.
//=========================================================

#include "video_decoder.h"

#include <ogg/ogg.h>
#include <theora/theoradec.h>

#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"

struct VideoDecoderState
{
	ogg_sync_state		sync;
	ogg_stream_state	stream;		// the Theora track
	ogg_page			page;
	ogg_packet			packet;
	int					serial;
	bool				streamInit;
	th_info				info;
	th_comment			comment;
	th_setup_info		*setup;
	th_dec_ctx			*dec;
	th_ycbcr_buffer		ycbcr;
	bool				haveFrame;
	size_t				pos;		// file offset of the next byte the sync layer has not returned
	size_t				pageOffset;	// file offset of the page NextPage returned last
};

static const size_t READ_CHUNK = 64 * 1024;

CVideoDecoder::CVideoDecoder() : m_pState( NULL ), m_pData( NULL ), m_iSize( 0 ), m_iRead( 0 ), m_iWidth( 0 ),
	m_iHeight( 0 ), m_flFrameRate( 0 ), m_iFrame( 0 ), m_bPendingPacket( false ), m_bFinished( false ),
	m_iFrameCount( 0 ), m_iDataStart( 0 ), m_bBottomUp( false ), m_iSeekKey( 0 ), m_pVorbis( NULL ), m_iAudioChannels( 0 ), m_iAudioRate( 0 ), m_bAudioEnd( false ), m_pszError( NULL )
{
}

CVideoDecoder::~CVideoDecoder()
{
	Close();
}

void CVideoDecoder::Close( void )
{
	if ( m_pState )
	{
		if ( m_pState->dec )
			th_decode_free( m_pState->dec );
		if ( m_pState->setup )
			th_setup_free( m_pState->setup );
		th_comment_clear( &m_pState->comment );
		th_info_clear( &m_pState->info );
		if ( m_pState->streamInit )
			ogg_stream_clear( &m_pState->stream );
		ogg_sync_clear( &m_pState->sync );
		delete m_pState;
		m_pState = NULL;
	}
	if ( m_pVorbis )
	{
		stb_vorbis_close( (stb_vorbis *)m_pVorbis );
		m_pVorbis = NULL;
	}
	m_pData = NULL;
	m_iSize = m_iRead = 0;
	m_iWidth = m_iHeight = 0;
	m_flFrameRate = 0;
	m_iFrame = 0;
	m_bPendingPacket = m_bFinished = false;
	m_RGBA.clear();
	m_Index.clear();
	m_iFrameCount = 0;
	m_iDataStart = 0;
	m_iSeekKey = 0;
	m_Audio.clear();
	m_iAudioChannels = m_iAudioRate = 0;
	m_bAudioEnd = false;
}

// The next page of the file (any track); false at the end of the data. Tracks where it starts in the file
// (s->pageOffset), for the seek index.
static bool NextPage( VideoDecoderState *s, const unsigned char *data, size_t size, size_t &read )
{
	for ( ;; )
	{
		long r = ogg_sync_pageseek( &s->sync, &s->page );
		if ( r > 0 )
		{
			s->pageOffset = s->pos;
			s->pos += (size_t)r;
			return true;
		}
		if ( r < 0 )
		{
			s->pos += (size_t)( -r );	// bytes skipped to the next page
			continue;
		}

		if ( read >= size )
			return false;
		size_t n = size - read;
		if ( n > READ_CHUNK )
			n = READ_CHUNK;
		char *buf = ogg_sync_buffer( &s->sync, (long)n );
		if ( !buf )
			return false;
		memcpy( buf, data + read, n );
		ogg_sync_wrote( &s->sync, (long)n );
		read += n;
	}
}

static bool PacketStarts( const ogg_packet &p, const char *magic, size_t len )
{
	return p.bytes >= (long)len && memcmp( p.packet, magic, len ) == 0;
}

bool CVideoDecoder::Open( const unsigned char *data, size_t size )
{
	Close();
	m_pszError = NULL;

	if ( !data || size < 28 || size > (size_t)MAX_FILE_BYTES )
	{
		m_pszError = "not an Ogg file, or larger than 128 MB";
		return false;
	}

	m_pData = data;
	m_iSize = size;
	m_pState = new VideoDecoderState;
	VideoDecoderState *s = m_pState;
	memset( &s->stream, 0, sizeof( s->stream ) );
	s->streamInit = false;
	s->serial = 0;
	s->setup = NULL;
	s->dec = NULL;
	s->haveFrame = false;
	s->pos = s->pageOffset = 0;
	ogg_sync_init( &s->sync );
	th_info_init( &s->info );
	th_comment_init( &s->comment );

	// Pass 1 over the whole file: find the Theora and Vorbis tracks (each starts with a page marked "beginning of
	// stream" whose first packet names the codec) and copy the Vorbis track's pages into one stream for stb_vorbis.
	{
		size_t read = 0;
		bool haveTheora = false, haveVorbis = false;
		int vorbisSerial = 0;
		while ( NextPage( s, data, size, read ) )
		{
			int serial = ogg_page_serialno( &s->page );
			if ( ogg_page_bos( &s->page ) )
			{
				ogg_stream_state probe;
				ogg_stream_init( &probe, serial );
				ogg_packet p;
				if ( ogg_stream_pagein( &probe, &s->page ) == 0 && ogg_stream_packetout( &probe, &p ) == 1 )
				{
					if ( !haveTheora && PacketStarts( p, "\x80theora", 7 ) )
					{
						haveTheora = true;
						s->serial = serial;
					}
					else if ( !haveVorbis && PacketStarts( p, "\x01vorbis", 7 ) )
					{
						haveVorbis = true;
						vorbisSerial = serial;
					}
				}
				ogg_stream_clear( &probe );
			}
			// the seek index: every Theora page that ends a frame, with its granule position
			if ( haveTheora && serial == s->serial && ogg_page_granulepos( &s->page ) >= 0 && m_Index.size() < 1000000 )
			{
				IndexEntry e;
				e.offset = s->pageOffset;
				e.granule = ogg_page_granulepos( &s->page );
				e.frame = -1;
				m_Index.push_back( e );
			}
			if ( haveVorbis && serial == vorbisSerial )
			{
				m_Audio.insert( m_Audio.end(), s->page.header, s->page.header + s->page.header_len );
				m_Audio.insert( m_Audio.end(), s->page.body, s->page.body + s->page.body_len );
			}
		}
		ogg_sync_reset( &s->sync );	// pass 2 (the picture) reads the file again from the start
		s->pos = 0;

		if ( !haveTheora )
		{
			m_Audio.clear();
			m_pszError = "no Ogg Theora video track";
			Close();
			return false;
		}
	}

	ogg_stream_init( &s->stream, s->serial );
	s->streamInit = true;

	// The Theora headers (three packets); the packet after them is the first frame
	for ( ;; )
	{
		if ( !NextVideoPacket() )
		{
			m_pszError = "the Theora headers are incomplete";
			Close();
			return false;
		}
		int r = th_decode_headerin( &s->info, &s->comment, &s->setup, &s->packet );
		if ( r == 0 )
		{
			m_bPendingPacket = true;	// a frame: decoded first by DecodeToTime
			m_iDataStart = s->pageOffset;	// the first frame starts a page of its own (Theora specification)
			break;
		}
		if ( r < 0 )
		{
			m_pszError = "broken Theora headers";
			Close();
			return false;
		}
	}

	const th_info &ti = s->info;
	if ( !s->setup || ti.pic_width == 0 || ti.pic_height == 0 || ti.pic_width > MAX_SIZE || ti.pic_height > MAX_SIZE
		|| ti.pic_x + ti.pic_width > ti.frame_width || ti.pic_y + ti.pic_height > ti.frame_height
		|| ti.fps_numerator == 0 || ti.fps_denominator == 0 || ti.pixel_fmt == TH_PF_RSVD )
	{
		m_pszError = "unsupported picture (size, frame rate or pixel format)";
		Close();
		return false;
	}

	m_flFrameRate = (double)ti.fps_numerator / (double)ti.fps_denominator;
	if ( m_flFrameRate < 1.0 || m_flFrameRate > 240.0 )
	{
		m_pszError = "unsupported frame rate (1 to 240)";
		Close();
		return false;
	}

	s->dec = th_decode_alloc( &s->info, s->setup );
	if ( !s->dec )
	{
		m_pszError = "the Theora decoder could not start";
		Close();
		return false;
	}

	// frame numbers of the index; the last one gives the length
	m_iFrameCount = 0;
	{
		// the header pages (before the first frame's page) are no frames
		size_t keep = 0;
		for ( size_t i = 0; i < m_Index.size(); i++ )
		{
			if ( m_Index[i].offset >= m_iDataStart )
				m_Index[keep++] = m_Index[i];
		}
		m_Index.resize( keep );
	}
	for ( size_t i = 0; i < m_Index.size(); i++ )
	{
		m_Index[i].frame = th_granule_frame( s->dec, m_Index[i].granule );
		if ( m_Index[i].frame + 1 > m_iFrameCount )
			m_iFrameCount = m_Index[i].frame + 1;
	}

	m_iWidth = (int)ti.pic_width;
	m_iHeight = (int)ti.pic_height;
	m_RGBA.assign( (size_t)m_iWidth * m_iHeight * 4, 0 );
	for ( size_t i = 3; i < m_RGBA.size(); i += 4 )
		m_RGBA[i] = 255;

	if ( !m_Audio.empty() && !OpenAudioData() )
		m_Audio.clear();	// broken sound: the picture plays without it

	return true;
}

bool CVideoDecoder::FeedPage( void )
{
	VideoDecoderState *s = m_pState;
	while ( NextPage( s, m_pData, m_iSize, m_iRead ) )
	{
		if ( ogg_page_serialno( &s->page ) == s->serial )
		{
			ogg_stream_pagein( &s->stream, &s->page );
			return true;
		}
	}
	return false;
}

bool CVideoDecoder::NextVideoPacket( void )
{
	VideoDecoderState *s = m_pState;
	for ( ;; )
	{
		int r = ogg_stream_packetout( &s->stream, &s->packet );
		if ( r == 1 )
			return true;
		if ( r < 0 )
			continue;	// a gap in the track: the next packet
		if ( !FeedPage() )
			return false;
	}
}

bool CVideoDecoder::DecodeToTime( double time, int maxFrames )
{
	if ( !m_pState || !m_pState->dec || m_bFinished )
		return false;

	VideoDecoderState *s = m_pState;
	bool decoded = false;
	int budget = maxFrames > 0 ? maxFrames : MAX_FRAMES_PER_CALL;

	// frame n is shown from n / frame rate on
	while ( budget-- > 0 && (double)m_iFrame / m_flFrameRate <= time )
	{
		if ( m_bPendingPacket )
			m_bPendingPacket = false;
		else if ( !NextVideoPacket() )
		{
			m_bFinished = true;
			break;
		}

		ogg_int64_t granule = 0;
		int r = th_decode_packetin( s->dec, &s->packet, &granule );
		// after a seek, the frames before the keyframe have no reference: not shown
		if ( r == 0 && m_iFrame >= m_iSeekKey )
		{
			decoded = true;
			s->haveFrame = true;
		}
		// TH_DUPFRAME: the same picture again; another error: a broken frame, the previous picture stays
		m_iFrame++;
	}

	if ( decoded && th_decode_ycbcr_out( s->dec, s->ycbcr ) == 0 )
	{
		ConvertFrame();
		return true;
	}
	return false;
}

double CVideoDecoder::Duration( void ) const
{
	return m_flFrameRate > 0 ? (double)m_iFrameCount / m_flFrameRate : 0;
}

bool CVideoDecoder::SeekTo( double time )
{
	if ( !m_pState || !m_pState->dec )
		return false;
	VideoDecoderState *s = m_pState;

	long long target = (long long)( time * m_flFrameRate );
	if ( target < 0 )
		target = 0;
	if ( m_iFrameCount > 0 && target >= m_iFrameCount )
		target = m_iFrameCount - 1;

	// the keyframe at or before the target: from the granule of the first page that ends at or after it
	long long key = 0;
	size_t i = 0;
	for ( ; i < m_Index.size() && m_Index[i].frame < target; i++ )
		;
	if ( i < m_Index.size() )
	{
		const int shift = s->info.keyframe_granule_shift;
		const ogg_int64_t keyGranule = ( m_Index[i].granule >> shift ) << shift;
		key = th_granule_frame( s->dec, keyGranule );
		if ( key < 0 || key > target )
			key = 0;
	}

	// start at the last page that ends before the keyframe (the keyframe's packet may begin on it); its own packets
	// are dropped. No such page: the first frame's page.
	size_t start = m_iDataStart;
	long long first = 0;
	bool dropFirstPage = false;
	for ( size_t k = 0; k < m_Index.size() && m_Index[k].frame < key; k++ )
	{
		start = m_Index[k].offset;
		first = m_Index[k].frame + 1;
		dropFirstPage = true;
	}

	ogg_sync_reset( &s->sync );
	ogg_stream_reset( &s->stream );
	m_iRead = start;
	s->pos = start;
	m_bPendingPacket = false;
	m_bFinished = false;
	if ( dropFirstPage )
	{
		if ( !FeedPage() )
		{
			m_bFinished = true;
			return false;
		}
		while ( ogg_stream_packetout( &s->stream, &s->packet ) != 0 )
			;
	}
	m_iFrame = (int)first;
	m_iSeekKey = key;
	return true;
}

// BT.601 video range (Theora's default) as tables: R = Y + V1, G = Y - U2 - V2, B = Y + U1 (all scaled by 256)
namespace
{
struct YuvTables
{
	int y[256], u1[256], u2[256], v1[256], v2[256];
	unsigned char clamp[1024];	// index value + 384 (the sums stay within -384 .. 639 after >> 8)
	YuvTables()
	{
		for ( int i = 0; i < 256; i++ )
		{
			y[i] = ( i - 16 ) * 298 + 128;
			u1[i] = 516 * ( i - 128 );
			u2[i] = 100 * ( i - 128 );
			v1[i] = 409 * ( i - 128 );
			v2[i] = 208 * ( i - 128 );
		}
		for ( int i = 0; i < 1024; i++ )
		{
			const int v = i - 384;
			clamp[i] = (unsigned char)( v < 0 ? 0 : ( v > 255 ? 255 : v ) );
		}
	}
};
const YuvTables s_Yuv;

inline unsigned char Clamp8( int sum )
{
	int i = ( sum >> 8 ) + 384;
	if ( i < 0 )
		i = 0;
	else if ( i > 1023 )
		i = 1023;
	return s_Yuv.clamp[i];
}
}

void CVideoDecoder::ConvertFrame( void )
{
	VideoDecoderState *s = m_pState;
	const th_info &ti = s->info;
	const th_img_plane *yp = &s->ycbcr[0], *up = &s->ycbcr[1], *vp = &s->ycbcr[2];

	// chroma subsampling from the pixel format (4:2:0 halves both, 4:2:2 the width, 4:4:4 none)
	const int xs = ( ti.pixel_fmt == TH_PF_444 ) ? 0 : 1;
	const int ys = ( ti.pixel_fmt == TH_PF_420 ) ? 1 : 0;

	// the columns every plane has (the decoder's planes cover the picture region; checked once, not per pixel)
	int width = m_iWidth;
	if ( (int)ti.pic_x + width > yp->width )
		width = yp->width - (int)ti.pic_x;
	while ( width > 0 && ( ( (int)ti.pic_x + width - 1 ) >> xs ) >= ( up->width < vp->width ? up->width : vp->width ) )
		width--;
	if ( width <= 0 )
		return;

	unsigned char *out = &m_RGBA[0];
	for ( int y = 0; y < m_iHeight; y++ )
	{
		const int fy = y + (int)ti.pic_y;
		if ( fy >= yp->height || ( fy >> ys ) >= up->height || ( fy >> ys ) >= vp->height )
			break;
		const unsigned char *yrow = yp->data + (ptrdiff_t)fy * yp->stride + ti.pic_x;
		const unsigned char *urow = up->data + (ptrdiff_t)( fy >> ys ) * up->stride;
		const unsigned char *vrow = vp->data + (ptrdiff_t)( fy >> ys ) * vp->stride;
		const int row = m_bBottomUp ? m_iHeight - 1 - y : y;
		unsigned char *o = out + (size_t)row * m_iWidth * 4;
		for ( int x = 0; x < width; x++ )
		{
			const int cx = ( x + (int)ti.pic_x ) >> xs;
			const int c = s_Yuv.y[yrow[x]];
			const int u = urow[cx], v = vrow[cx];
			o[0] = Clamp8( c + s_Yuv.v1[v] );
			o[1] = Clamp8( c - s_Yuv.u2[u] - s_Yuv.v2[v] );
			o[2] = Clamp8( c + s_Yuv.u1[u] );
			o[3] = 255;
			o += 4;
		}
	}
}

bool CVideoDecoder::OpenAudioData( void )
{
	if ( m_pVorbis )
	{
		stb_vorbis_close( (stb_vorbis *)m_pVorbis );
		m_pVorbis = NULL;
	}
	m_iAudioChannels = m_iAudioRate = 0;
	m_bAudioEnd = false;

	if ( m_Audio.empty() || m_Audio.size() > (size_t)MAX_FILE_BYTES )
		return false;

	int error = 0;
	stb_vorbis *v = stb_vorbis_open_memory( &m_Audio[0], (int)m_Audio.size(), &error, NULL );
	if ( !v )
		return false;

	stb_vorbis_info info = stb_vorbis_get_info( v );
	if ( info.channels < 1 || info.sample_rate < 8000 || info.sample_rate > 192000 )
	{
		stb_vorbis_close( v );
		return false;
	}

	m_pVorbis = v;
	m_iAudioChannels = info.channels;
	m_iAudioRate = (int)info.sample_rate;
	return true;
}

bool CVideoDecoder::OpenAudioFile( const unsigned char *data, size_t size )
{
	if ( !data || size == 0 || size > (size_t)MAX_FILE_BYTES )
		return false;

	std::vector<unsigned char> own( m_Audio );
	m_Audio.assign( data, data + size );
	if ( OpenAudioData() )
		return true;

	// not Ogg Vorbis: back to the video's own sound
	m_Audio.swap( own );
	if ( !m_Audio.empty() && !OpenAudioData() )
		m_Audio.clear();
	return false;
}

int CVideoDecoder::AudioRate( void ) const
{
	return m_pVorbis ? m_iAudioRate : 0;
}

bool CVideoDecoder::SeekAudio( double time )
{
	if ( !m_pVorbis )
		return false;
	if ( time < 0 )
		time = 0;
	m_bAudioEnd = false;
	const unsigned int sample = (unsigned int)( time * m_iAudioRate );
	if ( !stb_vorbis_seek( (stb_vorbis *)m_pVorbis, sample ) )
	{
		m_bAudioEnd = true;
		return false;
	}
	return true;
}

double CVideoDecoder::AudioPosition( void ) const
{
	if ( !m_pVorbis || m_iAudioRate <= 0 )
		return 0;
	const int offset = stb_vorbis_get_sample_offset( (stb_vorbis *)m_pVorbis );
	return offset < 0 ? 0 : (double)offset / m_iAudioRate;
}

int CVideoDecoder::ReadAudio( short *out, int frames )
{
	if ( !m_pVorbis || m_bAudioEnd || !out || frames <= 0 )
		return 0;

	// stb_vorbis mixes any channel count down to the two asked for
	int n = stb_vorbis_get_samples_short_interleaved( (stb_vorbis *)m_pVorbis, 2, out, frames * 2 );
	if ( n <= 0 )
	{
		m_bAudioEnd = true;
		return 0;
	}
	return n;
}
