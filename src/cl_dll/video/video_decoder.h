//=========================================================
// SBO 2.0: game_playvideo, the client's Ogg Theora / Vorbis decoder (docs/VIDEO_PLAYER.md).
//
// Port of Aura-CL's CTheoraDecoder / CAVStream (cl_dll/video/, The Phoenix Project Software): the same libraries for
// the picture (libogg + libtheora, vendored in external/), the sound decoded with stb_vorbis instead of libvorbisfile.
// The whole file is in memory (the engine's file system loads it, video_player.cpp). No engine, GL or SDL code here,
// so tests/video/ builds this file alone with AddressSanitizer and UndefinedBehaviorSanitizer.
//
// Sound: the Vorbis track inside the .ogv (its pages are collected when the file is opened), or a separate Ogg Vorbis
// file (OpenAudioFile, Aura's "audio" key) that replaces it.
//=========================================================

#if !defined( VIDEO_DECODER_H )
#define VIDEO_DECODER_H

#include <stddef.h>
#include <vector>

struct VideoDecoderState;

class CVideoDecoder
{
public:
	// limits for files from a server or a map: larger pictures or files are refused
	enum { MAX_SIZE = 2048, MAX_FILE_BYTES = 128 * 1024 * 1024, MAX_FRAMES_PER_CALL = 240 };

	CVideoDecoder();
	~CVideoDecoder();

	// Reads the headers of an Ogg Theora file. The data must stay valid until Close (it is not copied). False with
	// Error() set when there is no Theora track, the headers are broken or the picture is too large.
	bool Open( const unsigned char *data, size_t size );
	void Close( void );

	int Width( void ) const { return m_iWidth; }		// the picture, pixels
	int Height( void ) const { return m_iHeight; }
	double FrameRate( void ) const { return m_flFrameRate; }

	// Decodes every frame due by time (seconds from the start). True when the picture changed: RGBA() holds it,
	// Width() x Height() pixels, 4 bytes each, top row first.
	// maxFrames: at most that many frames this call (0: MAX_FRAMES_PER_CALL); the rest follows in later calls.
	bool DecodeToTime( double time, int maxFrames = 0 );
	const unsigned char *RGBA( void ) const { return m_RGBA.empty() ? NULL : &m_RGBA[0]; }
	bool IsFinished( void ) const { return m_bFinished; }
	// RGBA() bottom row first (OpenGL's order: one texture upload for a world screen); default top row first
	void SetBottomUp( bool bottomUp ) { m_bBottomUp = bottomUp; }
	int FramesDecoded( void ) const { return m_iFrame; }

	// A separate Ogg Vorbis file instead of the video's own sound. Copied; false (and the video's own sound kept)
	// when it is not Ogg Vorbis.
	bool OpenAudioFile( const unsigned char *data, size_t size );
	// 0 when the video has no sound (or it could not be read)
	int AudioRate( void ) const;
	// Interleaved 16-bit stereo: up to frames sample frames into out. Returns how many were written; 0 at the end.
	int ReadAudio( short *out, int frames );

	// Length (frames of the last Theora page + 1, divided by the frame rate); 0 when unknown
	double Duration( void ) const;
	// Picture: decoding goes on from the keyframe at or before time (seconds); the next DecodeToTime( t >= time )
	// decodes up to it (at most a keyframe interval of frames). False when the file has no frame there.
	bool SeekTo( double time );
	// Sound: from time on (stb_vorbis_seek). False past its end (ReadAudio then gives 0).
	bool SeekAudio( double time );
	// Seconds of sound read so far (stb_vorbis's sample offset)
	double AudioPosition( void ) const;

	const char *Error( void ) const { return m_pszError; }

private:
	bool NextVideoPacket( void );	// one Theora packet into the state's packet; false at the end of the file
	bool FeedPage( void );			// the next Theora page into the stream state; false at the end of the file
	void ConvertFrame( void );
	bool OpenAudioData( void );

	VideoDecoderState		*m_pState;
	const unsigned char		*m_pData;
	size_t					m_iSize;
	size_t					m_iRead;			// bytes handed to the Ogg sync layer

	int						m_iWidth, m_iHeight;
	double					m_flFrameRate;
	int						m_iFrame;			// frames decoded
	bool					m_bPendingPacket;	// the first data packet came with the headers
	bool					m_bFinished;
	std::vector<unsigned char> m_RGBA;

	// seek index: the Theora pages that end a frame (file offset, granule position, frame number)
	struct IndexEntry
	{
		size_t		offset;
		long long	granule;
		long long	frame;
	};
	std::vector<IndexEntry> m_Index;
	long long				m_iFrameCount;
	size_t					m_iDataStart;		// file offset of the first frame's page
	bool					m_bBottomUp;
	long long				m_iSeekKey;			// the keyframe the last seek starts at: earlier frames are not shown

	std::vector<unsigned char> m_Audio;			// the Ogg Vorbis stream (the video's track or the separate file)
	void					*m_pVorbis;			// stb_vorbis *
	int						m_iAudioChannels;
	int						m_iAudioRate;
	bool					m_bAudioEnd;

	const char				*m_pszError;
};

#endif // VIDEO_DECODER_H
