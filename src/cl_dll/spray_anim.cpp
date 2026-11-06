//=========================================================
// SBO 2.0: animated sprays. docs/ANIMATED_SPRAYS.md.
//
// The engine draws every spray of a player with one texture, made from the first frame of the player's
// tempdecal.wad. The client keeps the whole file of each player (player_info_t.customdata: the customization's
// pBuffer), and spray_anim_core.cpp reads the other frames from it. Here the current frame is put into that texture
// (glTexSubImage2D), so every spray of the player on the map plays, with the engine's clipping, lighting and removal.
//
// The texture is found through the engine's decal cache, as Draw_DecalTexture does (HL25 hw.dll, build of 2024-10-07):
// customization.pInfo is the player's cachewad_t, its cache[nUserData1] the cachepic_t whose data is the texture_t,
// and texture_t.gl_texturenum the GL texture. These are the engine's own structures, not part of the SDK: every value
// read is checked against what the engine writes there, and on any mismatch the spray stays still.
//
// Colors: the engine uploads the first frame from the raw palette (HL25 Draw_MiptexTexture changes nothing in it), its
// texture upload may apply texture gamma. The color curve the frames are put through is learned once per game from a
// texture read back from GL and compared with the palette, and only when the read-back is the first frame: the
// engine keeps a decal texture by name across map loads (the same map again, or a reconnect) and hands the existing
// GL texture to the new map without uploading it again, so it can still hold any frame this file put there.
//
// GL functions are looked up in the current context as cl_dll/worldtext_gl.cpp does.
//=========================================================

#include <string.h>
#include <vector>
#include "spray_anim_core.h"
#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "r_studioint.h"
#include "spray_anim.h"

#ifdef _WIN32
#include <windows.h>
#define SGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define SGL_APIENTRY
#endif

extern engine_studio_api_t IEngineStudio;

// The engine's decal cache (HL25 hw.dll: Draw_CustomCacheGet, Draw_MiptexTexture)
struct engine_cachepic_t
{
	char	name[64];
	void	*data;			// cache_user_t: the texture_t, NULL while not loaded
};

struct engine_cachewad_t
{
	char				*name;
	engine_cachepic_t	*cache;
	int					cacheCount;
	int					cacheMax;
	void				*lumps;
	int					lumpCount;
	int					cacheExtra;		// sizeof( engine_texture_t ) - sizeof( miptex_t )
};

struct engine_texture_t		// the GL engine's texture_t
{
	char		name[16];		// '{': transparent index 255
	unsigned	width, height;
	int			gl_texturenum;
	void		*texturechain;
	int			anim_total, anim_min, anim_max;
	void		*anim_next, *alternate_anims;
	unsigned	offsets[4];		// cacheExtra + the miptex's offsets
	unsigned	paloffset;
};

#define DECAL_EXTRASIZE	32		// sizeof( engine_texture_t ) - sizeof( miptex_t ) in the 32-bit engine

typedef unsigned int	SGLenum;
typedef int				SGLint;
typedef unsigned char	SGLboolean;

#define SGL_TEXTURE_2D			0x0DE1
#define SGL_TEXTURE_BINDING_2D	0x8069
#define SGL_TEXTURE_WIDTH		0x1000
#define SGL_TEXTURE_HEIGHT		0x1001
#define SGL_RGBA				0x1908
#define SGL_UNSIGNED_BYTE		0x1401
#define SGL_PACK_ALIGNMENT		0x0D05
#define SGL_UNPACK_ALIGNMENT	0x0CF5

static struct
{
	void		( SGL_APIENTRY *GetIntegerv )( SGLenum pname, SGLint *params );
	void		( SGL_APIENTRY *BindTexture )( SGLenum target, unsigned int texture );
	SGLboolean	( SGL_APIENTRY *IsTexture )( unsigned int texture );
	void		( SGL_APIENTRY *GetTexLevelParameteriv )( SGLenum target, SGLint level, SGLenum pname, SGLint *params );
	void		( SGL_APIENTRY *GetTexImage )( SGLenum target, SGLint level, SGLenum format, SGLenum type, void *pixels );
	void		( SGL_APIENTRY *TexSubImage2D )( SGLenum target, SGLint level, SGLint x, SGLint y, SGLint w, SGLint h,
					SGLenum format, SGLenum type, const void *pixels );
	void		( SGL_APIENTRY *PixelStorei )( SGLenum pname, SGLint param );
} qgl;

static void *s_pContext = NULL;
static bool s_bAvailable = false;

static void *CurrentContext( void )
{
#ifdef _WIN32
	typedef HGLRC ( WINAPI *pfnGetCurrentContext )( void );
	static pfnGetCurrentContext s_pfn = NULL;
	if ( !s_pfn )
	{
		HMODULE gl = GetModuleHandleA( "opengl32.dll" );
		s_pfn = gl ? (pfnGetCurrentContext)GetProcAddress( gl, "wglGetCurrentContext" ) : NULL;
		if ( !s_pfn )
			return NULL;
	}
	return (void *)s_pfn();
#else
	return (void *)SDL_GL_GetCurrentContext();
#endif
}

static void *GetProc( const char *name )
{
#ifdef _WIN32
	// all GL 1.1 functions, exported by opengl32.dll itself
	HMODULE gl = GetModuleHandleA( "opengl32.dll" );
	return gl ? (void *)GetProcAddress( gl, name ) : NULL;
#else
	return SDL_GL_GetProcAddress( name );
#endif
}

template <typename T> static bool Resolve( T &fn, const char *name )
{
	fn = (T)GetProc( name );
	return fn != NULL;
}

static bool Available( void )
{
	void *context = CurrentContext();
	if ( !context )
		return false;

	if ( context != s_pContext )
	{
		s_pContext = context;
		s_bAvailable =
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.BindTexture, "glBindTexture" ) &&
			Resolve( qgl.IsTexture, "glIsTexture" ) &&
			Resolve( qgl.GetTexLevelParameteriv, "glGetTexLevelParameteriv" ) &&
			Resolve( qgl.GetTexImage, "glGetTexImage" ) &&
			Resolve( qgl.TexSubImage2D, "glTexSubImage2D" ) &&
			Resolve( qgl.PixelStorei, "glPixelStorei" );
	}

	return s_bAvailable;
}

#define SPRAY_PLAYERS	32		// MAX_CLIENTS

struct SprayPlayer
{
	// the file this state was made from
	const void		*buffer;
	int				size;
	unsigned char	md5[16];

	bool			animated;
	SprayAnimInfo	info;
	std::vector<unsigned char> pixels;	// the indices of frame
	int				frame;

	// the engine's texture
	const void		*texture;			// the cache entry's texture_t
	int				texnum;
	int				shown;				// the frame in it, -1: unknown
};

static SprayPlayer s_Players[SPRAY_PLAYERS];
static unsigned char s_Curve[256];		// file color -> texture color (the engine's texture upload)
static bool s_bCurveLearned = false;
static float s_flLastTime = -1.0f;

static void ResetPlayer( SprayPlayer &p )
{
	p.buffer = NULL;
	p.size = 0;
	p.animated = false;
	p.texture = NULL;
	p.texnum = 0;
	p.shown = -1;
	p.frame = 0;
}

// The player's spray: the first decal customization in the list
static customization_t *FindSpray( player_info_t *pInfo )
{
	int depth = 0;
	for ( customization_t *c = pInfo->customdata.pNext; c && depth < 16; c = c->pNext, depth++ )
	{
		if ( c->bInUse && c->resource.type == t_decal && c->pBuffer && c->pInfo )
			return c;
	}
	return NULL;
}

// The texture_t the engine draws the spray with, NULL while not loaded or when the cache is not what the engine
// writes for a spray
static const engine_texture_t *FindTexture( const customization_t *c, const SprayAnimInfo &info )
{
	const engine_cachewad_t *wad = (const engine_cachewad_t *)c->pInfo;
	if ( wad->lumpCount != 1 || wad->cacheExtra != DECAL_EXTRASIZE || !wad->cache
		|| wad->cacheCount < 1 || wad->cacheCount > wad->cacheMax || wad->cacheMax > 64 )
		return NULL;

	const int index = c->nUserData1;
	if ( index < 0 || index >= wad->cacheCount )
		return NULL;

	const engine_texture_t *tex = (const engine_texture_t *)wad->cache[index].data;
	if ( !tex )
		return NULL;

	if ( tex->name[0] != '{' || tex->width != (unsigned)info.width || tex->height != (unsigned)info.height
		|| tex->offsets[0] != DECAL_EXTRASIZE + 40 || tex->gl_texturenum <= 0 )
		return NULL;

	return tex;
}

// The color curve of the texture upload, from the first frame as the engine put it in the texture (bound). False when
// the texture does not hold the first frame (one value of the file read back as different values, or the
// transparency differs): a texture the engine kept from an earlier map, with a later frame in it.
#define CURVE_TOLERANCE	4

static bool LearnCurve( const SprayPlayer &p, int w, int h )
{
	if ( w != p.info.width || h != p.info.height )
		return false;		// scaled by the engine

	std::vector<unsigned char> rgba( w * h * 4 );
	SGLint pack = 4;
	qgl.GetIntegerv( SGL_PACK_ALIGNMENT, &pack );
	qgl.PixelStorei( SGL_PACK_ALIGNMENT, 4 );
	qgl.GetTexImage( SGL_TEXTURE_2D, 0, SGL_RGBA, SGL_UNSIGNED_BYTE, &rgba[0] );
	qgl.PixelStorei( SGL_PACK_ALIGNMENT, pack );

	unsigned sum[256] = { 0 }, count[256] = { 0 };
	unsigned char lo[256], hi[256];
	memset( lo, 255, sizeof( lo ) );
	memset( hi, 0, sizeof( hi ) );
	for ( int i = 0; i < w * h; i++ )
	{
		const int index = p.info.frame0[i];
		const bool bTransparent = rgba[i * 4 + 3] < 128;
		if ( bTransparent != ( index == 255 ) )
			return false;
		if ( index == 255 )
			continue;
		for ( int c = 0; c < 3; c++ )
		{
			const int from = p.info.palette[index * 3 + c];
			const unsigned char to = rgba[i * 4 + c];
			sum[from] += to;
			count[from]++;
			if ( to < lo[from] )
				lo[from] = to;
			if ( to > hi[from] )
				hi[from] = to;
		}
	}

	for ( int i = 0; i < 256; i++ )
	{
		if ( count[i] && hi[i] - lo[i] > CURVE_TOLERANCE )
			return false;
	}

	// the values seen, with straight lines between them (0 and 255 stay where they are)
	unsigned char curve[256];
	for ( int i = 0; i < 256; i++ )
		curve[i] = (unsigned char)i;
	int last = 0, lastValue = 0;
	for ( int i = 1; i < 256; i++ )
	{
		if ( !count[i] && i < 255 )
			continue;
		const int value = count[i] ? (int)( ( sum[i] + count[i] / 2 ) / count[i] ) : 255;
		for ( int j = last + 1; j <= i; j++ )
			curve[j] = (unsigned char)( lastValue + ( value - lastValue ) * ( j - last ) / ( i - last ) );
		last = i;
		lastValue = value;
	}
	if ( count[0] )
		curve[0] = (unsigned char)( ( sum[0] + count[0] / 2 ) / count[0] );

	memcpy( s_Curve, curve, sizeof( s_Curve ) );
	return true;
}

// pixels into the bound texture, every mip level the texture has (w x h is level 0)
static void Upload( const SprayPlayer &p, int w, int h )
{
	const SprayAnimInfo &info = p.info;
	std::vector<unsigned char> level( w * h * 4 );

	for ( int y = 0; y < h; y++ )
	{
		const int sy = y * info.height / h;
		for ( int x = 0; x < w; x++ )
		{
			const int sx = x * info.width / w;
			const int index = p.pixels[sy * info.width + sx];
			unsigned char *out = &level[( y * w + x ) * 4];
			if ( index == 255 )
			{
				out[0] = out[1] = out[2] = out[3] = 0;
				continue;
			}
			out[0] = s_Curve[info.palette[index * 3]];
			out[1] = s_Curve[info.palette[index * 3 + 1]];
			out[2] = s_Curve[info.palette[index * 3 + 2]];
			out[3] = 255;
		}
	}

	// transparent pixels take the color of their opaque neighbors, so filtering leaves no dark rim (as the engine's
	// upload of alpha-tested textures does)
	for ( int y = 0; y < h; y++ )
	{
		for ( int x = 0; x < w; x++ )
		{
			unsigned char *out = &level[( y * w + x ) * 4];
			if ( out[3] )
				continue;
			int r = 0, g = 0, b = 0, n = 0;
			for ( int dy = -1; dy <= 1; dy++ )
			{
				for ( int dx = -1; dx <= 1; dx++ )
				{
					const int nx = x + dx, ny = y + dy;
					if ( nx < 0 || ny < 0 || nx >= w || ny >= h )
						continue;
					const unsigned char *in = &level[( ny * w + nx ) * 4];
					if ( in[3] != 255 )
						continue;
					r += in[0];
					g += in[1];
					b += in[2];
					n++;
				}
			}
			if ( n )
			{
				out[0] = (unsigned char)( r / n );
				out[1] = (unsigned char)( g / n );
				out[2] = (unsigned char)( b / n );
			}
		}
	}

	SGLint unpack = 4;
	qgl.GetIntegerv( SGL_UNPACK_ALIGNMENT, &unpack );
	qgl.PixelStorei( SGL_UNPACK_ALIGNMENT, 4 );

	for ( int mip = 0; mip < 16; mip++ )
	{
		qgl.TexSubImage2D( SGL_TEXTURE_2D, mip, 0, 0, w, h, SGL_RGBA, SGL_UNSIGNED_BYTE, &level[0] );

		SGLint nw = 0, nh = 0;
		qgl.GetTexLevelParameteriv( SGL_TEXTURE_2D, mip + 1, SGL_TEXTURE_WIDTH, &nw );
		qgl.GetTexLevelParameteriv( SGL_TEXTURE_2D, mip + 1, SGL_TEXTURE_HEIGHT, &nh );
		if ( nw <= 0 || nh <= 0 || nw > w || nh > h )
			break;

		// the next level: 2 x 2 averages
		std::vector<unsigned char> next( nw * nh * 4 );
		for ( int y = 0; y < nh; y++ )
		{
			for ( int x = 0; x < nw; x++ )
			{
				for ( int c = 0; c < 4; c++ )
				{
					int s = 0, n = 0;
					for ( int dy = 0; dy < 2; dy++ )
					{
						for ( int dx = 0; dx < 2; dx++ )
						{
							const int sx = x * w / nw + dx, sy = y * h / nh + dy;
							if ( sx < w && sy < h )
							{
								s += level[( sy * w + sx ) * 4 + c];
								n++;
							}
						}
					}
					next[( y * nw + x ) * 4 + c] = (unsigned char)( s / n );
				}
			}
		}
		level.swap( next );
		w = nw;
		h = nh;
	}

	qgl.PixelStorei( SGL_UNPACK_ALIGNMENT, unpack );
}

static void UpdatePlayer( int slot, unsigned int time )
{
	SprayPlayer &p = s_Players[slot];

	player_info_t *pInfo = IEngineStudio.PlayerInfo( slot );
	customization_t *c = pInfo ? FindSpray( pInfo ) : NULL;
	if ( !c )
	{
		ResetPlayer( p );
		return;
	}

	// a new spray: read it (the engine draws files of 1 .. 20 KB only)
	const int size = c->resource.nDownloadSize;
	if ( c->pBuffer != p.buffer || size != p.size || memcmp( c->resource.rgucMD5_hash, p.md5, 16 ) != 0 )
	{
		ResetPlayer( p );
		p.buffer = c->pBuffer;
		p.size = size;
		memcpy( p.md5, c->resource.rgucMD5_hash, 16 );
		p.animated = size >= 1024 && size <= 20480
			&& SprayAnim_Parse( (const unsigned char *)c->pBuffer, size, p.info );
		if ( p.animated )
			p.pixels.assign( p.info.frame0, p.info.frame0 + p.info.width * p.info.height );
	}

	if ( !p.animated )
		return;

	const engine_texture_t *tex = FindTexture( c, p.info );
	if ( !tex || !qgl.IsTexture( (unsigned)tex->gl_texturenum ) )
		return;		// not drawn yet (the engine loads it with the first spray), or not what was expected

	const int want = SprayAnim_FrameAt( p.info, time );
	const bool bNewTexture = ( tex != p.texture || tex->gl_texturenum != p.texnum );
	if ( !bNewTexture && want == p.shown )
		return;

	SGLint bound = 0;
	qgl.GetIntegerv( SGL_TEXTURE_BINDING_2D, &bound );
	qgl.BindTexture( SGL_TEXTURE_2D, (unsigned)tex->gl_texturenum );

	SGLint w = 0, h = 0;
	qgl.GetTexLevelParameteriv( SGL_TEXTURE_2D, 0, SGL_TEXTURE_WIDTH, &w );
	qgl.GetTexLevelParameteriv( SGL_TEXTURE_2D, 0, SGL_TEXTURE_HEIGHT, &h );

	if ( w > 0 && h > 0 && w <= 1024 && h <= 1024 )
	{
		if ( bNewTexture )
		{
			// made from the first frame, or kept from an earlier map with any frame in it: the frame is put in at once
			if ( !s_bCurveLearned )
				s_bCurveLearned = LearnCurve( p, w, h );
			p.texture = tex;
			p.texnum = tex->gl_texturenum;
			p.shown = -1;
		}

		if ( want != p.shown )
		{
			if ( want < p.frame )
			{
				memcpy( &p.pixels[0], p.info.frame0, p.pixels.size() );
				p.frame = 0;
			}
			while ( p.frame < want )
				SprayAnim_Decode( p.info, ++p.frame, &p.pixels[0] );

			Upload( p, w, h );
			p.shown = want;
		}
	}

	qgl.BindTexture( SGL_TEXTURE_2D, (unsigned)bound );
}

void SprayAnim_Frame( void )
{
	// until a texture shows the curve: the file's colors
	static bool s_bCurveInit = false;
	if ( !s_bCurveInit )
	{
		for ( int i = 0; i < 256; i++ )
			s_Curve[i] = (unsigned char)i;
		s_bCurveInit = true;
	}

	// once per frame (HUD_DrawNormalTriangles runs for each view), OpenGL only
	const float flTime = gEngfuncs.GetClientTime();
	if ( flTime == s_flLastTime )
		return;
	s_flLastTime = flTime;

	if ( IEngineStudio.IsHardware() != 1 || !Available() )
		return;

	int maxClients = gEngfuncs.GetMaxClients();
	if ( maxClients > SPRAY_PLAYERS )
		maxClients = SPRAY_PLAYERS;

	const unsigned int time = (unsigned int)( flTime * 1000.0f );
	for ( int i = 0; i < maxClients; i++ )
		UpdatePlayer( i, time );
}
