//=========================================================
// SBO 2.0 map compatibility file: see map_compat.h and docs/MAP_COMPAT.md.
//
// maps/map_compat.txt, one rule per line (// starts a comment line; tokens separated by spaces or tabs, "quoted"
// tokens may contain spaces):
//
//   <map> <crc32> drop <classname> <selector> <key>
//   <map> <crc32> set  <classname> <selector> <key> <value>
//
//   map        the map's name without maps/ and .bsp (case-insensitive)
//   crc32      CRC-32 of the whole .bsp file, 8 hex digits (python: zlib.crc32): the entry stops applying when the
//              author ships a changed map
//   selector   *            every entity of the class
//              #N           the entity at index N of the map's entity lump (0 = worldspawn)
//              name         every entity of the class with this targetname
//
// Everything here reads data from our own game folder, is bounded (line, token, rule and lump sizes) and never
// trusts the files: a malformed line is skipped with a console note, a malformed entity lump ends the scan.
// MAP_COMPAT_FUZZ builds only the parsers (tests/map_compat, ASan/UBSan).
//=========================================================

#ifndef MAP_COMPAT_FUZZ
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"map_compat.h"
#include	"game.h"	// mp_map_compat
#endif

#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>

#define MC_MAX_RULES		64
#define MC_MAX_LINE			512
#define MC_MAX_TOKEN		128
#define MC_MAX_TOKENS		8
#define MC_MAX_ORDINALS		64		// entities one rule can select by index or name
#define MC_MAX_LUMP_ENTS	8192	// entities scanned in a map's entity lump

enum { MC_DROP, MC_SET };
enum { MC_SEL_ALL, MC_SEL_INDEX, MC_SEL_NAME };

struct mc_rule_t
{
	char			map[ MC_MAX_TOKEN ];
	unsigned int	crc;
	int				action;
	char			classname[ MC_MAX_TOKEN ];
	int				selType;
	int				selIndex;
	char			selName[ MC_MAX_TOKEN ];
	char			key[ MC_MAX_TOKEN ];
	char			value[ MC_MAX_TOKEN ];

	// resolved for this map load
	int				ordinals[ MC_MAX_ORDINALS ];	// the selected entities: their number among the entities of the class
	int				numOrdinals;
	int				applied;						// entities changed (reported once)
};

static mc_rule_t s_Rules[ MC_MAX_RULES ];
static int s_iNumRules;

//---------------------------------------------------------
// CRC-32 (IEEE 802.3, as zlib.crc32)
//---------------------------------------------------------
static unsigned int MC_Crc32( const unsigned char *data, size_t len )
{
	static unsigned int table[256];
	static int init;

	if ( !init )
	{
		for ( unsigned int i = 0; i < 256; i++ )
		{
			unsigned int c = i;
			for ( int k = 0; k < 8; k++ )
				c = ( c & 1 ) ? 0xEDB88320u ^ ( c >> 1 ) : c >> 1;
			table[i] = c;
		}
		init = 1;
	}

	unsigned int crc = 0xFFFFFFFFu;
	for ( size_t i = 0; i < len; i++ )
		crc = table[ ( crc ^ data[i] ) & 0xFF ] ^ ( crc >> 8 );
	return crc ^ 0xFFFFFFFFu;
}

static int MC_StrCaseEq( const char *a, const char *b )
{
	for ( ; *a && *b; a++, b++ )
	{
		if ( tolower( (unsigned char)*a ) != tolower( (unsigned char)*b ) )
			return 0;
	}
	return *a == *b;
}

// Copies at most MC_MAX_TOKEN - 1 characters; returns 0 when the source is longer
static int MC_CopyToken( char *dst, const char *src, size_t len )
{
	if ( len >= MC_MAX_TOKEN )
		return 0;
	memcpy( dst, src, len );
	dst[len] = '\0';
	return 1;
}

//---------------------------------------------------------
// Rules file
//---------------------------------------------------------

// Splits one line (NUL-terminated, at most MC_MAX_LINE) into tokens. Returns the count, -1 if malformed.
static int MC_Tokenize( const char *line, char tokens[][ MC_MAX_TOKEN ], int maxTokens )
{
	int count = 0;
	const char *p = line;

	for ( ;; )
	{
		while ( *p == ' ' || *p == '\t' || *p == '\r' )
			p++;
		if ( !*p )
			break;
		if ( p[0] == '/' && p[1] == '/' )
			break;
		if ( count >= maxTokens )
			return -1;

		const char *start;
		size_t len;

		if ( *p == '"' )
		{
			start = ++p;
			while ( *p && *p != '"' )
				p++;
			if ( *p != '"' )
				return -1;	// unterminated quote
			len = (size_t)( p - start );
			p++;
		}
		else
		{
			start = p;
			while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '"' )
				p++;
			len = (size_t)( p - start );
		}

		if ( !MC_CopyToken( tokens[count], start, len ) )
			return -1;
		count++;
	}

	return count;
}

static int MC_ParseHex32( const char *s, unsigned int *out )
{
	unsigned int v = 0;
	int n = 0;

	for ( ; *s; s++, n++ )
	{
		int d;
		if ( *s >= '0' && *s <= '9' ) d = *s - '0';
		else if ( *s >= 'a' && *s <= 'f' ) d = *s - 'a' + 10;
		else if ( *s >= 'A' && *s <= 'F' ) d = *s - 'A' + 10;
		else return 0;
		if ( n >= 8 )
			return 0;
		v = ( v << 4 ) | (unsigned int)d;
	}

	if ( n != 8 )
		return 0;
	*out = v;
	return 1;
}

// One line into a rule. Returns 1 if it is a rule, 0 if empty / comment, -1 if malformed.
static int MC_ParseLine( const char *line, mc_rule_t *rule )
{
	char tokens[ MC_MAX_TOKENS ][ MC_MAX_TOKEN ];
	int n = MC_Tokenize( line, tokens, MC_MAX_TOKENS );

	if ( n == 0 )
		return 0;
	if ( n < 6 )
		return -1;

	memset( rule, 0, sizeof( *rule ) );

	strcpy( rule->map, tokens[0] );
	if ( !MC_ParseHex32( tokens[1], &rule->crc ) )
		return -1;

	if ( MC_StrCaseEq( tokens[2], "drop" ) && n == 6 )
		rule->action = MC_DROP;
	else if ( MC_StrCaseEq( tokens[2], "set" ) && n == 7 )
	{
		rule->action = MC_SET;
		strcpy( rule->value, tokens[6] );
	}
	else
		return -1;

	strcpy( rule->classname, tokens[3] );
	strcpy( rule->key, tokens[5] );

	const char *sel = tokens[4];
	if ( !strcmp( sel, "*" ) )
		rule->selType = MC_SEL_ALL;
	else if ( sel[0] == '#' )
	{
		const char *d = sel + 1;
		long v = 0;
		if ( !*d )
			return -1;
		for ( ; *d; d++ )
		{
			if ( *d < '0' || *d > '9' )
				return -1;
			v = v * 10 + ( *d - '0' );
			if ( v >= MC_MAX_LUMP_ENTS )
				return -1;
		}
		rule->selType = MC_SEL_INDEX;
		rule->selIndex = (int)v;
	}
	else
	{
		rule->selType = MC_SEL_NAME;
		strcpy( rule->selName, sel );
	}

	if ( !rule->classname[0] || !rule->key[0] || !rule->map[0] )
		return -1;

	return 1;
}

// Reads the rules of one map from the file's text (buf, len bytes, need not be NUL-terminated) into s_Rules.
// Returns the number of rules for the map (any CRC); malformed lines are counted in *pBad.
static int MC_ParseRules( const char *buf, size_t len, const char *mapname, int *pBad )
{
	char line[ MC_MAX_LINE ];
	size_t pos = 0;
	int bad = 0;

	s_iNumRules = 0;

	while ( pos < len )
	{
		size_t start = pos;
		while ( pos < len && buf[pos] != '\n' && buf[pos] != '\0' )
			pos++;
		size_t lineLen = pos - start;
		pos++;	// past the newline

		if ( lineLen >= MC_MAX_LINE )
		{
			bad++;
			continue;
		}
		memcpy( line, buf + start, lineLen );
		line[lineLen] = '\0';

		mc_rule_t rule;
		int r = MC_ParseLine( line, &rule );
		if ( r < 0 )
		{
			bad++;
			continue;
		}
		if ( r == 0 || !MC_StrCaseEq( rule.map, mapname ) )
			continue;
		if ( s_iNumRules >= MC_MAX_RULES )
		{
			bad++;
			continue;
		}
		s_Rules[ s_iNumRules++ ] = rule;
	}

	if ( pBad )
		*pBad = bad;
	return s_iNumRules;
}

//---------------------------------------------------------
// Entity lump: resolves "#N" and name selectors to the selected entities' numbers within their class
//---------------------------------------------------------

// Next quoted string or brace in the lump. Returns the token type ('{', '}', '"'), 0 at the end or on an error.
static int MC_LumpToken( const char *lump, size_t len, size_t *pos, char *out )
{
	size_t p = *pos;

	while ( p < len && ( lump[p] == ' ' || lump[p] == '\t' || lump[p] == '\r' || lump[p] == '\n' ) )
		p++;
	if ( p >= len || lump[p] == '\0' )
		return 0;

	char c = lump[p];
	if ( c == '{' || c == '}' )
	{
		*pos = p + 1;
		return c;
	}
	if ( c != '"' )
		return 0;

	size_t start = ++p;
	while ( p < len && lump[p] != '"' && lump[p] != '\0' )
		p++;
	if ( p >= len || lump[p] != '"' )
		return 0;

	size_t n = p - start;
	if ( n >= MC_MAX_TOKEN )
		n = MC_MAX_TOKEN - 1;	// long values (wads, messages) are cut; only names are compared
	memcpy( out, lump + start, n );
	out[n] = '\0';
	*pos = p + 1;
	return '"';
}

static void MC_ResolveSelectors( const char *lump, size_t len )
{
	char key[ MC_MAX_TOKEN ], value[ MC_MAX_TOKEN ];
	char classname[ MC_MAX_TOKEN ], targetname[ MC_MAX_TOKEN ];
	int classCount[ MC_MAX_RULES ];	// entities of rule i's class seen so far
	size_t pos = 0;

	memset( classCount, 0, sizeof( classCount ) );

	for ( int entIndex = 0; entIndex < MC_MAX_LUMP_ENTS; entIndex++ )
	{
		int t = MC_LumpToken( lump, len, &pos, key );
		if ( t != '{' )
			return;

		classname[0] = targetname[0] = '\0';

		for ( ;; )
		{
			t = MC_LumpToken( lump, len, &pos, key );
			if ( t == '}' )
				break;
			if ( t != '"' || MC_LumpToken( lump, len, &pos, value ) != '"' )
				return;	// malformed: stop

			if ( !strcmp( key, "classname" ) )
				strcpy( classname, value );
			else if ( !strcmp( key, "targetname" ) )
				strcpy( targetname, value );
		}

		for ( int i = 0; i < s_iNumRules; i++ )
		{
			mc_rule_t *r = &s_Rules[i];

			if ( strcmp( r->classname, classname ) )
				continue;

			int ordinal = classCount[i]++;
			int selected = 0;

			if ( r->selType == MC_SEL_INDEX )
				selected = r->selIndex == entIndex;
			else if ( r->selType == MC_SEL_NAME )
				selected = !strcmp( r->selName, targetname );

			if ( selected && r->numOrdinals < MC_MAX_ORDINALS )
				r->ordinals[ r->numOrdinals++ ] = ordinal;
		}
	}
}

// The entity lump of a BSP v30 file (header: version, 15 lumps of offset + length; lump 0 = entities)
static int MC_EntityLump( const unsigned char *bsp, size_t len, const char **lump, size_t *lumpLen )
{
	if ( len < 4 + 15 * 8 )
		return 0;

	int version = bsp[0] | ( bsp[1] << 8 ) | ( bsp[2] << 16 ) | ( (unsigned int)bsp[3] << 24 );
	if ( version != 30 )
		return 0;

	unsigned int ofs = bsp[4] | ( bsp[5] << 8 ) | ( bsp[6] << 16 ) | ( (unsigned int)bsp[7] << 24 );
	unsigned int n = bsp[8] | ( bsp[9] << 8 ) | ( bsp[10] << 16 ) | ( (unsigned int)bsp[11] << 24 );

	if ( ofs > len || n > len - ofs )
		return 0;

	*lump = (const char *)bsp + ofs;
	*lumpLen = n;
	return 1;
}

#ifndef MAP_COMPAT_FUZZ

//---------------------------------------------------------
// Engine side
//---------------------------------------------------------

static BOOL s_bMapActive;		// ServerActivate ran: nothing is filtered
// The entity whose keys come in. The engine parses the lump one entity at a time (keys, then DispatchSpawn), but a
// new entity may get the edict of the one before (removed in its Spawn: an unnamed light, an env_static), so the
// pointer alone doesn't tell a new entity: DispatchSpawn ends the current one, and a second "classname" key starts
// the next one (an entity the engine dropped without a Spawn).
static edict_t *s_pLastEnt;
static BOOL s_bCurEnded;		// DispatchSpawn ran for it: the next key belongs to a new entity
static BOOL s_bCurHadClassname;	// its "classname" key came
static int s_iCurOrdinal[ MC_MAX_RULES ];	// its number among the entities of rule i's class, -1 not that class
static int s_iClassCount[ MC_MAX_RULES ];
static BOOL s_bCurCounted;		// the current entity is counted

// numbers the current entity within the rules' classes (once)
static void MC_Count( const char *pszClassName )
{
	for ( int i = 0; i < s_iNumRules; i++ )
	{
		if ( !strcmp( s_Rules[i].classname, pszClassName ) )
			s_iCurOrdinal[i] = s_iClassCount[i]++;
		else
			s_iCurOrdinal[i] = -1;
	}
	s_bCurCounted = TRUE;
}

static void MC_BeginMap( void )
{
	s_iNumRules = 0;

	// mp_map_compat 0: no entries apply; the map loads its keys as written
	if ( map_compat.value == 0 )
		return;

	// the world's model is "maps/<name>.bsp", set by the engine before the map's keys are dispatched; else the
	// map name
	char mapname[ MC_MAX_TOKEN ];
	char model[ MC_MAX_TOKEN + 16 ];
	const char *worldModel = STRING( VARS( INDEXENT( 0 ) )->model );

	if ( worldModel && !strncmp( worldModel, "maps/", 5 ) )
	{
		const char *name = worldModel + 5;
		size_t n = strlen( name );
		if ( n < 5 || n >= sizeof( mapname ) || !MC_StrCaseEq( name + n - 4, ".bsp" ) )
			return;
		memcpy( mapname, name, n - 4 );
		mapname[ n - 4 ] = '\0';
	}
	else
	{
		const char *name = STRING( gpGlobals->mapname );
		size_t n = name ? strlen( name ) : 0;
		if ( !n || n >= sizeof( mapname ) )
			return;
		memcpy( mapname, name, n + 1 );
	}
	snprintf( model, sizeof( model ), "maps/%s.bsp", mapname );

	int fileLen = 0;
	byte *pFile = LOAD_FILE_FOR_ME( (char *)"maps/map_compat.txt", &fileLen );
	if ( !pFile )
		return;

	int bad = 0;
	int count = MC_ParseRules( (const char *)pFile, fileLen > 0 ? (size_t)fileLen : 0, mapname, &bad );
	FREE_FILE( pFile );

	if ( bad )
		ALERT( at_console, "maps/map_compat.txt: %d malformed line(s) ignored\n", bad );
	if ( !count )
		return;

	// the map's own checksum: only the version an entry was written for is changed
	int bspLen = 0;
	byte *pBsp = LOAD_FILE_FOR_ME( (char *)model, &bspLen );
	if ( !pBsp || bspLen <= 0 )
	{
		if ( pBsp )
			FREE_FILE( pBsp );
		s_iNumRules = 0;
		return;
	}

	unsigned int crc = MC_Crc32( pBsp, (size_t)bspLen );
	int kept = 0;
	for ( int i = 0; i < s_iNumRules; i++ )
	{
		if ( s_Rules[i].crc == crc )
			s_Rules[ kept++ ] = s_Rules[i];
	}
	if ( kept < s_iNumRules )
		ALERT( at_aiconsole, "map_compat: %d entr%s for %s skipped: written for another version of the map (crc %08x)\n",
			s_iNumRules - kept, s_iNumRules - kept == 1 ? "y" : "ies", mapname, crc );
	s_iNumRules = kept;

	BOOL bSelectors = FALSE;
	for ( int i = 0; i < s_iNumRules; i++ )
	{
		if ( s_Rules[i].selType != MC_SEL_ALL )
			bSelectors = TRUE;
	}

	const char *lump;
	size_t lumpLen;
	if ( bSelectors && MC_EntityLump( pBsp, (size_t)bspLen, &lump, &lumpLen ) )
		MC_ResolveSelectors( lump, lumpLen );

	FREE_FILE( pBsp );
}

static BOOL MC_Selected( int i )
{
	const mc_rule_t *r = &s_Rules[i];

	if ( s_iCurOrdinal[i] < 0 )
		return FALSE;
	if ( r->selType == MC_SEL_ALL )
		return TRUE;
	for ( int k = 0; k < r->numOrdinals; k++ )
	{
		if ( r->ordinals[k] == s_iCurOrdinal[i] )
			return TRUE;
	}
	return FALSE;
}

BOOL MapCompat_KeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
	if ( s_bMapActive || !pkvd || !pentKeyvalue || !pkvd->szKeyName )
		return FALSE;

	const BOOL bClassnameKey = !strcmp( pkvd->szKeyName, "classname" );

	if ( pentKeyvalue != s_pLastEnt || s_bCurEnded || ( bClassnameKey && s_bCurHadClassname ) )
	{
		// the world's keys come first: a new map
		if ( ENTINDEX( pentKeyvalue ) == 0 )
		{
			MC_BeginMap();
			memset( s_iClassCount, 0, sizeof( s_iClassCount ) );
		}

		s_pLastEnt = pentKeyvalue;
		s_bCurEnded = FALSE;
		s_bCurHadClassname = FALSE;
		s_bCurCounted = FALSE;
	}

	if ( bClassnameKey )
		s_bCurHadClassname = TRUE;

	if ( !s_iNumRules )
		return FALSE;

	// number this entity within the rules' classes once its class is known
	if ( !s_bCurCounted && pkvd->szClassName )
		MC_Count( pkvd->szClassName );

	if ( !s_bCurCounted )
		return FALSE;

	for ( int i = 0; i < s_iNumRules; i++ )
	{
		mc_rule_t *r = &s_Rules[i];

		if ( strcmp( r->key, pkvd->szKeyName ) || !MC_Selected( i ) )
			continue;

		r->applied++;
		if ( CVAR_GET_FLOAT( "developer" ) > 0 )
		{
			char szIndex[16];
			snprintf( szIndex, sizeof( szIndex ), "#%d", r->selIndex );
			ALERT( at_console, "map_compat: %s: %s %s %s %s%s%s: entity #%d, %s %d of the map\n", r->map,
				r->action == MC_DROP ? "drop" : "set", r->classname,
				r->selType == MC_SEL_ALL ? "*" : ( r->selType == MC_SEL_NAME ? r->selName : szIndex ), r->key,
				r->action == MC_SET ? " = " : "", r->action == MC_SET ? r->value : "", ENTINDEX( pentKeyvalue ),
				pkvd->szClassName ? pkvd->szClassName : "?", s_iCurOrdinal[i] );
		}

		if ( r->action == MC_DROP )
		{
			pkvd->fHandled = TRUE;
			return TRUE;
		}

		pkvd->szValue = r->value;	// MC_SET: the entry's value replaces the map's
	}

	return FALSE;
}

void MapCompat_Spawn( edict_t *pent )
{
	// (the keys alone number the entities: every lump entity has its "classname" key; an entity created
	// by another one's Spawn has no keys and takes no number)
	if ( s_bMapActive || !pent )
		return;

	s_bCurEnded = TRUE;
}

void MapCompat_MapLoaded( void )
{
	if ( s_iNumRules && CVAR_GET_FLOAT( "developer" ) > 0 )
	{
		for ( int i = 0; i < s_iNumRules; i++ )
			ALERT( at_console, "map_compat: %s %s %s: %d entities\n", s_Rules[i].action == MC_DROP ? "drop" : "set",
				s_Rules[i].classname, s_Rules[i].key, s_Rules[i].applied );
	}

	s_bMapActive = TRUE;
	s_pLastEnt = NULL;
	s_iNumRules = 0;
}

void MapCompat_MapUnloaded( void )
{
	s_bMapActive = FALSE;
	s_pLastEnt = NULL;
	s_iNumRules = 0;
}

#endif // MAP_COMPAT_FUZZ
