/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#if !defined( PM_MATERIALSH )
#define PM_MATERIALSH
#pragma once

// SBO 2.0 material system (docs/MATERIALS.md): one registry, shared by the client and server player movement
// (footsteps), the server's TEXTURETYPE_* (sound.cpp) and the client's ev_hldm.cpp (bullet hit sounds).
// Design from halflife-featureful's MaterialRegistry (game_shared/tex_materials.cpp, FreeSlave, HL SDK license),
// written again in C for pm_shared.c; data in COM_ParseFile-style text files instead of JSON.

// texture names: a BSP / WAD miptex name has at most 15 characters (16 bytes with the terminator). HL compared only
// the first 12 (CBTEXTURENAMEMAX 13); SBO matches the whole name and falls back to the first 12 (pm_materials.c).
#define CBTEXTURENAMEMAX	16

// A solid studio prop with a material (env_model "Material", SBO 2.0, docs/ENV_MODEL.md): its entity state carries the
// material's letter in endpos[0] and this tag in endpos[1] (fields entity_state_t already sends; env_model does not
// use them otherwise), so the client's bullet impact (ev_hldm.cpp) plays that material's sound and effects
#define MAT_PROP_TAG		4001

#define CHAR_TEX_CONCRETE	'C'			// texture types
#define CHAR_TEX_METAL		'M'
#define CHAR_TEX_DIRT		'D'
#define CHAR_TEX_VENT		'V'
#define CHAR_TEX_GRATE		'G'
#define CHAR_TEX_TILE		'T'
#define CHAR_TEX_SLOSH		'S'
#define CHAR_TEX_WOOD		'W'
#define CHAR_TEX_COMPUTER	'P'
#define CHAR_TEX_GLASS		'Y'
#define CHAR_TEX_FLESH		'F'
// SBO 2.0, the letters of CS:CZ Deleted Scenes
#define CHAR_TEX_GRASS		'A'
#define CHAR_TEX_CARPET		'E'
#define CHAR_TEX_GRAVEL		'R'
#define CHAR_TEX_SNOW		'N'
#define CHAR_TEX_PAPER		'X'
#define CHAR_TEX_CUSTOM		'K'
// SBO 2.0, CS:CZ Deleted Scenes' server-side letters with footsteps of their own
#define CHAR_TEX_SNOW_LIGHT	'L'
#define CHAR_TEX_MUD		'Z'
#define CHAR_TEX_TIN		'I'

#define MAT_TEXTURES_MAX	1024		// texture entries of sound/materials.txt and a map's file together
#define MAT_LEGACY_CHARS	12			// HL compared this many characters of a texture name
#define MAT_SOUNDS_MAX		6			// sounds per list (right foot, left foot, hit)
#define MAT_SOUND_LEN		64			// sound path relative to sound/, with the terminator
#define MAT_NAME_LEN		32			// material name ("concrete")
#define MAT_FILE_MAX		( 256 * 1024 )	// larger files are refused

#define MAT_PATH_LEN		64			// a data file's path, with the terminator (the engine's MAX_QPATH)
#define MAT_INCLUDE_DEPTH	2			// #include in a map's file, and in the file it includes
#define MAT_MAP_FILES_MAX	8			// files a map's file may pull in, itself included

// the files, read on every map (docs/MATERIALS.md): Half-Life's texture list, SBO's texture tags, SBO's types; then
// the map's own file, if it has one
#define MAT_TEXTURE_LIST	"sound/materials.txt"		// Half-Life's (valve/: SBO ships none)
#define MAT_TEXTURES_FILE	"materials/textures.txt"	// SBO's WAD texture tags, over Half-Life's
#define MAT_TYPES_FILE		"materials/types.txt"		// letters -> sounds and impact effects
#define MAT_MAP_FILE		"materials/maps/%s.txt"		// a map's own file (worldspawn materials_file left empty)

// texture list layers: a higher layer wins. A map's file: its own lines MAT_LAYER_MAP + MAT_INCLUDE_DEPTH, the files
// it includes one less per level.
#define MAT_LAYER_VALVE		0
#define MAT_LAYER_GLOBAL	1
#define MAT_LAYER_MAP		2

typedef struct mat_sounds_s
{
	int		count;
	char	name[ MAT_SOUNDS_MAX ][ MAT_SOUND_LEN ];
} mat_sounds_t;

typedef struct mat_step_s
{
	mat_sounds_t	right;			// right foot (HL: pl_step1 / pl_step3)
	mat_sounds_t	left;			// left foot (HL: pl_step2 / pl_step4)
	float			walk_volume;
	int				walk_time;		// msec until the next step
	float			run_volume;
	int				run_time;
	int				skip_steps;		// HL's wading: every fourth step is silent
} mat_step_t;

typedef struct mat_hit_s
{
	mat_sounds_t	sounds;
	float			volume;
	float			volume_bar;		// returned to melee weapons (volume of the swing)
	float			attenuation;
	int				sparks;			// HL computer: a random spark and spark sound on a hit (server)
} mat_hit_t;

// impact effects of a bullet hit (client, docs/MATERIALS.md; CS:CZ Deleted Scenes' debris, smoke, sparks, impacts)
#define MAT_FX_SPRITES		4
#define MAT_DECAL_LEN		16
#define MAT_TEXFX_MAX		32			// per-texture effect overrides (texture blocks)

typedef struct mat_fx_s
{
	int				defined;		// 0: the 1.38 effect (streaks and a puff on every brush hit)
	int				debris_count;	// chips per hit (plus 0-2 at random); 0 none
	int				debris_sprites;
	char			debris_sprite[ MAT_FX_SPRITES ][ MAT_SOUND_LEN ];	// one picked per chip
	float			debris_size;
	float			debris_life;
	float			debris_speed;	// velocity factor (CS:CZ "velmod")
	float			debris_gravity;
	float			debris_damp_time;		// seconds until the chip slows down
	float			debris_damp_speed;		// velocity factor then
	float			debris_afterdamp_gravity;
	float			debris_brightness;
	int				debris_additive;		// render mode: 0 alpha test (kRenderTransAlpha), 1 additive
	float			debris_color[ 3 ];
	int				smoke;			// wall puffs
	float			smoke_color[ 3 ];
	int				sparks;			// spark streaks, a flash and a light
	char			decal[ MAT_DECAL_LEN ];	// "" Half-Life's {shot decals
	mat_sounds_t	ricochet;		// none: Half-Life's ric1-5
} mat_fx_t;

typedef struct mat_type_s
{
	int			defined;
	char		name[ MAT_NAME_LEN ];
	int			has_step;		// no steps: the default material's steps
	mat_step_t	step;
	int			has_hit;		// no hit: the default material's hit
	mat_hit_t	hit;
	mat_fx_t	fx;
} mat_type_t;

// loads a file; the result is freed with the matching free function. The data need not be NUL-terminated.
typedef unsigned char *( *mat_load_fn )( const char *path, int *length );
typedef void ( *mat_free_fn )( unsigned char *data );
typedef void ( *mat_log_fn )( const char *text );
typedef void ( *mat_file_fn )( const char *path, void *ctx );

#ifdef __cplusplus
extern "C" {
#endif

// not exported from hl.so (which exports everything else, as the SDK's): the registry's names cannot bind to another
// module's symbols on a listen server
#if defined( __GNUC__ ) && !defined( _WIN32 )
#pragma GCC visibility push( hidden )
#endif

// built-in Half-Life types, no textures; every type usable
void MAT_Reset( void );
int MAT_IsLoaded( void );		// MAT_LoadGlobal has run
void MAT_SetLog( mat_log_fn log );

// sound/materials.txt format ("C TEXTURENAME" per line). layer: MAT_LAYER_*; a higher layer overrides a lower one,
// inside one layer the first entry of a name wins. Returns the entries added.
int MAT_ParseTextureList( const char *data, int length, int layer, const char *source );
// materials/types.txt format (docs/MATERIALS.md). All or nothing: on an error nothing of the file is used.
// Returns 1 on success.
int MAT_ParseTypes( const char *data, int length, const char *source );
// MAT_Reset, then the global files (sound/materials.txt, materials/textures.txt, materials/types.txt) through the
// loader
void MAT_LoadGlobal( mat_load_fn load, mat_free_fn release );
// a map's materials file (docs/MATERIALS.md): a texture list whose entries override the global ones, with the lines
// "#include <file>" (another such file, below the including file's own entries; 2 deep at most), "#types <file>"
// (a types file) and inline types blocks ("material X { }", "texture "NAME" { }", "ladder { }", "wade { }").
// Types: the included files' first, then the file's own #types and blocks in file order, all over
// materials/types.txt. onFile is called with every file read (the server puts them in the download list).
// Returns the files read, -1 if the path is refused or the file missing.
int MAT_LoadMapFile( mat_load_fn load, mat_free_fn release, const char *path, mat_file_fn onFile, void *ctx );
// a data file path from a map or a server: relative, [A-Za-z0-9_-./], one dot, ".txt", under 64 characters
int MAT_IsSafeDataPath( const char *path );

// calls back with every file an impact effect names, relative to the game folder: its debris sprites and its
// ricochet sounds ("sound/<name>"; a name too long for that is skipped). The server's download list.
void MAT_ForEachFxFile( const mat_fx_t *fx, mat_file_fn onFile, void *ctx );
// calls back with every impact effect of the registry (each letter's, each texture block's)
void MAT_ForEachFx( void ( *callback )( const mat_fx_t *fx, void *ctx ), void *ctx );

// strips the engine prefixes of a texture name ("-0", "+0~", "{", "!", "~", " ") into out (CBTEXTURENAMEMAX bytes)
void MAT_StripTextureName( const char *raw, char *out );
// material letter of a (stripped or raw) texture name; the default material if it has none
char MAT_FindTextureType( const char *name );
// the same lookup, also giving the entry that matched (CBTEXTURENAMEMAX bytes, "" if none) and its layer
char MAT_FindTextureMatch( const char *name, char *matched, int *layer );
int MAT_TextureCount( void );

const mat_type_t *MAT_GetType( char letter );		// NULL if not defined
const mat_step_t *MAT_GetStep( char letter );		// never NULL
const mat_hit_t *MAT_GetHit( char letter );			// never NULL
const mat_step_t *MAT_GetLadderStep( void );
const mat_step_t *MAT_GetWadeStep( void );
const mat_step_t *MAT_GetSloshStep( void );
// impact effects of a texture: its own texture block, else its letter's (never NULL; fx->defined 0: the 1.38 effect)
const mat_fx_t *MAT_GetFx( char letter, const char *strippedName );
char MAT_DefaultType( void );
char MAT_FleshType( void );
char MAT_SloshType( void );

// server: a type whose sounds were not precached for this map is played as the default type
void MAT_SetUsable( char letter, int usable );
int MAT_IsUsable( char letter );

// calls back with every texture name of a BSP (version 30) texture lump; -1 if the data is not a usable BSP
int MAT_ForEachBspTexture( const unsigned char *data, int length, void ( *callback )( const char *name, void *ctx ), void *ctx );
// a sound path from a data file: relative to sound/, [A-Za-z0-9_-./] only, no "..", ends in .wav
int MAT_IsSafeSoundPath( const char *path );
// the same rule for another extension (".spr")
int MAT_IsSafeAssetPath( const char *path, const char *extension );

#if defined( __GNUC__ ) && !defined( _WIN32 )
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif // !PM_MATERIALSH
