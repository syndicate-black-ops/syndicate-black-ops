//=========================================================
// SBO 2.0 test layer (SBO_TEST_COMMANDS builds only; docs/dev/ENTITY_TESTS.md "Test commands").
//
// Drives a server for the automated entity tests and records what the game does as a JSON-lines trace:
//   - test_* server commands (console, rcon, step files run with test_run; from a player with sv_cheats 1)
//   - fake players (the engine's CreateFakeClient, moved with RunPlayerMove every frame)
//   - a trace of target firing (FireTargets), entities spawned / removed, sounds, messages to clients, precaches
//   - a seeded random generator in place of the engine's RandomLong / RandomFloat (test_seed)
// The engine functions are wrapped in the game's own copy of the function table (GiveFnptrsToDll), so this works on
// every engine: stock HLDS, HL25 listen servers. Nothing here runs unless a test command is used.
//=========================================================

// the standard library first: extdll.h defines min / max as macros
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "player.h"
#include "client.h"
#include "gamerules.h"
#include "coop.h"
#include "in_buttons.h"
#include "sbo_test.h"


#ifndef _WIN32
#define TEST_USED __attribute__((used))
#else
#define TEST_USED
#endif

// Marker the release check looks for (TOOLING-NOTES.md "Release workflow"): a release build must not contain it.
extern "C" TEST_USED const char g_szSboTestCommands[] = "SBO_TEST_COMMANDS test layer";

extern int m_Mode;

typedef std::vector<std::string> Args;

// ------------------------------------------------------------------------------------------ state
static enginefuncs_t s_engine;			// the engine's functions (the wrappers call these)
static FILE *s_pTrace;
static unsigned long s_iFrame;
static double s_flWall0;

static std::vector<std::string> s_steps;	// test_run
static size_t s_iStep;
static float s_flWaitUntil;
static unsigned long s_iWaitFrames;
static std::string s_szStepFile;
static Args s_waitFor;					// wait_for: the condition
static float s_flWaitForEnd;

static bool s_bSeeded;
static unsigned int s_iRand;

static bool s_bFakeCmd;					// ClientCommand from the test layer: Cmd_Argv returns s_fakeArgs
static Args s_fakeArgs;
static std::string s_szFakeArgs;

static std::map<int, std::string> s_userMsgs;	// RegUserMsg: id -> name
static std::set<std::string> s_precModels, s_precSounds, s_precGeneric;

// a message being written (MessageBegin .. MessageEnd)
static bool s_bInMsg;
static int s_iMsgDest, s_iMsgType, s_iMsgTo;
static float s_vecMsgOrigin[3];
static bool s_bMsgOrigin;
static std::vector<unsigned char> s_msg;

struct FakePlayer
{
	edict_t *ed;
	float angles[3];
	float fwd, side, up;
	unsigned short buttons;
	long frames;			// input held for this many frames (-1: until changed)
};
static std::vector<FakePlayer> s_fakes;

// spawn / remove scan
static int s_serial[ 4096 ];
static bool s_used[ 4096 ];
static bool s_bScanBase;

static std::set<std::string> s_firedNames;	// names fired since the current wait_for began

// ------------------------------------------------------------------------------------------ helpers
static double WallTime( void )
{
#ifdef _WIN32
	return (double)clock() / CLOCKS_PER_SEC;
#else
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

static void Print( const char *fmt, ... )
{
	char buf[ 1024 ];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	s_engine.pfnServerPrint( buf );
}

static std::string Json( const char *s )
{
	std::string out = "\"";
	for ( ; s && *s; s++ )
	{
		unsigned char c = (unsigned char)*s;
		if ( c == '"' || c == '\\' ) { out += '\\'; out += (char)c; }
		else if ( c == '\n' ) out += "\\n";
		else if ( c == '\r' ) out += "\\r";
		else if ( c == '\t' ) out += "\\t";
		else if ( c < 0x20 || c >= 0x7f ) { char b[ 8 ]; snprintf( b, sizeof( b ), "\\u%04x", c ); out += b; }
		else out += (char)c;
	}
	return out + "\"";
}

static std::string Fmt( const char *fmt, ... )
{
	char buf[ 2048 ];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	return buf;
}

static std::string EntRef( edict_t *e )
{
	if ( !e )
		return "null";
	int i = s_engine.pfnIndexOfEdict( e );
	return Fmt( "{\"i\":%d,\"c\":%s,\"n\":%s}", i, Json( e->free ? "" : STRING( e->v.classname ) ).c_str(),
		Json( e->free ? "" : STRING( e->v.targetname ) ).c_str() );
}

static std::string EntRef( CBaseEntity *p )
{
	return p ? EntRef( p->edict() ) : std::string( "null" );
}

// one trace line: {"t":time,"dt":frame time,"f":frame,"ev":ev,<body>}
static void Trace( const char *ev, const std::string &body )
{
	if ( !s_pTrace )
		return;
	// dt: this frame's length (the checks widen their time windows by it: an event is seen at frame granularity)
	fprintf( s_pTrace, "{\"t\":%.4f,\"dt\":%.4f,\"f\":%lu,\"ev\":\"%s\"%s%s}\n", gpGlobals ? gpGlobals->time : 0.0f,
		gpGlobals ? gpGlobals->frametime : 0.0f, s_iFrame, ev, body.empty() ? "" : ",", body.c_str() );
}

static Args Tokenize( const char *s )
{
	Args out;
	while ( *s )
	{
		while ( *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' )
			s++;
		if ( !*s || ( s[0] == '/' && s[1] == '/' ) || *s == '#' )
			break;
		std::string tok;
		if ( *s == '"' )
		{
			s++;
			while ( *s && *s != '"' )
				tok += *s++;
			if ( *s == '"' )
				s++;
		}
		else
		{
			while ( *s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n' )
				tok += *s++;
		}
		out.push_back( tok );
	}
	return out;
}

static std::string Join( const Args &a, size_t from )
{
	std::string s;
	for ( size_t i = from; i < a.size(); i++ )
	{
		if ( i > from )
			s += ' ';
		s += a[i];
	}
	return s;
}

// Entity references: #<index>, player<n> (client slot n), @<classname> / @<classname>[n], world, a targetname,
// or a player's name.
static std::vector<edict_t *> Resolve( const std::string &ref, edict_t *pSelf = NULL )
{
	std::vector<edict_t *> out;
	if ( ref.empty() )
		return out;
	if ( ref == "self" )
	{
		if ( pSelf )
			out.push_back( pSelf );
		return out;
	}
	if ( ref == "world" )
	{
		out.push_back( s_engine.pfnPEntityOfEntIndex( 0 ) );
		return out;
	}
	if ( ref[0] == '#' )
	{
		edict_t *e = s_engine.pfnPEntityOfEntIndex( atoi( ref.c_str() + 1 ) );
		if ( e && !e->free )
			out.push_back( e );
		return out;
	}
	if ( !strncmp( ref.c_str(), "player", 6 ) && ref.size() > 6 && isdigit( (unsigned char)ref[6] ) )
	{
		CBaseEntity *p = UTIL_PlayerByIndex( atoi( ref.c_str() + 6 ) );
		if ( p )
			out.push_back( p->edict() );
		return out;
	}
	if ( ref[0] == '@' )
	{
		std::string cls = ref.substr( 1 );
		int n = -1;
		size_t b = cls.find( '[' );
		if ( b != std::string::npos )
		{
			n = atoi( cls.c_str() + b + 1 );
			cls = cls.substr( 0, b );
		}
		edict_t *e = NULL;
		int k = 0;
		while ( !FNullEnt( e = FIND_ENTITY_BY_CLASSNAME( e, cls.c_str() ) ) )
		{
			if ( n < 0 || k == n )
				out.push_back( e );
			k++;
		}
		return out;
	}
	edict_t *e = NULL;
	while ( !FNullEnt( e = FIND_ENTITY_BY_TARGETNAME( e, ref.c_str() ) ) )
		out.push_back( e );
	if ( out.empty() )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *p = UTIL_PlayerByIndex( i );
			if ( p && !strcmp( STRING( p->pev->netname ), ref.c_str() ) )
				out.push_back( p->edict() );
		}
	}
	return out;
}

static edict_t *ResolveOne( const std::string &ref, edict_t *pSelf, const char *pszCmd )
{
	std::vector<edict_t *> v = Resolve( ref, pSelf );
	if ( v.empty() )
	{
		Print( "%s: no entity \"%s\"\n", pszCmd, ref.c_str() );
		Trace( "error", Fmt( "\"cmd\":%s,\"msg\":%s", Json( pszCmd ).c_str(), Json( ( "no entity " + ref ).c_str() ).c_str() ) );
		return NULL;
	}
	return v[0];
}

// A player for the commands that act as a client (test_join, test_cmd, test_userinfo): a player's name wins over a
// map's targetname of the same name, and any other reference must resolve to a player (a client command sent to a
// path_corner crashes the server).
static edict_t *ResolvePlayer( const std::string &ref, edict_t *pSelf, const char *pszCmd )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *p = UTIL_PlayerByIndex( i );
		if ( p && ( p->pev->flags & FL_CLIENT ) && !strcmp( STRING( p->pev->netname ), ref.c_str() ) )
			return p->edict();
	}
	std::vector<edict_t *> v = Resolve( ref, pSelf );
	for ( size_t i = 0; i < v.size(); i++ )
	{
		const int idx = ENTINDEX( v[i] );
		if ( idx >= 1 && idx <= gpGlobals->maxClients && ( v[i]->v.flags & FL_CLIENT ) && UTIL_PlayerByIndex( idx ) )
			return v[i];
	}
	Print( "%s: no player \"%s\"\n", pszCmd, ref.c_str() );
	Trace( "error", Fmt( "\"cmd\":%s,\"msg\":%s", Json( pszCmd ).c_str(), Json( ( "no player " + ref ).c_str() ).c_str() ) );
	return NULL;
}

static FakePlayer *FindFake( edict_t *e )
{
	for ( size_t i = 0; i < s_fakes.size(); i++ )
		if ( s_fakes[i].ed == e )
			return &s_fakes[i];
	return NULL;
}

// ------------------------------------------------------------------------------------------ snapshot fields
enum { F_FLOAT, F_INT, F_VEC, F_STR };
struct Field { const char *name; int offset; int type; };
#define FLD( n, t ) { #n, (int)offsetof( entvars_t, n ), t }
static const Field s_fields[] =
{
	FLD( classname, F_STR ), FLD( targetname, F_STR ), FLD( target, F_STR ), FLD( netname, F_STR ),
	FLD( model, F_STR ), FLD( message, F_STR ), FLD( noise, F_STR ),
	FLD( origin, F_VEC ), FLD( angles, F_VEC ), FLD( velocity, F_VEC ), FLD( avelocity, F_VEC ),
	FLD( mins, F_VEC ), FLD( maxs, F_VEC ), FLD( absmin, F_VEC ), FLD( absmax, F_VEC ), FLD( v_angle, F_VEC ),
	FLD( rendercolor, F_VEC ), FLD( movedir, F_VEC ), FLD( startpos, F_VEC ), FLD( endpos, F_VEC ),
	FLD( punchangle, F_VEC ),
	FLD( health, F_FLOAT ), FLD( max_health, F_FLOAT ), FLD( armorvalue, F_FLOAT ), FLD( frags, F_FLOAT ),
	FLD( takedamage, F_FLOAT ), FLD( renderamt, F_FLOAT ), FLD( scale, F_FLOAT ), FLD( frame, F_FLOAT ),
	FLD( speed, F_FLOAT ), FLD( dmg, F_FLOAT ), FLD( nextthink, F_FLOAT ), FLD( ltime, F_FLOAT ),
	FLD( gravity, F_FLOAT ), FLD( friction, F_FLOAT ), FLD( framerate, F_FLOAT ), FLD( fov, F_FLOAT ),
	FLD( fuser1, F_FLOAT ), FLD( fuser2, F_FLOAT ), FLD( fuser3, F_FLOAT ), FLD( fuser4, F_FLOAT ),
	FLD( solid, F_INT ), FLD( movetype, F_INT ), FLD( effects, F_INT ), FLD( rendermode, F_INT ),
	FLD( renderfx, F_INT ), FLD( sequence, F_INT ), FLD( skin, F_INT ), FLD( body, F_INT ),
	FLD( spawnflags, F_INT ), FLD( flags, F_INT ), FLD( deadflag, F_INT ), FLD( button, F_INT ),
	FLD( team, F_INT ), FLD( playerclass, F_INT ), FLD( weapons, F_INT ), FLD( impulse, F_INT ),
	FLD( iuser1, F_INT ), FLD( iuser2, F_INT ), FLD( iuser3, F_INT ), FLD( iuser4, F_INT ),
	FLD( modelindex, F_INT ), FLD( waterlevel, F_INT ), FLD( maxspeed, F_FLOAT ), FLD( colormap, F_INT ),
};

static std::string FieldJson( edict_t *e, const Field &f )
{
	const char *base = (const char *)&e->v;
	switch ( f.type )
	{
	case F_FLOAT: return Fmt( "%g", *(const float *)( base + f.offset ) );
	case F_INT: return Fmt( "%d", *(const int *)( base + f.offset ) );
	case F_VEC: { const float *v = (const float *)( base + f.offset ); return Fmt( "[%g,%g,%g]", v[0], v[1], v[2] ); }
	default: return Json( STRING( *(const string_t *)( base + f.offset ) ) );
	}
}

static std::string Snapshot( edict_t *e, const Args &fields, size_t from )
{
	std::string s = "\"ent\":" + EntRef( e ) + ",\"v\":{";
	bool first = true;
	for ( size_t k = 0; k < sizeof( s_fields ) / sizeof( s_fields[0] ); k++ )
	{
		const Field &f = s_fields[k];
		if ( fields.size() > from )	// fields named: only those (kv:<key> below)
		{
			bool want = false;
			for ( size_t j = from; j < fields.size(); j++ )
				if ( fields[j] == f.name )
					want = true;
			if ( !want )
				continue;
		}
		s += Fmt( "%s\"%s\":", first ? "" : ",", f.name ) + FieldJson( e, f );
		first = false;
	}
	CBaseEntity *p = CBaseEntity::Instance( e );
	// kv:<key>: the key's current value as the entity answers it (CBaseEntity::GetKeyValue: entvars fields, custom
	// keyvalues, a class's own keys such as game_timer's)
	for ( size_t j = from; p && j < fields.size(); j++ )
	{
		if ( strncmp( fields[j].c_str(), "kv:", 3 ) )
			continue;
		CKeyValue kv = p->GetKeyValue( fields[j].c_str() + 3 );
		std::string val;
		switch ( kv.keyType )
		{
		case KEY_TYPE_INT: case KEY_TYPE_EHANDLE: val = Fmt( "%d", kv.iVal ); break;
		case KEY_TYPE_FLOAT: val = Fmt( "%g", kv.fVal ); break;
		case KEY_TYPE_VECTOR: val = Fmt( "[%g,%g,%g]", kv.vVal[0], kv.vVal[1], kv.vVal[2] ); break;
		case KEY_TYPE_STRING: val = Json( STRING( kv.sVal ) ); break;
		default: val = "null"; break;
		}
		s += Fmt( "%s%s:", first ? "" : ",", Json( fields[j].c_str() ).c_str() ) + val;
		first = false;
	}
	// physinfo:<key>: a player's physics key value as the engine holds it (pfnGetPhysicsKeyValue; sent to his client)
	for ( size_t j = from; p && p->IsPlayer() && j < fields.size(); j++ )
	{
		if ( strncmp( fields[j].c_str(), "physinfo:", 9 ) )
			continue;
		const char *pszValue = g_engfuncs.pfnGetPhysicsKeyValue( e, fields[j].c_str() + 9 );
		s += Fmt( "%s%s:", first ? "" : ",", Json( fields[j].c_str() ).c_str() ) + Json( pszValue ? pszValue : "" );
		first = false;
	}
	if ( p && p->IsPlayer() )
	{
		CBasePlayer *pl = (CBasePlayer *)p;
		s += Fmt( "%s\"player\":{\"spectating\":%d,\"out_of_play\":%d,\"loadout_ready\":%d,\"alive\":%d}", first ? "" : ",",
			pl->m_bSpectating ? 1 : 0, pl->m_bOutOfPlay ? 1 : 0, pl->m_bLoadoutReady ? 1 : 0, pl->IsAlive() ? 1 : 0 );
		first = false;
		// a player's tank, only when named: tank (1: m_pTank set, the func_tank he controls)
		for ( size_t j = from; j < fields.size(); j++ )
			if ( fields[j] == "tank" )
				s += Fmt( ",\"tank\":%d", (CBaseEntity *)pl->m_pTank != NULL ? 1 : 0 );
		// SBO's co-op level fields, only when named: level ((m_iSboSpeed - 300) / 5, the scoreboard's), sbo_maxspeed
		// (m_iSboMaxSpeed), sbo_health (m_iSboHealth, the spawn health), total_xp (m_iSboTotalXP)
		for ( size_t j = from; j < fields.size(); j++ )
		{
			if ( fields[j] == "level" )
				s += Fmt( ",\"level\":%d", ( pl->m_iSboSpeed - 300 ) / 5 );
			else if ( fields[j] == "sbo_maxspeed" )
				s += Fmt( ",\"sbo_maxspeed\":%d", pl->m_iSboMaxSpeed );
			else if ( fields[j] == "sbo_health" )
				s += Fmt( ",\"sbo_health\":%d", pl->m_iSboHealth );
			else if ( fields[j] == "total_xp" )
				s += Fmt( ",\"total_xp\":%d", pl->m_iSboTotalXP );
			else if ( fields[j] == "game_master" )
				s += Fmt( ",\"game_master\":%d", pl->m_bGameMaster ? 1 : 0 );
		}
	}
	// a monster's AI state, only when named: enemy (m_hEnemy's classname, "" for none), state (m_MonsterState),
	// schedule (its name), damage_cond (1: bits_COND_LIGHT_DAMAGE or HEAVY_DAMAGE set), gun (GetGunPosition),
	// gun_dist (gun to origin), gun_muzzle (gun to attachment 0)
	CBaseMonster *pMonster = ( p && !p->IsPlayer() ) ? p->MyMonsterPointer() : NULL;
	for ( size_t j = from; pMonster && j < fields.size(); j++ )
	{
		std::string val;
		if ( fields[j] == "enemy" )
			val = Json( pMonster->m_hEnemy != NULL ? STRING( pMonster->m_hEnemy->pev->classname ) : "" );
		else if ( fields[j] == "damage_cond" )
			val = Fmt( "%d", pMonster->HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) ? 1 : 0 );
		else if ( fields[j] == "state" )
			val = Fmt( "%d", (int)pMonster->m_MonsterState );
		else if ( fields[j] == "schedule" )
			val = Json( pMonster->m_pSchedule ? pMonster->m_pSchedule->pName : "" );
		else if ( fields[j] == "rel" )		// IRelationship toward player 1 (R_AL -2 .. R_NM 3)
			val = Fmt( "%d", UTIL_PlayerByIndex( 1 ) ? pMonster->IRelationship( UTIL_PlayerByIndex( 1 ) ) : 0 );
		else if ( fields[j] == "fov" )		// m_flFieldOfView
			val = Fmt( "%g", pMonster->m_flFieldOfView );
		else if ( fields[j] == "soundmask" )	// the sound types it listens for (SoundMaskWithCustom)
			val = Fmt( "%d", pMonster->SoundMaskWithCustom() );
		else if ( fields[j] == "follow" )	// 1: following a player (m_hTargetEnt is a player: CTalkMonster::IsFollowing)
			val = Fmt( "%d", ( pMonster->m_hTargetEnt != NULL && pMonster->m_hTargetEnt->IsPlayer() ) ? 1 : 0 );
		else if ( fields[j] == "cine" )		// 1: linked to a scripted_sequence (m_pCine)
			val = Fmt( "%d", pMonster->m_pCine ? 1 : 0 );
		else if ( fields[j] == "squad" )	// SquadCount: members of its squad, the leader included (0: no squad)
			val = Fmt( "%d", pMonster->MySquadMonsterPointer() ? pMonster->MySquadMonsterPointer()->SquadCount() : 0 );
		else if ( fields[j] == "gun" || fields[j] == "gun_dist" || fields[j] == "gun_muzzle" )
		{
			Vector vecGun = pMonster->GetGunPosition();
			Vector vecMuzzle, vecAngles;
			pMonster->GetAttachment( 0, vecMuzzle, vecAngles );
			if ( fields[j] == "gun" )
				val = Fmt( "[%g,%g,%g]", vecGun.x, vecGun.y, vecGun.z );
			else
				val = Fmt( "%g", ( vecGun - ( fields[j] == "gun_dist" ? pMonster->pev->origin : vecMuzzle ) ).Length() );
		}
		else
			continue;
		s += Fmt( "%s\"%s\":", first ? "" : ",", fields[j].c_str() ) + val;
		first = false;
	}
	return s + "}";
}

// ------------------------------------------------------------------------------------------ engine wrappers
extern char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];

static void W_EmitSound( edict_t *e, int chan, const char *sample, float vol, float attn, int flags, int pitch )
{
	if ( s_pTrace )
	{
		// a sentence goes out by its number ("!123"): its name too ("sentence", e.g. "SC_POK1")
		std::string sentence;
		if ( sample && sample[0] == '!' && sample[1] >= '0' && sample[1] <= '9' )
		{
			int i = atoi( sample + 1 );
			if ( i >= 0 && i < CVOXFILESENTENCEMAX )
				sentence = Fmt( ",\"sentence\":%s", Json( gszallsentencenames[i] ).c_str() );
		}
		Trace( "sound", Fmt( "\"ent\":%s,\"chan\":%d,\"sample\":%s,\"vol\":%g,\"attn\":%g,\"flags\":%d,\"pitch\":%d%s",
			EntRef( e ).c_str(), chan, Json( sample ).c_str(), vol, attn, flags, pitch, sentence.c_str() ) );
	}
	s_engine.pfnEmitSound( e, chan, sample, vol, attn, flags, pitch );
}

static void W_EmitAmbientSound( edict_t *e, float *pos, const char *sample, float vol, float attn, int flags, int pitch )
{
	if ( s_pTrace )
		Trace( "ambient", Fmt( "\"ent\":%s,\"pos\":[%g,%g,%g],\"sample\":%s,\"vol\":%g,\"attn\":%g,\"flags\":%d,\"pitch\":%d",
			EntRef( e ).c_str(), pos[0], pos[1], pos[2], Json( sample ).c_str(), vol, attn, flags, pitch ) );
	s_engine.pfnEmitAmbientSound( e, pos, sample, vol, attn, flags, pitch );
}

// the view a client sees from (SET_VIEW: trigger_camera, ResetViewEntity)
static void W_SetView( const edict_t *pClient, const edict_t *pViewent )
{
	if ( s_pTrace )
		Trace( "setview", Fmt( "\"ent\":%s,\"view\":%s", EntRef( (edict_t *)pClient ).c_str(), EntRef( (edict_t *)pViewent ).c_str() ) );
	s_engine.pfnSetView( pClient, pViewent );
}

// console commands the game sends a client (CLIENT_COMMAND: "cd play", ...)
static void W_ClientCommand( edict_t *pEdict, char *szFmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, szFmt );
	vsnprintf( buf, sizeof( buf ), szFmt, ap );
	va_end( ap );
	if ( s_pTrace )
		Trace( "clcmd", Fmt( "\"ent\":%s,\"cmd\":%s", EntRef( pEdict ).c_str(), Json( buf ).c_str() ) );
	s_engine.pfnClientCommand( pEdict, (char *)"%s", buf );
}

// events (PLAYBACK_EVENT_FULL) by their precached name
static std::map<unsigned short, std::string> s_eventNames;

static unsigned short W_PrecacheEvent( int type, const char *psz )
{
	unsigned short idx = s_engine.pfnPrecacheEvent( type, psz );
	if ( psz )
		s_eventNames[idx] = psz;
	return idx;
}

static void W_PlaybackEvent( int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	if ( s_pTrace )
	{
		std::map<unsigned short, std::string>::const_iterator it = s_eventNames.find( eventindex );
		Trace( "event", Fmt( "\"ent\":%s,\"name\":%s,\"flags\":%d,\"fparam1\":%g,\"fparam2\":%g,\"iparam1\":%d,"
			"\"iparam2\":%d,\"bparam1\":%d,\"bparam2\":%d", EntRef( (edict_t *)pInvoker ).c_str(),
			Json( it != s_eventNames.end() ? it->second.c_str() : "" ).c_str(), flags, fparam1, fparam2, iparam1, iparam2,
			bparam1, bparam2 ) );
	}
	s_engine.pfnPlaybackEvent( flags, pInvoker, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2,
		bparam1, bparam2 );
}

static void W_MessageBegin( int dest, int type, const float *origin, edict_t *ed )
{
	if ( s_bInMsg && s_pTrace )
		Trace( "engine_error", Fmt( "\"msg\":\"MessageBegin inside a message (type %d, new %d)\"", s_iMsgType, type ) );
	s_bInMsg = true;
	s_iMsgDest = dest;
	s_iMsgType = type;
	s_iMsgTo = ed ? s_engine.pfnIndexOfEdict( ed ) : 0;
	s_bMsgOrigin = origin != NULL;
	if ( origin )
		memcpy( s_vecMsgOrigin, origin, sizeof( s_vecMsgOrigin ) );
	s_msg.clear();
	s_engine.pfnMessageBegin( dest, type, origin, ed );
}

static void W_MessageEnd( void )
{
	if ( s_pTrace )
	{
		std::string hex;
		for ( size_t i = 0; i < s_msg.size(); i++ )
			hex += Fmt( "%02x", s_msg[i] );
		std::map<int, std::string>::const_iterator it = s_userMsgs.find( s_iMsgType );
		Trace( "msg", Fmt( "\"dest\":%d,\"type\":%d,\"name\":%s,\"to\":%d,\"size\":%d,\"hex\":\"%s\"", s_iMsgDest, s_iMsgType,
			Json( it != s_userMsgs.end() ? it->second.c_str() : "" ).c_str(), s_iMsgTo, (int)s_msg.size(), hex.c_str() ) );
	}
	s_bInMsg = false;
	s_engine.pfnMessageEnd();
}

static void Put( int v, int n ) { for ( int i = 0; i < n; i++ ) s_msg.push_back( (unsigned char)( v >> ( 8 * i ) ) ); }
static void W_WriteByte( int v ) { Put( v, 1 ); s_engine.pfnWriteByte( v ); }
static void W_WriteChar( int v ) { Put( v, 1 ); s_engine.pfnWriteChar( v ); }
static void W_WriteShort( int v ) { Put( v, 2 ); s_engine.pfnWriteShort( v ); }
static void W_WriteLong( int v ) { Put( v, 4 ); s_engine.pfnWriteLong( v ); }
static void W_WriteAngle( float v ) { Put( (int)( v * 256 / 360 ) & 255, 1 ); s_engine.pfnWriteAngle( v ); }
static void W_WriteCoord( float v ) { Put( (int)( v * 8 ), 2 ); s_engine.pfnWriteCoord( v ); }
static void W_WriteEntity( int v ) { Put( v, 2 ); s_engine.pfnWriteEntity( v ); }
static void W_WriteString( const char *s )
{
	for ( const char *p = s ? s : ""; *p; p++ )
		s_msg.push_back( (unsigned char)*p );
	s_msg.push_back( 0 );
	s_engine.pfnWriteString( s );
}

static int W_RegUserMsg( const char *name, int size )
{
	int id = s_engine.pfnRegUserMsg( name, size );
	if ( id > 0 )
		s_userMsgs[ id ] = name;
	return id;
}

// test_precache_first: models precached before the first precache of the next map load (what a Metamod or AMXX
// plugin's precache before the game's does to the model table: every index after them moves up)
static std::vector<std::string> s_precacheFirst;

static int W_PrecacheModel( char *s );

static void PrecacheFirst( void )
{
	// the engine keeps the name's pointer for the map's life: names that are never freed
	static std::set<std::string> s_names;
	std::vector<std::string> names;
	names.swap( s_precacheFirst );
	for ( size_t i = 0; i < names.size(); i++ )
		W_PrecacheModel( (char *)s_names.insert( names[i] ).first->c_str() );
}

static int W_PrecacheModel( char *s )
{
	if ( s_precModels.empty() && !s_precacheFirst.empty() )
		PrecacheFirst();
	bool first = s && s_precModels.insert( s ).second;
	int index = s_engine.pfnPrecacheModel( s );
	// with the engine's index: what a temp entity's model index must be on this map (expect.msg 'model:<name>')
	if ( first && s_pTrace )
		Trace( "precache", Fmt( "\"kind\":\"model\",\"name\":%s,\"index\":%d", Json( s ).c_str(), index ) );
	return index;
}

static int W_PrecacheSound( char *s )
{
	if ( s && s_precSounds.insert( s ).second && s_pTrace )
		Trace( "precache", Fmt( "\"kind\":\"sound\",\"name\":%s", Json( s ).c_str() ) );
	return s_engine.pfnPrecacheSound( s );
}

static int W_PrecacheGeneric( char *s )
{
	if ( s && s_precGeneric.insert( s ).second && s_pTrace )
		Trace( "precache", Fmt( "\"kind\":\"generic\",\"name\":%s", Json( s ).c_str() ) );
	return s_engine.pfnPrecacheGeneric( s );
}

// xorshift32: the same sequence on every engine and platform for one seed
static unsigned int NextRand( void )
{
	s_iRand ^= s_iRand << 13;
	s_iRand ^= s_iRand >> 17;
	s_iRand ^= s_iRand << 5;
	return s_iRand;
}

static long W_RandomLong( long lo, long hi )
{
	if ( !s_bSeeded )
		return s_engine.pfnRandomLong( lo, hi );
	if ( hi <= lo )
		return lo;
	unsigned long range = (unsigned long)( hi - lo ) + 1;
	return lo + (long)( NextRand() % range );
}

static float W_RandomFloat( float lo, float hi )
{
	if ( !s_bSeeded )
		return s_engine.pfnRandomFloat( lo, hi );
	return lo + ( hi - lo ) * ( NextRand() / 4294967296.0f );
}

static const char *W_Cmd_Argv( int i )
{
	if ( !s_bFakeCmd )
		return s_engine.pfnCmd_Argv( i );
	return i >= 0 && i < (int)s_fakeArgs.size() ? s_fakeArgs[i].c_str() : "";
}

static int W_Cmd_Argc( void )
{
	return s_bFakeCmd ? (int)s_fakeArgs.size() : s_engine.pfnCmd_Argc();
}

static const char *W_Cmd_Args( void )
{
	return s_bFakeCmd ? s_szFakeArgs.c_str() : s_engine.pfnCmd_Args();
}

void Test_HookEngine( enginefuncs_t *pFuncs )
{
	s_engine = *pFuncs;
	pFuncs->pfnEmitSound = W_EmitSound;
	pFuncs->pfnEmitAmbientSound = W_EmitAmbientSound;
	pFuncs->pfnSetView = W_SetView;
	pFuncs->pfnClientCommand = W_ClientCommand;
	pFuncs->pfnPrecacheEvent = W_PrecacheEvent;
	pFuncs->pfnPlaybackEvent = W_PlaybackEvent;
	pFuncs->pfnMessageBegin = W_MessageBegin;
	pFuncs->pfnMessageEnd = W_MessageEnd;
	pFuncs->pfnWriteByte = W_WriteByte;
	pFuncs->pfnWriteChar = W_WriteChar;
	pFuncs->pfnWriteShort = W_WriteShort;
	pFuncs->pfnWriteLong = W_WriteLong;
	pFuncs->pfnWriteAngle = W_WriteAngle;
	pFuncs->pfnWriteCoord = W_WriteCoord;
	pFuncs->pfnWriteString = W_WriteString;
	pFuncs->pfnWriteEntity = W_WriteEntity;
	pFuncs->pfnRegUserMsg = W_RegUserMsg;
	pFuncs->pfnPrecacheModel = W_PrecacheModel;
	pFuncs->pfnPrecacheSound = W_PrecacheSound;
	pFuncs->pfnPrecacheGeneric = W_PrecacheGeneric;
	pFuncs->pfnRandomLong = W_RandomLong;
	pFuncs->pfnRandomFloat = W_RandomFloat;
	pFuncs->pfnCmd_Argv = W_Cmd_Argv;
	pFuncs->pfnCmd_Argc = W_Cmd_Argc;
	pFuncs->pfnCmd_Args = W_Cmd_Args;
}

// a client command as if the player had typed it (the join menus' commands)
static void FakeClientCommand( edict_t *e, const Args &argv )
{
	s_fakeArgs = argv;
	s_szFakeArgs = Join( argv, 1 );
	s_bFakeCmd = true;
	ClientCommand( e );
	s_bFakeCmd = false;
	if ( s_pTrace )
		Trace( "client_cmd", Fmt( "\"ent\":%s,\"cmd\":%s", EntRef( e ).c_str(), Json( Join( argv, 0 ).c_str() ).c_str() ) );
}

// ------------------------------------------------------------------------------------------ trace hooks
void Test_TraceFire( const char *pszTarget, CBaseEntity *pActivator, CBaseEntity *pCaller, int useType, float value )
{
	if ( pszTarget )
		s_firedNames.insert( pszTarget );
	if ( !s_pTrace )
		return;
	Trace( "fire", Fmt( "\"target\":%s,\"activator\":%s,\"caller\":%s,\"use\":%d,\"value\":%g", Json( pszTarget ).c_str(),
		EntRef( pActivator ).c_str(), EntRef( pCaller ).c_str(), useType, value ) );
}

static void TraceUseEvent( CBaseEntity *pTarget, CBaseEntity *pActivator, CBaseEntity *pCaller, int useType, float value )
{
	if ( !s_pTrace )
		return;
	Trace( "use", Fmt( "\"ent\":%s,\"activator\":%s,\"caller\":%s,\"use\":%d,\"value\":%g", EntRef( pTarget ).c_str(),
		EntRef( pActivator ).c_str(), EntRef( pCaller ).c_str(), useType, value ) );
}

// Every Use and TakeDamage call, not only FireTargets' (buttons, trains, func_tank controls, the engine's DispatchUse, ...): on
// Linux the Use slot of each entity class's vtable is replaced by UseHook, which records the call and calls the
// class's own Use. The slot comes from the Itanium C++ ABI's pointer to a virtual member function (vtable offset + 1).
// The entity code is not changed; without SBO_TEST_COMMANDS none of this exists. Windows: FireTargets' calls only.
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>

typedef void ( *UseFn )( CBaseEntity *, CBaseEntity *, CBaseEntity *, USE_TYPE, float );
static std::map<void **, UseFn> s_useOriginal;	// vtable -> the class's Use
static int s_iUseSlot = -2;						// -2 not computed yet, -1 not available

static int UseSlot( void )
{
	if ( s_iUseSlot != -2 )
		return s_iUseSlot;
	void ( CBaseEntity::*pmf )( CBaseEntity *, CBaseEntity *, USE_TYPE, float ) = &CBaseEntity::Use;
	intptr_t raw[ 2 ];
	memcpy( raw, &pmf, sizeof( raw ) );
	s_iUseSlot = ( raw[0] & 1 ) ? (int)( ( raw[0] - 1 ) / sizeof( void * ) ) : -1;
	return s_iUseSlot;
}

static void UseHook( CBaseEntity *pThis, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	TraceUseEvent( pThis, pActivator, pCaller, useType, value );
	std::map<void **, UseFn>::const_iterator it = s_useOriginal.find( *(void ***)pThis );
	if ( it != s_useOriginal.end() )
		it->second( pThis, pActivator, pCaller, useType, value );
}

// TakeDamage the same way: every hit (bullets, melee, explosions, test_damage) with the health left after it
typedef int ( *DamageFn )( CBaseEntity *, entvars_t *, entvars_t *, float, int );
static std::map<void **, DamageFn> s_damageOriginal;
static int s_iDamageSlot = -2;

static int DamageSlot( void )
{
	if ( s_iDamageSlot != -2 )
		return s_iDamageSlot;
	int ( CBaseEntity::*pmf )( entvars_t *, entvars_t *, float, int ) = &CBaseEntity::TakeDamage;
	intptr_t raw[ 2 ];
	memcpy( raw, &pmf, sizeof( raw ) );
	s_iDamageSlot = ( raw[0] & 1 ) ? (int)( ( raw[0] - 1 ) / sizeof( void * ) ) : -1;
	return s_iDamageSlot;
}

static edict_t *EdictOf( entvars_t *pev )
{
	return pev ? pev->pContainingEntity : NULL;
}

static int DamageHook( CBaseEntity *pThis, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	std::map<void **, DamageFn>::const_iterator it = s_damageOriginal.find( *(void ***)pThis );
	if ( it == s_damageOriginal.end() )
		return 0;
	float flBefore = pThis->pev->health;
	edict_t *pVictim = pThis->edict();
	std::string victim = EntRef( pVictim );	// before: a killed entity may lose its name
	int result = it->second( pThis, pevInflictor, pevAttacker, flDamage, bitsDamageType );
	if ( s_pTrace )
		Trace( "damage", "\"ent\":" + victim + ",\"inflictor\":" + EntRef( EdictOf( pevInflictor ) ) + ",\"attacker\":"
			+ EntRef( EdictOf( pevAttacker ) ) + Fmt( ",\"amount\":%g,\"type\":%d,\"health_before\":%g,\"health\":%g,\"result\":%d",
			flDamage, bitsDamageType, flBefore, pVictim->free ? 0.0f : pVictim->v.health, result ) );
	return result;
}

static bool PatchSlot( void **vt, int slot, void *hook )
{
	long page = sysconf( _SC_PAGESIZE );
	uintptr_t start = (uintptr_t)&vt[ slot ] & ~(uintptr_t)( page - 1 );
	// the vtables are in RELRO memory (read-only after loading); the page stays writable (test builds only)
	if ( mprotect( (void *)start, page, PROT_READ | PROT_WRITE ) != 0 )
		return false;
	vt[ slot ] = hook;
	return true;
}

static void HookUse( CBaseEntity *p )
{
	if ( !p )
		return;
	void **vt = *(void ***)p;
	int slot = UseSlot();
	if ( slot >= 0 && !s_useOriginal.count( vt ) && vt[ slot ] != (void *)UseHook )
	{
		void *orig = vt[ slot ];
		if ( PatchSlot( vt, slot, (void *)UseHook ) )
			s_useOriginal[ vt ] = (UseFn)orig;
	}
	slot = DamageSlot();
	if ( slot >= 0 && !s_damageOriginal.count( vt ) && vt[ slot ] != (void *)DamageHook )
	{
		void *orig = vt[ slot ];
		if ( PatchSlot( vt, slot, (void *)DamageHook ) )
			s_damageOriginal[ vt ] = (DamageFn)orig;
	}
}
#else
static void HookUse( CBaseEntity * ) {}
#endif

// FireTargets, before it calls Use: Linux makes sure the class is hooked (UseHook records the call), Windows records it
void Test_TraceUse( CBaseEntity *pTarget, CBaseEntity *pActivator, CBaseEntity *pCaller, int useType, float value )
{
#ifndef _WIN32
	HookUse( pTarget );
	if ( !s_useOriginal.count( *(void ***)pTarget ) )
		TraceUseEvent( pTarget, pActivator, pCaller, useType, value );
#else
	TraceUseEvent( pTarget, pActivator, pCaller, useType, value );
#endif
}

static void ScanEntities( void )
{
	int n = gpGlobals->maxEntities;
	if ( n > (int)( sizeof( s_serial ) / sizeof( s_serial[0] ) ) )
		n = sizeof( s_serial ) / sizeof( s_serial[0] );
	for ( int i = 0; i < n; i++ )
	{
		edict_t *e = s_engine.pfnPEntityOfEntIndex( i );
		bool used = e && !e->free && e->pvPrivateData;
		int serial = e ? e->serialnumber : 0;
		if ( used )
			HookUse( (CBaseEntity *)e->pvPrivateData );
		if ( s_bScanBase )
		{
			if ( s_used[i] && ( !used || serial != s_serial[i] ) )
				Trace( "remove", Fmt( "\"i\":%d", i ) );
			if ( used && ( !s_used[i] || serial != s_serial[i] ) )
				Trace( "spawn", "\"ent\":" + EntRef( e ) + Fmt( ",\"origin\":[%g,%g,%g]", e->v.origin[0], e->v.origin[1], e->v.origin[2] ) );
		}
		else if ( used )
			Trace( "exists", "\"ent\":" + EntRef( e ) + Fmt( ",\"origin\":[%g,%g,%g]", e->v.origin[0], e->v.origin[1], e->v.origin[2] ) );
		s_used[i] = used;
		s_serial[i] = serial;
	}
	s_bScanBase = true;
}

// ------------------------------------------------------------------------------------------ commands
typedef void ( *TestCmd )( const Args &a, edict_t *pSelf );

static void OpenTrace( const std::string &file )
{
	if ( s_pTrace )
	{
		Trace( "trace_end", "" );
		fclose( s_pTrace );
		s_pTrace = NULL;
	}
	if ( file.empty() || file == "off" )
		return;
	char dir[ 256 ];
	s_engine.pfnGetGameDir( dir );
	std::string path = std::string( dir ) + "/" + file;
	s_pTrace = fopen( path.c_str(), "w" );
	if ( !s_pTrace )
	{
		Print( "test_trace: can't write %s\n", path.c_str() );
		return;
	}
	setvbuf( s_pTrace, NULL, _IOLBF, 0 );
	s_bScanBase = false;
	Trace( "trace_start", Fmt( "\"map\":%s,\"frametime\":%g,\"maxclients\":%d,\"maxentities\":%d",
		Json( STRING( gpGlobals->mapname ) ).c_str(), gpGlobals->frametime, gpGlobals->maxClients, gpGlobals->maxEntities ) );
	for ( std::map<int, std::string>::const_iterator it = s_userMsgs.begin(); it != s_userMsgs.end(); ++it )
		Trace( "usermsg", Fmt( "\"id\":%d,\"name\":%s", it->first, Json( it->second.c_str() ).c_str() ) );
	Trace( "precache_counts", Fmt( "\"models\":%d,\"sounds\":%d,\"generic\":%d", (int)s_precModels.size(),
		(int)s_precSounds.size(), (int)s_precGeneric.size() ) );
	// the precaches of the map load, made before the trace was open ("load": expect.precached / not_precached)
	const struct { const char *kind; const std::set<std::string> *names; } loaded[] =
		{ { "model", &s_precModels }, { "sound", &s_precSounds }, { "generic", &s_precGeneric } };
	for ( int k = 0; k < 3; k++ )
		for ( std::set<std::string>::const_iterator it = loaded[k].names->begin(); it != loaded[k].names->end(); ++it )
		{
			if ( k == 0 )	// a model: its index too (every name in the set was precached on this map)
				Trace( "precache", Fmt( "\"kind\":\"model\",\"name\":%s,\"load\":1,\"index\":%d", Json( it->c_str() ).c_str(),
					s_engine.pfnModelIndex( it->c_str() ) ) );
			else
				Trace( "precache", Fmt( "\"kind\":\"%s\",\"name\":%s,\"load\":1", loaded[k].kind, Json( it->c_str() ).c_str() ) );
		}
	ScanEntities();
}

static void Cmd_Trace( const Args &a, edict_t * ) { OpenTrace( a.size() > 1 ? a[1] : "off" ); }

// test_precache_first <model> ...: precached first on the next map load (W_PrecacheModel)
static void Cmd_PrecacheFirst( const Args &a, edict_t * )
{
	for ( size_t i = 1; i < a.size(); i++ )
		s_precacheFirst.push_back( a[i] );
}

static void Cmd_Seed( const Args &a, edict_t * )
{
	s_iRand = a.size() > 1 ? (unsigned int)strtoul( a[1].c_str(), NULL, 0 ) : 1;
	if ( !s_iRand )
		s_iRand = 1;
	s_bSeeded = true;
	Trace( "seed", Fmt( "\"seed\":%u", s_iRand ) );
}

static void Cmd_Mark( const Args &a, edict_t * )
{
	Trace( "mark", "\"label\":" + Json( Join( a, 1 ).c_str() ) );
	Print( "test_mark %s\n", Join( a, 1 ).c_str() );
}

static void Cmd_Time( const Args &, edict_t * )
{
	Print( "test_time time %.4f frame %lu frametime %.5f wall %.3f\n", gpGlobals->time, s_iFrame, gpGlobals->frametime,
		WallTime() - s_flWall0 );
	Trace( "time", Fmt( "\"frametime\":%g,\"wall\":%.4f", gpGlobals->frametime, WallTime() - s_flWall0 ) );
}

static void Cmd_Snap( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 3 )
	{
		Print( "usage: test_snap <label> <entity> [fields]\n" );
		return;
	}
	std::vector<edict_t *> v = Resolve( a[2], pSelf );
	if ( v.empty() )
		Trace( "snap", "\"label\":" + Json( a[1].c_str() ) + ",\"ref\":" + Json( a[2].c_str() ) + ",\"ent\":null" );
	for ( size_t i = 0; i < v.size(); i++ )
	{
		std::string s = Snapshot( v[i], a, 3 );
		if ( s_pTrace )
			Trace( "snap", "\"label\":" + Json( a[1].c_str() ) + ",\"ref\":" + Json( a[2].c_str() ) + "," + s );
		else
			Print( "%s\n", s.c_str() );
	}
}

static void Cmd_Player( const Args &a, edict_t * )
{
	if ( a.size() >= 2 && a[1] == "add" )
	{
		std::string name = a.size() > 2 ? a[2] : Fmt( "test%d", (int)s_fakes.size() + 1 );
		edict_t *e = s_engine.pfnCreateFakeClient( name.c_str() );
		if ( FNullEnt( e ) )
		{
			Print( "test_player: no free player slot\n" );
			Trace( "error", "\"cmd\":\"test_player\",\"msg\":\"no free player slot\"" );
			return;
		}
		// as a bot does: the game's player entity, the client's keys, then connect and put in server
		GetClassPtr( (CBasePlayer *)VARS( e ) );
		char *info = s_engine.pfnGetInfoKeyBuffer( e );
		s_engine.pfnSetClientKeyValue( ENTINDEX( e ), info, "_vgui_menus", "0" );
		// what SBO's client sends (FCVAR_USERINFO): weapon switch as default
		s_engine.pfnSetClientKeyValue( ENTINDEX( e ), info, "cl_autowepswitch", "1" );
		char reject[ 128 ] = "";
		ClientConnect( e, name.c_str(), "127.0.0.1", reject );
		ClientPutInServer( e );
		// A test player is a client like any other for the game (message boxes, tutor, AFK rules skip bots): the
		// engine's FL_FAKECLIENT is cleared unless "test_player add <name> bot". The engine still knows it is fake.
		if ( a.size() > 3 && a[3] == "bot" )
			e->v.flags |= FL_FAKECLIENT;
		else
			e->v.flags &= ~FL_FAKECLIENT;
		FakePlayer fp;
		memset( &fp, 0, sizeof( fp ) );
		fp.ed = e;
		s_fakes.push_back( fp );
		Trace( "player_add", "\"ent\":" + EntRef( e ) + ",\"name\":" + Json( name.c_str() ) );
		Print( "test_player: added %s (#%d)\n", name.c_str(), ENTINDEX( e ) );
		return;
	}
	if ( a.size() >= 3 && a[1] == "remove" )
	{
		edict_t *e = ResolveOne( a[2], NULL, "test_player" );
		if ( !e )
			return;
		for ( size_t i = 0; i < s_fakes.size(); i++ )
			if ( s_fakes[i].ed == e )
				s_fakes.erase( s_fakes.begin() + i-- );
		char cmd[ 64 ];
		snprintf( cmd, sizeof( cmd ), "kick #%d\n", GETPLAYERUSERID( e ) );
		SERVER_COMMAND( cmd );
		SERVER_EXECUTE();
		return;
	}
	// test_player level <player> <level>: the co-op level (m_iSboSpeed 300 + 5 per level) and the scoreboard XP
	// that goes with it (5000 per level, as CCoop::PlayerKilled's penalty counts it); the next spawn's loadout
	// builds the speed from it
	if ( a.size() >= 4 && a[1] == "level" )
	{
		edict_t *e = ResolveOne( a[2], NULL, "test_player" );
		CBaseEntity *p = e ? CBaseEntity::Instance( e ) : NULL;
		if ( !p || !p->IsPlayer() )
			return;
		CBasePlayer *pl = (CBasePlayer *)p;
		int iLevel = atoi( a[3].c_str() );
		pl->m_iSboSpeed = 300 + 5 * iLevel;
		pl->m_iSboTotalXP = 5000 * iLevel;
		SboScoreboard_SendScoreInfo( pl );
		Trace( "player_level", "\"ent\":" + EntRef( e ) + Fmt( ",\"level\":%d", iLevel ) );
		return;
	}
	// test_player sbo_health <player> <health>: the spawn health (m_iSboHealth) the next spawn gives
	if ( a.size() >= 4 && a[1] == "sbo_health" )
	{
		edict_t *e = ResolveOne( a[2], NULL, "test_player" );
		CBaseEntity *p = e ? CBaseEntity::Instance( e ) : NULL;
		if ( !p || !p->IsPlayer() )
			return;
		CBasePlayer *pl = (CBasePlayer *)p;
		pl->m_iSboHealth = atoi( a[3].c_str() );
		Trace( "player_sbo_health", "\"ent\":" + EntRef( e ) + Fmt( ",\"sbo_health\":%d", pl->m_iSboHealth ) );
		return;
	}
	Print( "usage: test_player add [name] [bot] | remove <player> | level <player> <level> | sbo_health <player> <health>\n" );
}

// the selection menu's "Go !" (client.cpp Spectate_MenuConfirmed: blade, light, heavy, item, cyberware, armor,
// malefemale), without the menus
static void Cmd_Join( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 2 )
	{
		Print( "usage: test_join <player> [blade light heavy item cyberware armor malefemale]\n" );
		return;
	}
	edict_t *e = ResolvePlayer( a[1], pSelf, "test_join" );
	if ( !e )
		return;
	static const char *steps[] = { "blade", "light", "heavy", "item", "cyberware", "armor", "malefemale" };
	for ( int i = 0; i < 7; i++ )
	{
		Args c;
		c.push_back( steps[i] );
		c.push_back( a.size() > (size_t)( 2 + i ) ? a[ 2 + i ] : "1" );
		FakeClientCommand( e, c );
	}
}

static void Cmd_Cmd( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 3 )
	{
		Print( "usage: test_cmd <player> <client command ...>\n" );
		return;
	}
	edict_t *e = ResolvePlayer( a[1], pSelf, "test_cmd" );
	if ( e )
		FakeClientCommand( e, Args( a.begin() + 2, a.end() ) );
}

// test_userinfo <player> <key> <value>: a client setting changed (ClientUserInfoChanged)
static void Cmd_Userinfo( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 4 )
	{
		Print( "usage: test_userinfo <player> <key> <value>\n" );
		return;
	}
	edict_t *e = ResolvePlayer( a[1], pSelf, "test_userinfo" );
	if ( !e )
		return;
	char *info = s_engine.pfnGetInfoKeyBuffer( e );
	s_engine.pfnSetClientKeyValue( ENTINDEX( e ), info, (char *)a[2].c_str(), (char *)a[3].c_str() );
	ClientUserInfoChanged( e, info );
}

// test_flags <ref> <+bits|-bits|=bits>: pev->flags directly (FL_NOTARGET 128, FL_GODMODE 64, ...); numbers or
// the FL_ names
static int FlagBits( const std::string &s )
{
	static const struct { const char *name; int bit; } names[] =
	{
		{ "FL_FLY", FL_FLY }, { "FL_SWIM", FL_SWIM }, { "FL_CONVEYOR", FL_CONVEYOR }, { "FL_CLIENT", FL_CLIENT },
		{ "FL_INWATER", FL_INWATER }, { "FL_MONSTER", FL_MONSTER }, { "FL_GODMODE", FL_GODMODE },
		{ "FL_NOTARGET", FL_NOTARGET }, { "FL_ONGROUND", FL_ONGROUND }, { "FL_FROZEN", FL_FROZEN },
	};
	for ( size_t i = 0; i < sizeof( names ) / sizeof( names[0] ); i++ )
		if ( !stricmp( s.c_str(), names[i].name ) )
			return names[i].bit;
	return (int)strtol( s.c_str(), NULL, 0 );
}

static void Cmd_Flags( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 3 || ( a[2][0] != '+' && a[2][0] != '-' && a[2][0] != '=' ) )
	{
		Print( "usage: test_flags <entity> <+bits | -bits | =bits>\n" );
		return;
	}
	int bits = FlagBits( a[2].substr( 1 ) );
	std::vector<edict_t *> v = Resolve( a[1], pSelf );
	for ( size_t i = 0; i < v.size(); i++ )
	{
		if ( a[2][0] == '+' ) v[i]->v.flags |= bits;
		else if ( a[2][0] == '-' ) v[i]->v.flags &= ~bits;
		else v[i]->v.flags = bits;
		Trace( "flags", "\"ent\":" + EntRef( v[i] ) + Fmt( ",\"flags\":%d", v[i]->v.flags ) );
	}
}

static void Cmd_Spawn( const Args &a, edict_t *pSelf )
{
	edict_t *e = ResolveOne( a.size() > 1 ? a[1] : "", pSelf, "test_spawn" );
	if ( !e )
		return;
	CBasePlayer *pl = (CBasePlayer *)CBaseEntity::Instance( e );
	if ( pl && pl->IsPlayer() )
		respawn( pl->pev, FALSE );
}

// test_hitscan <label> <entity> <fx fy fz> <tx ty tz>: a bullet's trace (a line that meets monsters and players,
// their hitboxes) between two points given relative to the entity's origin; into the trace as a snapshot of the
// entity: hit (1: the trace hit this entity), hitgroup, fraction, endpos (SBO 2.0 model / head scale hitboxes)
static void Cmd_Hitscan( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 9 )
	{
		Print( "usage: test_hitscan <label> <entity> <fx fy fz> <tx ty tz>\n" );
		return;
	}
	edict_t *e = ResolveOne( a[2], pSelf, "test_hitscan" );
	if ( !e )
		return;
	Vector vecFrom = e->v.origin + Vector( atof( a[3].c_str() ), atof( a[4].c_str() ), atof( a[5].c_str() ) );
	Vector vecTo = e->v.origin + Vector( atof( a[6].c_str() ), atof( a[7].c_str() ), atof( a[8].c_str() ) );
	TraceResult tr;
	UTIL_TraceLine( vecFrom, vecTo, dont_ignore_monsters, NULL, &tr );
	std::string v = Fmt( "\"hit\":%d,\"hitgroup\":%d,\"fraction\":%g,\"endpos\":[%g,%g,%g]",
		tr.pHit == e ? 1 : 0, tr.iHitgroup, tr.flFraction, tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z );
	if ( s_pTrace )
		Trace( "snap", "\"label\":" + Json( a[1].c_str() ) + ",\"ref\":" + Json( a[2].c_str() ) + ",\"ent\":" + EntRef( e )
			+ ",\"v\":{" + v + "}" );
	else
		Print( "test_hitscan: %s\n", v.c_str() );
}

static void Cmd_Teleport( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 3 )
	{
		Print( "usage: test_teleport <entity> <x y z | entity> [yaw]\n" );
		return;
	}
	edict_t *e = ResolveOne( a[1], pSelf, "test_teleport" );
	if ( !e )
		return;
	Vector pos, ang = e->v.angles;
	size_t next;
	if ( a.size() >= 5 && ( isdigit( (unsigned char)a[2][0] ) || a[2][0] == '-' || a[2][0] == '.' ) )
	{
		pos = Vector( atof( a[2].c_str() ), atof( a[3].c_str() ), atof( a[4].c_str() ) );
		next = 5;
	}
	else
	{
		edict_t *d = ResolveOne( a[2], pSelf, "test_teleport" );
		if ( !d )
			return;
		pos = d->v.origin;
		ang = d->v.angles;
		next = 3;
	}
	if ( a.size() > next )
		ang = Vector( 0, atof( a[next].c_str() ), 0 );
	UTIL_SetOrigin( &e->v, pos );
	e->v.angles = ang;
	e->v.velocity = g_vecZero;
	if ( e->v.flags & FL_CLIENT )
	{
		e->v.v_angle = ang;
		e->v.fixangle = 1;
		FakePlayer *fp = FindFake( e );
		if ( fp )
			memcpy( fp->angles, &ang.x, sizeof( fp->angles ) );
	}
	Trace( "teleport", "\"ent\":" + EntRef( e ) + Fmt( ",\"origin\":[%g,%g,%g]", pos.x, pos.y, pos.z ) );
}

// test_move <player> <forward> <side> <frames> [+use] [+jump] [+duck] [+attack]
static void Cmd_Move( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 5 )
	{
		Print( "usage: test_move <player> <forward> <side> <frames> [+use +forward +jump +duck +attack +attack2]\n" );
		return;
	}
	FakePlayer *fp = FindFake( ResolveOne( a[1], pSelf, "test_move" ) );
	if ( !fp )
	{
		Print( "test_move: not a test player\n" );
		return;
	}
	fp->fwd = atof( a[2].c_str() );
	fp->side = atof( a[3].c_str() );
	fp->frames = atol( a[4].c_str() );
	fp->buttons = 0;
	for ( size_t i = 5; i < a.size(); i++ )
	{
		if ( a[i] == "+use" ) fp->buttons |= IN_USE;
		else if ( a[i] == "+forward" ) fp->buttons |= IN_FORWARD;	// a client holding +forward sends the bit with the move
		else if ( a[i] == "+jump" ) fp->buttons |= IN_JUMP;
		else if ( a[i] == "+duck" ) fp->buttons |= IN_DUCK;
		else if ( a[i] == "+attack" ) fp->buttons |= IN_ATTACK;
		else if ( a[i] == "+attack2" ) fp->buttons |= IN_ATTACK2;
		else if ( a[i] == "+reload" ) fp->buttons |= IN_RELOAD;
	}
}

static void Cmd_Look( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 4 )
	{
		Print( "usage: test_look <player> <pitch> <yaw>\n" );
		return;
	}
	edict_t *e = ResolveOne( a[1], pSelf, "test_look" );
	FakePlayer *fp = FindFake( e );
	if ( !fp )
		return;
	fp->angles[0] = atof( a[2].c_str() );
	fp->angles[1] = atof( a[3].c_str() );
	e->v.v_angle = e->v.angles = Vector( fp->angles[0], fp->angles[1], 0 );
	e->v.fixangle = 1;
}

static int ParseUse( const std::string &s )
{
	if ( s == "off" ) return USE_OFF;
	if ( s == "on" ) return USE_ON;
	if ( s == "set" ) return USE_SET;
	return USE_TOGGLE;
}

// test_fire <target> [activator] [caller] [on|off|toggle|set] [value]
static void Cmd_Fire( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 2 )
	{
		Print( "usage: test_fire <target> [activator] [caller] [on|off|toggle|set] [value]\n" );
		return;
	}
	CBaseEntity *pAct = NULL, *pCaller = NULL;
	if ( a.size() > 2 && a[2] != "-" )
	{
		edict_t *e = ResolveOne( a[2], pSelf, "test_fire" );
		pAct = e ? CBaseEntity::Instance( e ) : NULL;
	}
	if ( a.size() > 3 && a[3] != "-" )
	{
		edict_t *e = ResolveOne( a[3], pSelf, "test_fire" );
		pCaller = e ? CBaseEntity::Instance( e ) : NULL;
	}
	if ( !pCaller )
		pCaller = CBaseEntity::Instance( s_engine.pfnPEntityOfEntIndex( 0 ) );
	FireTargets( a[1].c_str(), pAct, pCaller, (USE_TYPE)ParseUse( a.size() > 4 ? a[4] : "toggle" ),
		a.size() > 5 ? atof( a[5].c_str() ) : 0 );
}

static void Cmd_Kv( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 4 )
	{
		Print( "usage: test_kv <entity> <key> <value>\n" );
		return;
	}
	std::vector<edict_t *> v = Resolve( a[1], pSelf );
	for ( size_t i = 0; i < v.size(); i++ )
	{
		KeyValueData kvd;
		kvd.szClassName = (char *)STRING( v[i]->v.classname );
		kvd.szKeyName = (char *)a[2].c_str();
		kvd.szValue = (char *)a[3].c_str();
		kvd.fHandled = FALSE;
		DispatchKeyValue( v[i], &kvd );
		Trace( "kv", "\"ent\":" + EntRef( v[i] ) + ",\"key\":" + Json( a[2].c_str() ) + ",\"value\":" + Json( a[3].c_str() )
			+ Fmt( ",\"handled\":%d", kvd.fHandled ? 1 : 0 ) );
	}
}

extern void ClearMultiDamage( void );	// weapons.h / combat.cpp
extern void ApplyMultiDamage( entvars_t *pevInflictor, entvars_t *pevAttacker );

// test_damage <entity> <amount> [dmgtype] [attacker] [hitgroup]: TakeDamage directly, or with a hitgroup (HITGROUP_*:
// 1 head, 2 chest, 3 stomach, 4/5 arms, 6/7 legs) a hit on that part the way FireBullets lands it: TraceAttack
// (the entity's hit-group multipliers, SBO's armor location), then ApplyMultiDamage (TakeDamage)
static void Cmd_Damage( const Args &a, edict_t *pSelf )
{
	if ( a.size() < 3 )
	{
		Print( "usage: test_damage <entity> <amount> [dmgtype] [attacker] [hitgroup]\n" );
		return;
	}
	edict_t *pAtt = a.size() > 4 ? ResolveOne( a[4], pSelf, "test_damage" ) : s_engine.pfnPEntityOfEntIndex( 0 );
	int iType = a.size() > 3 ? atoi( a[3].c_str() ) : DMG_GENERIC;
	std::vector<edict_t *> v = Resolve( a[1], pSelf );
	for ( size_t i = 0; i < v.size(); i++ )
	{
		CBaseEntity *p = CBaseEntity::Instance( v[i] );
		if ( !p || !pAtt )
			continue;
		if ( a.size() > 5 )
		{
			TraceResult tr;
			memset( &tr, 0, sizeof( tr ) );
			tr.flFraction = 0.5f;
			tr.vecEndPos = p->pev->origin;
			tr.pHit = v[i];
			tr.iHitgroup = atoi( a[5].c_str() );
			ClearMultiDamage();
			p->TraceAttack( VARS( pAtt ), atof( a[2].c_str() ), Vector( 1, 0, 0 ), &tr, iType );
			ApplyMultiDamage( VARS( pAtt ), VARS( pAtt ) );
		}
		else
			p->TakeDamage( VARS( pAtt ), VARS( pAtt ), atof( a[2].c_str() ), iType );
	}
}

// test_round start | restart | state
static void Cmd_Round( const Args &a, edict_t * )
{
	if ( m_Mode != MODE_COOP || !g_pGameRules )
	{
		Print( "test_round: co-op maps only\n" );
		return;
	}
	CCoop *pCoop = (CCoop *)g_pGameRules;
	if ( a.size() > 1 && a[1] == "start" )
	{
		// the next Think runs the pending step now (SetupRound from ROUND_PAUSE / ROUND_WAITING)
		pCoop->m_iNextRoundStep = (int)gpGlobals->time;
	}
	else if ( a.size() > 1 && a[1] == "restart" )
	{
		// a new round now, as after a round's pause: ROUND_PAUSE -> CheckRound -> SetupRound -> CheckWho (the last
		// round ends, round_reset entities reset, the players respawn, the new round starts)
		pCoop->m_iRoundState = ROUND_PAUSE;
		pCoop->m_iNextRoundStep = (int)gpGlobals->time;
	}
	Trace( "round", Fmt( "\"state\":%d,\"next\":%d", pCoop->m_iRoundState, pCoop->m_iNextRoundStep ) );
	Print( "test_round: state %d next step %d (time %.2f)\n", pCoop->m_iRoundState, pCoop->m_iNextRoundStep, gpGlobals->time );
}

// test_create <classname> <x y z> [yaw] [key value ...]: a new entity of a class the map has precached, its keys set
// before it spawns (render keys, scale, ...); prints its entity index
static void Cmd_Create( const Args &a, edict_t * )
{
	if ( a.size() < 5 )
	{
		Print( "usage: test_create <classname> <x y z> [yaw] [key value ...]\n" );
		return;
	}
	edict_t *e = CREATE_NAMED_ENTITY( ALLOC_STRING( a[1].c_str() ) );
	if ( FNullEnt( e ) || !GET_PRIVATE( e ) )
	{
		Print( "test_create: no class %s\n", a[1].c_str() );
		if ( !FNullEnt( e ) )
			REMOVE_ENTITY( e );
		return;
	}
	e->v.origin = Vector( atof( a[2].c_str() ), atof( a[3].c_str() ), atof( a[4].c_str() ) );
	size_t next = 5;
	if ( a.size() % 2 == 0 )	// an odd count of words after the position: the first is the yaw
		e->v.angles.y = atof( a[next++].c_str() );
	for ( ; next + 1 < a.size(); next += 2 )
	{
		KeyValueData kvd;
		kvd.szClassName = (char *)STRING( e->v.classname );
		kvd.szKeyName = (char *)a[next].c_str();
		kvd.szValue = (char *)a[next + 1].c_str();
		kvd.fHandled = FALSE;
		DispatchKeyValue( e, &kvd );
	}
	DispatchSpawn( e );
	Trace( "create", "\"ent\":" + EntRef( e ) );
	Print( "test_create: %s #%d\n", a[1].c_str(), ENTINDEX( e ) );
}

static void Cmd_Quit( const Args &, edict_t * )
{
	Trace( "precache_counts", Fmt( "\"models\":%d,\"sounds\":%d,\"generic\":%d", (int)s_precModels.size(),
		(int)s_precSounds.size(), (int)s_precGeneric.size() ) );
	Trace( "end", Fmt( "\"wall\":%.4f", WallTime() - s_flWall0 ) );
	OpenTrace( "off" );
	SERVER_COMMAND( (char *)"quit\n" );
}

static void Cmd_Run( const Args &a, edict_t * );

struct CmdEntry { const char *name; TestCmd fn; };
static const CmdEntry s_cmds[] =
{
	{ "test_run", Cmd_Run }, { "test_trace", Cmd_Trace }, { "test_seed", Cmd_Seed }, { "test_mark", Cmd_Mark },
	{ "test_time", Cmd_Time }, { "test_snap", Cmd_Snap }, { "test_player", Cmd_Player }, { "test_join", Cmd_Join },
	{ "test_cmd", Cmd_Cmd }, { "test_spawn", Cmd_Spawn }, { "test_teleport", Cmd_Teleport }, { "test_move", Cmd_Move },
	{ "test_look", Cmd_Look }, { "test_fire", Cmd_Fire }, { "test_kv", Cmd_Kv }, { "test_damage", Cmd_Damage },
	{ "test_round", Cmd_Round }, { "test_quit", Cmd_Quit }, { "test_userinfo", Cmd_Userinfo },
	{ "test_flags", Cmd_Flags }, { "test_precache_first", Cmd_PrecacheFirst }, { "test_create", Cmd_Create },
	{ "test_hitscan", Cmd_Hitscan },
};

static bool Dispatch( const Args &a, edict_t *pSelf )
{
	if ( a.empty() )
		return false;
	for ( size_t i = 0; i < sizeof( s_cmds ) / sizeof( s_cmds[0] ); i++ )
	{
		if ( a[0] == s_cmds[i].name )
		{
			s_cmds[i].fn( a, pSelf );
			return true;
		}
	}
	return false;
}

// the server console's commands: one function for all, it reads the real command line
static void ConsoleCommand( void )
{
	Args a;
	for ( int i = 0; i < s_engine.pfnCmd_Argc(); i++ )
		a.push_back( s_engine.pfnCmd_Argv( i ) );
	Dispatch( a, NULL );
}

BOOL Test_ClientCommand( edict_t *pEntity, const char *pcmd )
{
	if ( s_bFakeCmd || strncmp( pcmd, "test_", 5 ) )
		return FALSE;
	if ( CVAR_GET_FLOAT( "sv_cheats" ) == 0 )
		return FALSE;
	Args a;
	for ( int i = 0; i < CMD_ARGC(); i++ )
		a.push_back( CMD_ARGV( i ) );
	if ( a[0] == "test_run" || a[0] == "test_quit" || a[0] == "test_trace" )
		return FALSE;		// server console / rcon only: they write files and stop the server
	return Dispatch( a, pEntity ) ? TRUE : FALSE;
}

// ------------------------------------------------------------------------------------------ step files
static void Cmd_Run( const Args &a, edict_t * )
{
	if ( a.size() < 2 )
	{
		Print( "usage: test_run <file in the game directory>\n" );
		return;
	}
	char dir[ 256 ];
	s_engine.pfnGetGameDir( dir );
	std::string path = std::string( dir ) + "/" + a[1];
	FILE *f = fopen( path.c_str(), "r" );
	if ( !f )
	{
		Print( "test_run: can't read %s\n", path.c_str() );
		return;
	}
	s_steps.clear();
	char line[ 1024 ];
	while ( fgets( line, sizeof( line ), f ) )
		s_steps.push_back( line );
	fclose( f );
	s_iStep = 0;
	s_flWaitUntil = 0;
	s_iWaitFrames = 0;
	s_waitFor.clear();
	s_szStepFile = a[1];
	Print( "test_run: %s, %d lines\n", a[1].c_str(), (int)s_steps.size() );
}

// wait_for fire <name> | round <state> | alive <player>; true when met
static bool WaitForMet( void )
{
	const Args &w = s_waitFor;
	if ( w.size() >= 2 && w[0] == "fire" )
		return s_firedNames.count( w[1] ) > 0;
	if ( w.size() >= 2 && w[0] == "round" && m_Mode == MODE_COOP && g_pGameRules )
		return ( (CCoop *)g_pGameRules )->m_iRoundState == atoi( w[1].c_str() );
	if ( w.size() >= 2 && w[0] == "alive" )
	{
		std::vector<edict_t *> v = Resolve( w[1] );
		return !v.empty() && v[0]->v.deadflag == DEAD_NO && v[0]->v.health > 0 && v[0]->v.solid != SOLID_NOT;
	}
	return true;
}

static void RunSteps( void )
{
	if ( s_iStep >= s_steps.size() )
		return;
	if ( !s_waitFor.empty() )
	{
		if ( WaitForMet() )
		{
			Trace( "wait_done", "\"cond\":" + Json( Join( s_waitFor, 0 ).c_str() ) );
			s_waitFor.clear();
		}
		else if ( gpGlobals->time >= s_flWaitForEnd )
		{
			Trace( "wait_timeout", "\"cond\":" + Json( Join( s_waitFor, 0 ).c_str() ) );
			s_waitFor.clear();
		}
		else
			return;
	}
	if ( gpGlobals->time < s_flWaitUntil || s_iFrame < s_iWaitFrames )
		return;

	while ( s_iStep < s_steps.size() )
	{
		std::string line = s_steps[ s_iStep++ ];
		Args a = Tokenize( line.c_str() );
		if ( a.empty() )
			continue;
		if ( a[0] == "wait" )
		{
			s_flWaitUntil = gpGlobals->time + ( a.size() > 1 ? atof( a[1].c_str() ) : 0 );
			return;
		}
		if ( a[0] == "wait_frames" )
		{
			s_iWaitFrames = s_iFrame + ( a.size() > 1 ? atol( a[1].c_str() ) : 1 );
			return;
		}
		if ( a[0] == "wait_for" )
		{
			// wait_for <kind> <arg> <timeout>
			s_waitFor = Args( a.begin() + 1, a.end() - ( a.size() >= 4 ? 1 : 0 ) );
			s_flWaitForEnd = gpGlobals->time + ( a.size() >= 4 ? atof( a.back().c_str() ) : 10 );
			if ( s_waitFor.size() >= 2 && s_waitFor[0] == "fire" )
				s_firedNames.erase( s_waitFor[1] );
			return;
		}
		Trace( "step", Fmt( "\"line\":%d,\"cmd\":", (int)s_iStep ) + Json( Join( a, 0 ).c_str() ) );
		if ( !Dispatch( a, NULL ) )
		{
			// an engine or game console command (cvars, changelevel, ...)
			std::string cmd = Join( a, 0 ) + "\n";
			SERVER_COMMAND( (char *)cmd.c_str() );
			if ( a[0] == "changelevel" || a[0] == "map" )
			{
				// not from inside StartFrame: the engine runs it from its command buffer before the next frame. The
				// steps go on on the new map (wait in frames there: its game time starts again)
				return;
			}
			SERVER_EXECUTE();
		}
	}
	Print( "test_run: %s done\n", s_szStepFile.c_str() );
}

// ------------------------------------------------------------------------------------------ frame
void Test_GameInit( void )
{
	s_flWall0 = WallTime();
	for ( size_t i = 0; i < sizeof( s_cmds ) / sizeof( s_cmds[0] ); i++ )
		s_engine.pfnAddServerCommand( (char *)s_cmds[i].name, ConsoleCommand );
	Print( "%s: %d commands\n", g_szSboTestCommands, (int)( sizeof( s_cmds ) / sizeof( s_cmds[0] ) ) );
}

void Test_StartFrame( void )
{
	s_iFrame++;

	// fake players: one move per frame, as a client sends one
	for ( size_t i = 0; i < s_fakes.size(); i++ )
	{
		FakePlayer &fp = s_fakes[i];
		// gone (kicked, disconnected): the slot was freed. Not the flags: a test may change them (test_flags)
		if ( fp.ed->free || !fp.ed->pvPrivateData )
		{
			s_fakes.erase( s_fakes.begin() + i-- );
			continue;
		}
		float ms = gpGlobals->frametime * 1000.0f;
		byte msec = (byte)( ms < 1 ? 1 : ms > 255 ? 255 : ms );
		bool held = fp.frames != 0;
		s_engine.pfnRunPlayerMove( fp.ed, fp.angles, held ? fp.fwd : 0, held ? fp.side : 0, 0, held ? fp.buttons : 0, 0, msec );
		// the engine clears fixangle when it sends the angles to a client (SV_WriteClientdataToMessage), which it
		// never does for a fake client: a teleport's fixangle would stay set, and the AFK code would ignore this
		// player's input for good (PostThink: fixangle -> AfkIgnoreInput)
		fp.ed->v.fixangle = 0;
		if ( fp.frames > 0 )
			fp.frames--;
	}

	// the engine calls SetupVisibility when it builds a client's packet (SV_WriteEntitiesToClient), which it never
	// does for a fake client: called here for the test players, so what the game does there (an observer moved to
	// its target) happens for them too
	for ( size_t i = 0; i < s_fakes.size(); i++ )
	{
		unsigned char *pvs = NULL, *pas = NULL;
		SetupVisibility( NULL, s_fakes[i].ed, &pvs, &pas );
	}

	if ( s_pTrace )
		ScanEntities();
	RunSteps();
}

void Test_ServerActivate( void )
{
	s_bScanBase = false;
	s_flWaitUntil = 0;		// a wait in game seconds from the last map (its clock): a new map's time starts again
	if ( s_pTrace )
	{
		Trace( "map_start", "\"map\":" + Json( STRING( gpGlobals->mapname ) ) );
		Trace( "precache_counts", Fmt( "\"models\":%d,\"sounds\":%d,\"generic\":%d", (int)s_precModels.size(),
			(int)s_precSounds.size(), (int)s_precGeneric.size() ) );
	}
}

void Test_ServerDeactivate( void )
{
	if ( s_pTrace )
		Trace( "map_end", "" );
	s_fakes.clear();
	s_precModels.clear();
	s_precSounds.clear();
	s_precGeneric.clear();
	s_eventNames.clear();
}
