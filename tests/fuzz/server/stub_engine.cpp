// Stub GoldSrc engine for the server fuzz harness: see stub_engine.h.
#include "stub_engine.h"

#include <dlfcn.h>
#include <strings.h>

edict_t *g_stubEdicts;
int g_stubMaxClients = 8;
globalvars_t g_stubGlobals;
DLL_FUNCTIONS g_dll;
NEW_DLL_FUNCTIONS g_newDll;
bool g_stubHaveNewDll;
bool g_stubLoading = true;
int g_stubReports;
std::string g_stubAction;
bool g_stubVerbose;
std::string g_stubGameDir;
std::string g_stubConsole;	// everything the game printed (ALERT, SERVER_PRINT): the cases' "expect"

std::set<std::string> g_stubMapStrings;
int g_stubMapReports;

static void *s_hGame;
static char s_stringBase[1] = { 0 };	// gpGlobals->pStringBase: offset 0 is ""

void Stub_Report( const char *fmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	g_stubReports++;
	fprintf( stderr, "ENGINE: %s   [during: %s]\n", buf, g_stubAction.c_str() );
}

// a report about a string from a key value: the map's error, not the game's
static bool MapString( const char *s )
{
	if ( s && g_stubMapStrings.count( s ) )
	{
		g_stubMapReports++;
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------------------------- strings

int Stub_AllocString( const char *s )
{
	if ( !s )
	{
		Stub_Report( "ALLOC_STRING( NULL )" );
		s = "";
	}
	// kept for the whole run, like the engine's hunk strings
	char *p = strdup( s );
	return (int)( p - s_stringBase );
}

static const char *Str( int i ) { return s_stringBase + i; }

//------------------------------------------------------------------------------------------------- edicts

static int EdictIndex( const edict_t *e ) { return e ? (int)( e - g_stubEdicts ) : -1; }

static bool ValidEdict( const edict_t *e )
{
	int i = EdictIndex( e );
	return i >= 0 && i < STUB_MAX_EDICTS;
}

edict_t *Stub_AllocEdict( void )
{
	for ( int i = g_stubMaxClients + 1; i < STUB_MAX_EDICTS; i++ )
	{
		edict_t *e = &g_stubEdicts[i];
		if ( e->free && ( e->freetime < 2 || g_stubGlobals.time - e->freetime > 0.5f ) )
		{
			memset( &e->v, 0, sizeof( e->v ) );
			e->free = 0;
			e->pvPrivateData = NULL;
			e->v.pContainingEntity = e;
			return e;
		}
	}
	Stub_Report( "ED_Alloc: no free edicts (Sys_Error)" );
	abort();
}

void Stub_FreeEdict( edict_t *e )
{
	if ( !ValidEdict( e ) )
	{
		Stub_Report( "REMOVE_ENTITY of a bad edict %p", (void *)e );
		return;
	}
	if ( e->free )
		return;
	if ( EdictIndex( e ) == 0 )
	{
		Stub_Report( "REMOVE_ENTITY of the world" );
		return;
	}
	if ( e->pvPrivateData )
	{
		if ( g_stubHaveNewDll && g_newDll.pfnOnFreeEntPrivateData )
			g_newDll.pfnOnFreeEntPrivateData( e );
		free( e->pvPrivateData );
		e->pvPrivateData = NULL;
	}
	e->serialnumber++;
	e->freetime = g_stubGlobals.time;
	memset( &e->v, 0, sizeof( e->v ) );
	if ( EdictIndex( e ) > g_stubMaxClients )
		e->free = 1;
}

edict_t *Stub_CreateNamed( const char *classname )
{
	typedef void ( *ENTITYINIT )( entvars_t * );
	ENTITYINIT pfn = (ENTITYINIT)dlsym( s_hGame, classname );
	if ( !pfn )
	{
		if ( g_stubVerbose )
			fprintf( stderr, "Can't create entity: %s\n", classname );
		return NULL;
	}
	edict_t *e = Stub_AllocEdict();
	e->v.classname = Stub_AllocString( classname );
	pfn( &e->v );
	return e;
}

//------------------------------------------------------------------------------------------------- precache

static std::set<std::string> s_models, s_sounds, s_generic;
static int s_iModels = 1, s_iSounds = 1, s_iGeneric = 1, s_iEvents = 1;

static std::string Lower( const char *s )
{
	std::string r( s );
	for ( auto &c : r )
		c = (char)tolower( (unsigned char)c );
	return r;
}

static int Precache( std::set<std::string> &set, int &count, int limit, const char *what, const char *s )
{
	if ( !s )
	{
		Stub_Report( "PF_precache_%s_I: NULL pointer (Host_Error)", what );
		return 0;
	}
	if ( !s[0] || (unsigned char)s[0] <= ' ' )
	{
		// an empty one is a key the map left out (pev->model of an env_sprite without a model): the map's error
		if ( !s[0] || MapString( s ) )
		{
			g_stubMapReports += !s[0];
			return 0;
		}
		Stub_Report( "PF_precache_%s_I: Bad string '%s' (Host_Error)", what, s );
		return 0;
	}
	std::string k = Lower( s );
	if ( set.count( k ) )
		return 1;
	if ( !g_stubLoading )
	{
		Stub_Report( "PF_precache_%s_I: '%s' Precache can only be done in spawn functions (Host_Error)", what, s );
		return 0;
	}
	// no limit check: the harness spawns more kinds of entities than a map has (real counts: hx precache)
	(void)limit;
	if ( strlen( s ) >= 64 && !MapString( s ) )
		Stub_Report( "PF_precache_%s_I: '%s' is longer than 63 characters", what, s );
	set.insert( k );
	return count++;
}

static int PrecacheModel( char *s ) { return Precache( s_models, s_iModels, 512, "model", s ); }
static int PrecacheSound( char *s ) { return Precache( s_sounds, s_iSounds, 512, "sound", s ); }
static int PrecacheGeneric( char *s ) { return Precache( s_generic, s_iGeneric, 512, "generic", s ); }

static bool ModelPrecached( const char *m )
{
	return m && ( m[0] == '*' || s_models.count( Lower( m ) ) );
}

static void SetModel( edict_t *e, const char *m )
{
	if ( !ValidEdict( e ) )
	{
		Stub_Report( "SET_MODEL of a bad edict" );
		return;
	}
	if ( !m )
	{
		Stub_Report( "SET_MODEL( NULL ) (Host_Error)" );
		return;
	}
	if ( m[0] && !ModelPrecached( m ) && !MapString( m ) )
		Stub_Report( "SET_MODEL: no precache: %s (Host_Error)", m );
	e->v.model = Stub_AllocString( m );
	e->v.modelindex = m[0] ? 1 : 0;
}

static int ModelIndex( const char *m )
{
	if ( !m || !m[0] )
		return 0;
	if ( !ModelPrecached( m ) && !MapString( m ) )
		Stub_Report( "SV_ModelIndex: model %s not precached (Sys_Error)", m );
	return 1;
}

static int ModelFrames( int ) { return 1; }

static void SetSize( edict_t *e, const float *mins, const float *maxs )
{
	if ( !ValidEdict( e ) )
		return;
	if ( mins[0] > maxs[0] || mins[1] > maxs[1] || mins[2] > maxs[2] )
		Stub_Report( "SET_SIZE: backwards mins/maxs (Host_Error)" );
	e->v.mins = Vector( mins );
	e->v.maxs = Vector( maxs );
	e->v.size = e->v.maxs - e->v.mins;
}

static void SetOrigin( edict_t *e, const float *org )
{
	if ( !ValidEdict( e ) )
		return;
	e->v.origin = Vector( org );
	e->v.absmin = e->v.origin + e->v.mins;
	e->v.absmax = e->v.origin + e->v.maxs;
}

static void ChangeLevel( char *s1, char *s2 )
{
	if ( g_stubVerbose )
		fprintf( stderr, "changelevel %s %s\n", s1 ? s1 : "(null)", s2 ? s2 : "" );
}

static void Nop_Edict( edict_t * ) {}

static float VecToYaw( const float *v )
{
	if ( v[1] == 0 && v[0] == 0 )
		return 0;
	float yaw = atan2f( v[1], v[0] ) * 180 / M_PI;
	if ( yaw < 0 )
		yaw += 360;
	return yaw;
}

static void VecToAngles( const float *in, float *out )
{
	float yaw, pitch;
	if ( in[1] == 0 && in[0] == 0 )
	{
		yaw = 0;
		pitch = in[2] > 0 ? 90 : 270;
	}
	else
	{
		yaw = atan2f( in[1], in[0] ) * 180 / M_PI;
		if ( yaw < 0 )
			yaw += 360;
		float forward = sqrtf( in[0] * in[0] + in[1] * in[1] );
		pitch = atan2f( in[2], forward ) * 180 / M_PI;
		if ( pitch < 0 )
			pitch += 360;
	}
	out[0] = pitch;
	out[1] = yaw;
	out[2] = 0;
}

static void MoveToOrigin( edict_t *, const float *, float, int ) {}

static const size_t s_stringFields[][2] = {
	{ (size_t)&( (entvars_t *)0 )->classname, 0 }, { (size_t)&( (entvars_t *)0 )->targetname, 0 },
	{ (size_t)&( (entvars_t *)0 )->target, 0 },    { (size_t)&( (entvars_t *)0 )->netname, 0 },
	{ (size_t)&( (entvars_t *)0 )->message, 0 },   { (size_t)&( (entvars_t *)0 )->model, 0 },
	{ (size_t)&( (entvars_t *)0 )->globalname, 0 },
};
static const char *s_stringFieldNames[] = { "classname", "targetname", "target", "netname", "message", "model",
	"globalname" };

static edict_t *FindEntityByString( edict_t *start, const char *field, const char *value )
{
	if ( !field || !value )
	{
		Stub_Report( "FIND_ENTITY_BY_STRING with a NULL %s", field ? "value" : "field" );
		return &g_stubEdicts[0];
	}
	int f = -1;
	for ( int i = 0; i < (int)( sizeof( s_stringFieldNames ) / sizeof( s_stringFieldNames[0] ) ); i++ )
		if ( !strcmp( field, s_stringFieldNames[i] ) )
			f = i;
	if ( f < 0 )
	{
		Stub_Report( "FIND_ENTITY_BY_STRING: unknown field %s (Sys_Error)", field );
		return &g_stubEdicts[0];
	}
	int i = start ? EdictIndex( start ) + 1 : 0;
	for ( ; i < STUB_MAX_EDICTS; i++ )
	{
		edict_t *e = &g_stubEdicts[i];
		if ( e->free )
			continue;
		int s = *(int *)( (char *)&e->v + s_stringFields[f][0] );
		if ( s && !strcmp( Str( s ), value ) )
			return e;
	}
	return &g_stubEdicts[0];	// the engine returns the world when there is no match (FNullEnt)
}

static int GetEntityIllum( edict_t * ) { return 128; }

static edict_t *FindEntityInSphere( edict_t *start, const float *org, float rad )
{
	int i = start ? EdictIndex( start ) + 1 : 1;
	for ( ; i < STUB_MAX_EDICTS; i++ )
	{
		edict_t *e = &g_stubEdicts[i];
		if ( e->free || !e->pvPrivateData )
			continue;
		if ( i <= g_stubMaxClients && !e->v.netname )
			continue;
		Vector c = ( e->v.absmin + e->v.absmax ) * 0.5f;
		if ( ( c - Vector( org ) ).Length() <= rad )
			return e;
	}
	return &g_stubEdicts[0];
}

static edict_t *FindClientInPVS( edict_t * ) { return &g_stubEdicts[0]; }
static edict_t *EntitiesInPVS( edict_t * ) { return &g_stubEdicts[0]; }

static void AngleVectorsImpl( const float *angles, float *forward, float *right, float *up )
{
	float sr, sp, sy, cr, cp, cy, a;
	a = angles[1] * ( M_PI * 2 / 360 ); sy = sinf( a ); cy = cosf( a );
	a = angles[0] * ( M_PI * 2 / 360 ); sp = sinf( a ); cp = cosf( a );
	a = angles[2] * ( M_PI * 2 / 360 ); sr = sinf( a ); cr = cosf( a );
	if ( forward ) { forward[0] = cp * cy; forward[1] = cp * sy; forward[2] = -sp; }
	if ( right ) { right[0] = -1 * sr * sp * cy + -1 * cr * -sy; right[1] = -1 * sr * sp * sy + -1 * cr * cy; right[2] = -1 * sr * cp; }
	if ( up ) { up[0] = cr * sp * cy + -sr * -sy; up[1] = cr * sp * sy + -sr * cy; up[2] = cr * cp; }
}

static void MakeVectors( const float *v )
{
	AngleVectorsImpl( v, g_stubGlobals.v_forward, g_stubGlobals.v_right, g_stubGlobals.v_up );
}

static edict_t *CreateEntity( void ) { return Stub_AllocEdict(); }
static void RemoveEntity( edict_t *e ) { Stub_FreeEdict( e ); }

static edict_t *CreateNamedEntity( int className )
{
	return Stub_CreateNamed( Str( className ) );
}

static void MakeStatic( edict_t *e ) { Stub_FreeEdict( e ); }
static int EntIsOnFloor( edict_t * ) { return 1; }
static int DropToFloor( edict_t * ) { return 1; }
static int WalkMove( edict_t *, float, float, int ) { return 1; }

static void CheckSound( const char *who, int channel, const char *sample, float volume, float attenuation, int pitch )
{
	if ( !sample )
	{
		Stub_Report( "%s: NULL sample", who );
		return;
	}
	int ivol = (int)( volume * 255 );
	// only NaN / negative: maps use volumes above 1 (func_tracktrain "volume" "50") on stock servers
	if ( !( volume >= 0 ) )
		Stub_Report( "SV_StartSound: volume = %i (NaN or negative) %s", ivol, sample );
	if ( attenuation < 0 || attenuation > 4 )
		Stub_Report( "SV_StartSound: attenuation = %f (out of 0-4) %s", attenuation, sample );
	if ( channel < 0 || channel > 7 )
		Stub_Report( "SV_StartSound: channel = %i (out of 0-7) %s", channel, sample );
	if ( pitch < 0 || pitch > 255 )
		Stub_Report( "SV_StartSound: pitch = %i (out of 0-255) %s", pitch, sample );
	if ( sample[0] != '!' && sample[0] != '#' && !s_sounds.count( Lower( sample ) ) && !MapString( sample ) )
		Stub_Report( "SV_StartSound: %s not precached", sample );
}

static void EmitSound( edict_t *e, int channel, const char *sample, float volume, float attenuation, int, int pitch )
{
	if ( !ValidEdict( e ) )
		Stub_Report( "EMIT_SOUND of a bad edict" );
	CheckSound( "EMIT_SOUND", channel, sample, volume, attenuation, pitch );
}

static void EmitAmbientSound( edict_t *, float *pos, const char *samp, float vol, float attenuation, int, int pitch )
{
	if ( !pos )
		Stub_Report( "EMIT_AMBIENT_SOUND: NULL origin" );
	CheckSound( "EMIT_AMBIENT_SOUND", 0, samp, vol, attenuation, pitch );
}

static void ClearTrace( const float *v2, TraceResult *ptr )
{
	memset( ptr, 0, sizeof( *ptr ) );
	ptr->flFraction = 1.0f;
	ptr->vecEndPos = Vector( v2 );
	ptr->pHit = &g_stubEdicts[0];
	ptr->vecPlaneNormal = Vector( 0, 0, 1 );
	ptr->fInOpen = 1;
}

static void TraceLine( const float *, const float *v2, int, edict_t *, TraceResult *ptr ) { ClearTrace( v2, ptr ); }
static void TraceToss( edict_t *pent, edict_t *, TraceResult *ptr ) { ClearTrace( pent->v.origin, ptr ); }
static int TraceMonsterHull( edict_t *, const float *, const float *v2, int, edict_t *, TraceResult *ptr )
{
	ClearTrace( v2, ptr );
	return 0;
}
static void TraceHull( const float *, const float *v2, int, int, edict_t *, TraceResult *ptr ) { ClearTrace( v2, ptr ); }
static void TraceModel( const float *, const float *v2, int, edict_t *, TraceResult *ptr ) { ClearTrace( v2, ptr ); }
static const char *TraceTexture( edict_t *, const float *, const float * ) { return NULL; }
static void TraceSphere( const float *, const float *v2, int, float, edict_t *, TraceResult *ptr ) { ClearTrace( v2, ptr ); }

static void GetAimVector( edict_t *ent, float, float *ret )
{
	AngleVectorsImpl( ent->v.v_angle, ret, NULL, NULL );
}

static void ServerCommand( char *str )
{
	if ( !str )
		Stub_Report( "SERVER_COMMAND( NULL )" );
	else if ( g_stubVerbose )
		fprintf( stderr, "server command: %s", str );
}
static void ServerExecute( void ) {}
static void ClientCommandEng( edict_t *e, char *fmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	if ( !ValidEdict( e ) || EdictIndex( e ) < 1 || EdictIndex( e ) > g_stubMaxClients )
		Stub_Report( "CLIENT_COMMAND to a non-client (Host_Error): %s", buf );
}
static void ParticleEffect( const float *, const float *, float, float ) {}
static void LightStyle( int style, char *val )
{
	if ( style < 0 || style >= 64 )
		Stub_Report( "PF_lightstyle: style %d (MAX_LIGHTSTYLES 64: memory overwrite in the engine)", style );
	if ( !val )
		Stub_Report( "PF_lightstyle: NULL value" );
	else if ( strlen( val ) >= 64 )
		Stub_Report( "PF_lightstyle: value of %d characters", (int)strlen( val ) );
}
static int DecalIndex( const char * ) { return 1; }
static int PointContents( const float * ) { return CONTENTS_EMPTY; }

//------------------------------------------------------------------------------------------------- messages

struct usermsg_t { std::string name; int size; };
static std::map<int, usermsg_t> s_userMsgs;
static int s_nextUserMsg = 64;

static struct
{
	bool active;
	int dest, type, size;
	edict_t *ed;
} s_msg;

static void MessageBegin( int dest, int type, const float *origin, edict_t *ed )
{
	if ( s_msg.active )
		Stub_Report( "MESSAGE_BEGIN: New message started when msg '%d' has not been sent yet (Sys_Error)", s_msg.type );
	if ( ( dest == MSG_ONE || dest == MSG_ONE_UNRELIABLE ) && !ed )
		Stub_Report( "MESSAGE_BEGIN( MSG_ONE ) without a target (Sys_Error), msg %d", type );
	if ( ( dest == MSG_ONE || dest == MSG_ONE_UNRELIABLE ) && ed
		&& ( EdictIndex( ed ) < 1 || EdictIndex( ed ) > g_stubMaxClients ) )
		Stub_Report( "MESSAGE_BEGIN( MSG_ONE ) to edict %d, not a client, msg %d", EdictIndex( ed ), type );
	if ( ( dest == MSG_PVS || dest == MSG_PAS || dest == MSG_PVS_R || dest == MSG_PAS_R ) && !origin )
		Stub_Report( "MESSAGE_BEGIN( PVS/PAS ) without an origin (Sys_Error), msg %d", type );
	if ( type >= 64 && !s_userMsgs.count( type ) )
		Stub_Report( "MESSAGE_BEGIN: user message %d not registered", type );
	s_msg.active = true;
	s_msg.dest = dest;
	s_msg.type = type;
	s_msg.size = 0;
	s_msg.ed = ed;
}

static void MessageEnd( void )
{
	if ( !s_msg.active )
	{
		Stub_Report( "MESSAGE_END called with no active message (Sys_Error)" );
		return;
	}
	s_msg.active = false;
	if ( s_msg.type < 64 )
		return;
	const usermsg_t &m = s_userMsgs[s_msg.type];
	if ( m.size == -1 )
	{
		if ( s_msg.size > 192 )
			Stub_Report( "MESSAGE_END: Refusing to send user message %s of %i bytes to client, user message size limit is 192 bytes (Host_Error)",
				m.name.c_str(), s_msg.size );
	}
	else if ( m.size != s_msg.size )
		Stub_Report( "MESSAGE_END: User Msg '%s': %d bytes written, expected %d (Sys_Error)", m.name.c_str(), s_msg.size, m.size );
}

static void Write( int n )
{
	if ( !s_msg.active )
		Stub_Report( "WRITE_* outside a message (Sys_Error)" );
	s_msg.size += n;
}
static void WriteByte( int ) { Write( 1 ); }
static void WriteChar( int ) { Write( 1 ); }
static void WriteShort( int ) { Write( 2 ); }
static void WriteLong( int ) { Write( 4 ); }
static void WriteAngle( float ) { Write( 1 ); }
static void WriteCoord( float ) { Write( 2 ); }
static void WriteString( const char *sz )
{
	if ( !sz )
	{
		Stub_Report( "WRITE_STRING( NULL )" );
		sz = "";
	}
	Write( (int)strlen( sz ) + 1 );
}
static void WriteEntity( int ) { Write( 2 ); }

static int RegUserMsg( const char *name, int size )
{
	if ( !name || strlen( name ) > 11 )
		Stub_Report( "REG_USER_MSG: name '%s' longer than 11 characters", name ? name : "(null)" );
	if ( size > 192 )
		Stub_Report( "REG_USER_MSG: %s of size %d", name, size );
	for ( auto &it : s_userMsgs )
		if ( it.second.name == name )
			return it.first;
	if ( s_nextUserMsg > 255 )
	{
		Stub_Report( "REG_USER_MSG: too many user messages" );
		return 0;
	}
	s_userMsgs[s_nextUserMsg] = { name, size };
	return s_nextUserMsg++;
}

//------------------------------------------------------------------------------------------------- cvars

static std::map<std::string, cvar_t *> s_cvars;

static cvar_t *FindCvar( const char *name, bool create )
{
	if ( !name )
	{
		Stub_Report( "cvar lookup with a NULL name" );
		return NULL;
	}
	auto it = s_cvars.find( Lower( name ) );
	if ( it != s_cvars.end() )
		return it->second;
	if ( !create )
		return NULL;
	cvar_t *c = (cvar_t *)calloc( 1, sizeof( cvar_t ) );
	c->name = strdup( name );
	c->string = strdup( "" );
	s_cvars[Lower( name )] = c;
	return c;
}

static void SetCvarString( cvar_t *c, const char *value )
{
	c->string = strdup( value ? value : "" );	// leaked on purpose: the game may keep the old pointer for a while
	c->value = (float)atof( c->string );
}

static void CVarRegister( cvar_t *c )
{
	if ( !c || !c->name )
	{
		Stub_Report( "CVAR_REGISTER( NULL )" );
		return;
	}
	auto it = s_cvars.find( Lower( c->name ) );
	if ( it != s_cvars.end() )
	{
		// an engine cvar the stub made up before the game registered its own: take the game's
		if ( it->second == c )
			Stub_Report( "CVAR_REGISTER: %s registered twice", c->name );
	}
	c->value = (float)atof( c->string );
	s_cvars[Lower( c->name )] = c;
}

static float CVarGetFloat( const char *n ) { cvar_t *c = FindCvar( n, true ); return c ? c->value : 0; }
static const char *CVarGetString( const char *n ) { cvar_t *c = FindCvar( n, true ); return c ? c->string : ""; }
static void CVarSetFloat( const char *n, float v )
{
	char buf[64];
	snprintf( buf, sizeof( buf ), "%g", v );
	if ( cvar_t *c = FindCvar( n, true ) )
		SetCvarString( c, buf );
}
static void CVarSetString( const char *n, const char *v )
{
	if ( cvar_t *c = FindCvar( n, true ) )
		SetCvarString( c, v );
}
static cvar_t *CVarGetPointer( const char *n ) { return FindCvar( n, false ); }
static void CvarDirectSet( cvar_t *c, char *v )
{
	if ( c )
		SetCvarString( c, v );
}

//------------------------------------------------------------------------------------------------- misc

static void AlertMessage( ALERT_TYPE t, char *fmt, ... )
{
	char buf[2048];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	if ( g_stubConsole.size() < ( 1u << 20 ) )
		g_stubConsole += buf;
	if ( g_stubVerbose || t == at_error )
		fprintf( stderr, "%s%s", t == at_error ? "ALERT at_error: " : "", buf );
}

static void EngineFprintf( FILE *f, char *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	if ( f )
		vfprintf( f, fmt, ap );
	va_end( ap );
}

static void *PvAllocEntPrivateData( edict_t *e, long cb )
{
	if ( !ValidEdict( e ) )
	{
		Stub_Report( "ALLOC_PRIVATE of a bad edict" );
		return NULL;
	}
	if ( e->pvPrivateData )
	{
		// the engine frees the old block first (FreeEntPrivateData)
		if ( g_stubHaveNewDll && g_newDll.pfnOnFreeEntPrivateData )
			g_newDll.pfnOnFreeEntPrivateData( e );
		free( e->pvPrivateData );
	}
	e->pvPrivateData = calloc( 1, cb );
	return e->pvPrivateData;
}
static void *PvEntPrivateData( edict_t *e ) { return e ? e->pvPrivateData : NULL; }
static void FreeEntPrivateData( edict_t *e )
{
	if ( e && e->pvPrivateData )
	{
		if ( g_stubHaveNewDll && g_newDll.pfnOnFreeEntPrivateData )
			g_newDll.pfnOnFreeEntPrivateData( e );
		free( e->pvPrivateData );
		e->pvPrivateData = NULL;
	}
}
static const char *SzFromIndex( int i ) { return Str( i ); }
static entvars_t *GetVarsOfEnt( edict_t *e ) { return e ? &e->v : NULL; }
static edict_t *PEntityOfEntOffset( int off ) { return (edict_t *)( (char *)g_stubEdicts + off ); }
static int EntOffsetOfPEntity( const edict_t *e ) { return e ? (int)( (const char *)e - (const char *)g_stubEdicts ) : 0; }
static int IndexOfEdict( const edict_t *e ) { return e ? EdictIndex( e ) : 0; }
static edict_t *PEntityOfEntIndex( int i )
{
	if ( i < 0 || i >= STUB_MAX_EDICTS )
		return NULL;
	edict_t *e = &g_stubEdicts[i];
	if ( i > g_stubMaxClients && ( e->free || !e->pvPrivateData ) )
		return NULL;
	return e;
}
static edict_t *PEntityOfEntIndexAll( int i ) { return ( i < 0 || i >= STUB_MAX_EDICTS ) ? NULL : &g_stubEdicts[i]; }
static edict_t *FindEntityByVars( entvars_t *pev )
{
	for ( int i = 0; i < STUB_MAX_EDICTS; i++ )
		if ( &g_stubEdicts[i].v == pev )
			return &g_stubEdicts[i];
	return NULL;
}
static void *GetModelPtr( edict_t * ) { return NULL; }
static void AnimationAutomove( const edict_t *, float ) {}
static void GetBonePosition( const edict_t *e, int, float *org, float *ang )
{
	if ( org ) { org[0] = e->v.origin.x; org[1] = e->v.origin.y; org[2] = e->v.origin.z; }
	if ( ang ) { ang[0] = e->v.angles.x; ang[1] = e->v.angles.y; ang[2] = e->v.angles.z; }
}
static unsigned long FunctionFromName( const char *name ) { return (unsigned long)dlsym( s_hGame, name ); }
static const char *NameForFunction( unsigned long f )
{
	Dl_info info;
	if ( dladdr( (void *)f, &info ) && info.dli_sname )
		return info.dli_sname;
	return NULL;
}
static void ClientPrintf( edict_t *e, PRINT_TYPE, const char *msg )
{
	if ( !msg )
		Stub_Report( "CLIENT_PRINTF( NULL )" );
	if ( !ValidEdict( e ) || EdictIndex( e ) < 1 || EdictIndex( e ) > g_stubMaxClients )
		Stub_Report( "CLIENT_PRINTF to edict %d, not a client (Host_Error)", EdictIndex( e ) );
}
static void ServerPrint( const char *msg )
{
	if ( !msg )
		Stub_Report( "SERVER_PRINT( NULL )" );
	else
	{
		if ( g_stubConsole.size() < ( 1u << 20 ) )
			g_stubConsole += msg;
		if ( g_stubVerbose )
			fputs( msg, stderr );
	}
}

static std::vector<std::string> s_argv;
static std::string s_args;
void Stub_SetCommand( const std::vector<std::string> &argv, const std::string &args )
{
	s_argv = argv;
	s_args = args;
}
static const char *CmdArgs( void ) { return s_args.c_str(); }
static const char *CmdArgv( int i ) { return ( i >= 0 && i < (int)s_argv.size() ) ? s_argv[i].c_str() : ""; }
static int CmdArgc( void ) { return (int)s_argv.size(); }

static void GetAttachment( const edict_t *e, int, float *org, float *ang )
{
	if ( org ) { org[0] = e->v.origin.x; org[1] = e->v.origin.y; org[2] = e->v.origin.z; }
	if ( ang ) { ang[0] = e->v.angles.x; ang[1] = e->v.angles.y; ang[2] = e->v.angles.z; }
}
static void CRC32_Init( CRC32_t *p ) { *p = 0xffffffff; }
static void CRC32_ProcessBuffer( CRC32_t *p, void *b, int len )
{
	for ( int i = 0; i < len; i++ )
		*p = ( *p >> 8 ) ^ ( ( *p ^ ( (unsigned char *)b )[i] ) & 0xff );
}
static void CRC32_ProcessByte( CRC32_t *p, unsigned char c ) { *p = ( *p >> 8 ) ^ ( ( *p ^ c ) & 0xff ); }
static CRC32_t CRC32_Final( CRC32_t c ) { return ~c; }

static unsigned int s_rand = 12345;
static unsigned int NextRand( void ) { s_rand = s_rand * 1103515245 + 12345; return ( s_rand >> 8 ) & 0xffffff; }
static long RandomLong( long lo, long hi )
{
	if ( hi < lo )
		return lo;
	unsigned long range = (unsigned long)( hi - lo ) + 1;
	if ( !range )
		return lo + (long)NextRand();
	return lo + (long)( NextRand() % range );
}
static float RandomFloat( float lo, float hi ) { return lo + ( hi - lo ) * ( NextRand() / (float)0xffffff ); }
static void SetView( const edict_t *, const edict_t * ) {}
static float Time( void ) { return g_stubGlobals.time; }
static void CrosshairAngle( const edict_t *, float, float ) {}

static byte *LoadFileForMe( char *name, int *len )
{
	if ( len )
		*len = 0;
	if ( !name )
	{
		Stub_Report( "LOAD_FILE_FOR_ME( NULL )" );
		return NULL;
	}
	if ( g_stubGameDir.empty() || strstr( name, ".." ) )
		return NULL;
	std::string path = g_stubGameDir + "/" + name;
	FILE *f = fopen( path.c_str(), "rb" );
	if ( !f )
		return NULL;
	fseek( f, 0, SEEK_END );
	long n = ftell( f );
	fseek( f, 0, SEEK_SET );
	if ( n < 0 )
	{
		fclose( f );
		return NULL;
	}
	// exactly sized, plus the terminating 0 the engine adds (COM_LoadFile)
	byte *buf = (byte *)malloc( n + 1 );
	if ( fread( buf, 1, n, f ) != (size_t)n )
		n = 0;
	buf[n] = 0;
	fclose( f );
	if ( len )
		*len = (int)n;
	return buf;
}
static void FreeFile( void *b ) { free( b ); }
static void EndSection( const char * ) {}
static int CompareFileTime( char *, char *, int *c ) { if ( c ) *c = 0; return 0; }
static void GetGameDir( char *d ) { strcpy( d, "syndicate" ); }
static void FadeClientVolume( const edict_t *, int, int, int, int ) {}
static void SetClientMaxspeed( const edict_t *e, float v ) { const_cast<edict_t *>( e )->v.maxspeed = v; }
static edict_t *CreateFakeClient( const char * ) { return NULL; }
static void RunPlayerMove( edict_t *, const float *, float, float, float, unsigned short, byte, byte ) {}
static int NumberOfEntities( void )
{
	int n = 0;
	for ( int i = 0; i < STUB_MAX_EDICTS; i++ )
		n += !g_stubEdicts[i].free;
	return n;
}

// infobuffers: one per client, "\key\value" pairs, 256 bytes like the engine's MAX_INFO_STRING
static char s_info[33][256];
static char s_serverInfo[256];

char *Stub_InfoBuffer( edict_t *e )
{
	if ( !e )
		return s_serverInfo;
	int i = EdictIndex( e );
	if ( i < 1 || i > g_stubMaxClients )
		return s_serverInfo;
	return s_info[i];
}

static char *GetInfoKeyBuffer( edict_t *e ) { return Stub_InfoBuffer( e ); }

static char *InfoKeyValue( char *buffer, char *key )
{
	static char values[4][256];
	static int which;
	char *out = values[which++ & 3];
	out[0] = 0;
	if ( !buffer || !key )
		return out;
	const char *s = buffer;
	while ( *s == '\\' )
	{
		s++;
		const char *k = s;
		while ( *s && *s != '\\' )
			s++;
		std::string kk( k, s - k );
		if ( *s != '\\' )
			break;
		s++;
		const char *v = s;
		while ( *s && *s != '\\' )
			s++;
		if ( kk == key )
		{
			size_t n = std::min( (size_t)( s - v ), sizeof( values[0] ) - 1 );
			memcpy( out, v, n );
			out[n] = 0;
			return out;
		}
	}
	return out;
}

void Stub_SetInfo( char *buffer, const char *key, const char *value )
{
	// rebuild without the key, then append (the engine's Info_SetValueForKey refuses \ and " in values and cuts
	// over-long buffers)
	std::string out;
	const char *s = buffer;
	while ( *s == '\\' )
	{
		s++;
		const char *k = s;
		while ( *s && *s != '\\' )
			s++;
		std::string kk( k, s - k );
		if ( *s != '\\' )
			break;
		s++;
		const char *v = s;
		while ( *s && *s != '\\' )
			s++;
		if ( kk != key )
			out += "\\" + kk + "\\" + std::string( v, s - v );
	}
	std::string val( value );
	for ( auto &c : val )
		if ( c == '\\' || c == '"' )
			c = ' ';
	out += std::string( "\\" ) + key + "\\" + val;
	if ( out.size() >= 256 )
		return;	// the engine: "Info string length exceeded", the buffer stays as it was
	strcpy( buffer, out.c_str() );
}

static void SetKeyValue( char *buf, char *k, char *v ) { if ( buf && k && v ) Stub_SetInfo( buf, k, v ); }
static void SetClientKeyValue( int i, char *buf, char *k, char *v )
{
	if ( i < 1 || i > g_stubMaxClients )
		Stub_Report( "SET_CLIENT_KEY_VALUE for client %d", i );
	if ( buf && k && v )
		Stub_SetInfo( buf, k, v );
}
static int IsMapValid( char * ) { return 1; }
static void StaticDecal( const float *, int, int, int ) {}
static int GetPlayerUserId( edict_t *e )
{
	int i = EdictIndex( e );
	return ( i >= 1 && i <= g_stubMaxClients ) ? 100 + i : -1;
}
static void BuildSoundMsg( edict_t *, int channel, const char *sample, float volume, float attenuation, int, int pitch,
	int, int, const float *, edict_t * )
{
	CheckSound( "BUILD_SOUND_MSG", channel, sample, volume, attenuation, pitch );
}
static int IsDedicatedServer( void ) { return 1; }
static unsigned int GetPlayerWONId( edict_t * ) { return 0xffffffff; }
static void InfoRemoveKey( char *, const char * ) {}
static const char *GetPhysicsKeyValue( const edict_t *, const char * ) { return ""; }
static void SetPhysicsKeyValue( const edict_t *, const char *, const char * ) {}
static const char *GetPhysicsInfoString( const edict_t * ) { return ""; }
static std::map<std::string, int> s_events;
static unsigned short PrecacheEvent( int, const char *psz )
{
	if ( !psz || !psz[0] )
	{
		Stub_Report( "EV_Precache: NULL pointer or bad string (Host_Error)" );
		return 0;
	}
	auto it = s_events.find( Lower( psz ) );
	if ( it != s_events.end() )
		return (unsigned short)it->second;
	if ( !g_stubLoading )
		Stub_Report( "EV_Precache: '%s' Precache can only be done in spawn functions (Host_Error)", psz );
	s_events[Lower( psz )] = s_iEvents;
	return (unsigned short)s_iEvents++;
}
static void PlaybackEvent( int, const edict_t *, unsigned short idx, float, float *, float *, float, float, int, int, int, int )
{
	if ( idx == 0 || (int)idx >= s_iEvents )
		Stub_Report( "PLAYBACK_EVENT: bad event index %d", idx );
}
static unsigned char s_pvs[1024];
static unsigned char *SetFatPVS( float * ) { memset( s_pvs, 0xff, sizeof( s_pvs ) ); return s_pvs; }
static unsigned char *SetFatPAS( float * ) { memset( s_pvs, 0xff, sizeof( s_pvs ) ); return s_pvs; }
static int CheckVisibility( const edict_t *, unsigned char * ) { return 1; }
static void DeltaSetField( struct delta_s *, const char * ) {}
static void DeltaUnsetField( struct delta_s *, const char * ) {}
static void DeltaAddEncoder( char *, void ( * )( struct delta_s *, const unsigned char *, const unsigned char * ) ) {}
static int GetCurrentPlayer( void ) { return -1; }
static int CanSkipPlayer( const edict_t * ) { return 0; }
static int DeltaFindField( struct delta_s *, const char * ) { return -1; }
static void DeltaSetFieldByIndex( struct delta_s *, int ) {}
static void DeltaUnsetFieldByIndex( struct delta_s *, int ) {}
static void SetGroupMask( int, int ) {}
static int CreateInstancedBaseline( int, struct entity_state_s * ) { return 0; }
static void ForceUnmodified( FORCE_TYPE, float *, float *, const char * ) {}
static void GetPlayerStats( const edict_t *, int *ping, int *loss ) { *ping = 50; *loss = 0; }

static std::vector<std::pair<std::string, void ( * )( void )>> s_serverCommands;
const std::vector<std::pair<std::string, void ( * )( void )>> &Stub_ServerCommands( void ) { return s_serverCommands; }
static void AddServerCommand( char *name, void ( *fn )( void ) ) { s_serverCommands.push_back( { name, fn } ); }
static qboolean VoiceGetClientListening( int, int ) { return 1; }
static qboolean VoiceSetClientListening( int r, int s, qboolean )
{
	if ( r < 1 || r > g_stubMaxClients || s < 1 || s > g_stubMaxClients )
		Stub_Report( "Voice_SetClientListening( %d, %d )", r, s );
	return 1;
}
static const char *GetPlayerAuthId( edict_t *e )
{
	static char buf[64];
	snprintf( buf, sizeof( buf ), "STEAM_0:0:%d", EdictIndex( e ) );
	return buf;
}
static struct sequenceEntry_s *SequenceGet( const char *, const char * ) { return NULL; }
static struct sentenceEntry_s *SequencePickSentence( const char *, int, int *picked ) { if ( picked ) *picked = 0; return NULL; }
static int GetFileSize( char * ) { return -1; }
static unsigned int GetApproxWavePlayLen( const char * ) { return 1000; }
static int IsCareerMatch( void ) { return 0; }
static int GetLocalizedStringLength( const char * ) { return 0; }
static void RegisterTutorMessageShown( int ) {}
static int GetTimesTutorMessageShown( int ) { return 0; }
static void ProcessTutorMessageDecayBuffer( int *, int ) {}
static void ConstructTutorMessageDecayBuffer( int *, int ) {}
static void ResetTutorMessageDecayData( void ) {}
static void QueryClientCvarValue( const edict_t *, const char * ) {}
static void QueryClientCvarValue2( const edict_t *, const char *, int ) {}
static int CheckParm( const char *, char ** ) { return 0; }

void Stub_Init( void *hGameDll, enginefuncs_t *e )
{
	s_hGame = hGameDll;
	g_stubEdicts = (edict_t *)calloc( STUB_MAX_EDICTS, sizeof( edict_t ) );
	for ( int i = 0; i < STUB_MAX_EDICTS; i++ )
	{
		g_stubEdicts[i].free = i > g_stubMaxClients;
		g_stubEdicts[i].v.pContainingEntity = &g_stubEdicts[i];
	}
	g_stubGlobals.pStringBase = s_stringBase;
	g_stubGlobals.maxClients = g_stubMaxClients;
	g_stubGlobals.maxEntities = STUB_MAX_EDICTS;
	g_stubGlobals.time = 1.0f;
	g_stubGlobals.frametime = 0.1f;

	// engine cvars the game reads
	const char *defaults[][2] = { { "deathmatch", "1" }, { "coop", "0" }, { "teamplay", "0" }, { "hostname", "Half-Life" },
		{ "sv_gravity", "800" }, { "sv_maxspeed", "320" }, { "sv_cheats", "0" }, { "sv_aim", "0" },
		{ "motdfile", "motd.txt" }, { "mapcyclefile", "mapcycle.txt" }, { "servercfgfile", "server.cfg" },
		{ "lservercfgfile", "listenserver.cfg" }, { "sv_allowdownload", "1" }, { "sv_downloadurl", "" },
		{ "maxplayers", "8" }, { "sv_maxrate", "0" }, { "sv_stepsize", "18" }, { "sv_friction", "4" } };
	for ( auto &d : defaults )
		SetCvarString( FindCvar( d[0], true ), d[1] );

	memset( e, 0, sizeof( *e ) );
	e->pfnPrecacheModel = PrecacheModel;
	e->pfnPrecacheSound = PrecacheSound;
	e->pfnSetModel = SetModel;
	e->pfnModelIndex = ModelIndex;
	e->pfnModelFrames = ModelFrames;
	e->pfnSetSize = SetSize;
	e->pfnChangeLevel = ChangeLevel;
	e->pfnGetSpawnParms = Nop_Edict;
	e->pfnSaveSpawnParms = Nop_Edict;
	e->pfnVecToYaw = VecToYaw;
	e->pfnVecToAngles = VecToAngles;
	e->pfnMoveToOrigin = MoveToOrigin;
	e->pfnChangeYaw = Nop_Edict;
	e->pfnChangePitch = Nop_Edict;
	e->pfnFindEntityByString = FindEntityByString;
	e->pfnGetEntityIllum = GetEntityIllum;
	e->pfnFindEntityInSphere = FindEntityInSphere;
	e->pfnFindClientInPVS = FindClientInPVS;
	e->pfnEntitiesInPVS = EntitiesInPVS;
	e->pfnMakeVectors = MakeVectors;
	e->pfnAngleVectors = AngleVectorsImpl;
	e->pfnCreateEntity = CreateEntity;
	e->pfnRemoveEntity = RemoveEntity;
	e->pfnCreateNamedEntity = CreateNamedEntity;
	e->pfnMakeStatic = MakeStatic;
	e->pfnEntIsOnFloor = EntIsOnFloor;
	e->pfnDropToFloor = DropToFloor;
	e->pfnWalkMove = WalkMove;
	e->pfnSetOrigin = SetOrigin;
	e->pfnEmitSound = EmitSound;
	e->pfnEmitAmbientSound = EmitAmbientSound;
	e->pfnTraceLine = TraceLine;
	e->pfnTraceToss = TraceToss;
	e->pfnTraceMonsterHull = TraceMonsterHull;
	e->pfnTraceHull = TraceHull;
	e->pfnTraceModel = TraceModel;
	e->pfnTraceTexture = TraceTexture;
	e->pfnTraceSphere = TraceSphere;
	e->pfnGetAimVector = GetAimVector;
	e->pfnServerCommand = ServerCommand;
	e->pfnServerExecute = ServerExecute;
	e->pfnClientCommand = ClientCommandEng;
	e->pfnParticleEffect = ParticleEffect;
	e->pfnLightStyle = LightStyle;
	e->pfnDecalIndex = DecalIndex;
	e->pfnPointContents = PointContents;
	e->pfnMessageBegin = MessageBegin;
	e->pfnMessageEnd = MessageEnd;
	e->pfnWriteByte = WriteByte;
	e->pfnWriteChar = WriteChar;
	e->pfnWriteShort = WriteShort;
	e->pfnWriteLong = WriteLong;
	e->pfnWriteAngle = WriteAngle;
	e->pfnWriteCoord = WriteCoord;
	e->pfnWriteString = WriteString;
	e->pfnWriteEntity = WriteEntity;
	e->pfnCVarRegister = CVarRegister;
	e->pfnCVarGetFloat = CVarGetFloat;
	e->pfnCVarGetString = CVarGetString;
	e->pfnCVarSetFloat = CVarSetFloat;
	e->pfnCVarSetString = CVarSetString;
	e->pfnAlertMessage = AlertMessage;
	e->pfnEngineFprintf = EngineFprintf;
	e->pfnPvAllocEntPrivateData = PvAllocEntPrivateData;
	e->pfnPvEntPrivateData = PvEntPrivateData;
	e->pfnFreeEntPrivateData = FreeEntPrivateData;
	e->pfnSzFromIndex = SzFromIndex;
	e->pfnAllocString = Stub_AllocString;
	e->pfnGetVarsOfEnt = GetVarsOfEnt;
	e->pfnPEntityOfEntOffset = PEntityOfEntOffset;
	e->pfnEntOffsetOfPEntity = EntOffsetOfPEntity;
	e->pfnIndexOfEdict = IndexOfEdict;
	e->pfnPEntityOfEntIndex = PEntityOfEntIndex;
	e->pfnFindEntityByVars = FindEntityByVars;
	e->pfnGetModelPtr = GetModelPtr;
	e->pfnRegUserMsg = RegUserMsg;
	e->pfnAnimationAutomove = AnimationAutomove;
	e->pfnGetBonePosition = GetBonePosition;
	e->pfnFunctionFromName = FunctionFromName;
	e->pfnNameForFunction = NameForFunction;
	e->pfnClientPrintf = ClientPrintf;
	e->pfnServerPrint = ServerPrint;
	e->pfnCmd_Args = CmdArgs;
	e->pfnCmd_Argv = CmdArgv;
	e->pfnCmd_Argc = CmdArgc;
	e->pfnGetAttachment = GetAttachment;
	e->pfnCRC32_Init = CRC32_Init;
	e->pfnCRC32_ProcessBuffer = CRC32_ProcessBuffer;
	e->pfnCRC32_ProcessByte = CRC32_ProcessByte;
	e->pfnCRC32_Final = CRC32_Final;
	e->pfnRandomLong = RandomLong;
	e->pfnRandomFloat = RandomFloat;
	e->pfnSetView = SetView;
	e->pfnTime = Time;
	e->pfnCrosshairAngle = CrosshairAngle;
	e->pfnLoadFileForMe = LoadFileForMe;
	e->pfnFreeFile = FreeFile;
	e->pfnEndSection = EndSection;
	e->pfnCompareFileTime = CompareFileTime;
	e->pfnGetGameDir = GetGameDir;
	e->pfnCvar_RegisterVariable = CVarRegister;
	e->pfnFadeClientVolume = FadeClientVolume;
	e->pfnSetClientMaxspeed = SetClientMaxspeed;
	e->pfnCreateFakeClient = CreateFakeClient;
	e->pfnRunPlayerMove = RunPlayerMove;
	e->pfnNumberOfEntities = NumberOfEntities;
	e->pfnGetInfoKeyBuffer = GetInfoKeyBuffer;
	e->pfnInfoKeyValue = InfoKeyValue;
	e->pfnSetKeyValue = SetKeyValue;
	e->pfnSetClientKeyValue = SetClientKeyValue;
	e->pfnIsMapValid = IsMapValid;
	e->pfnStaticDecal = StaticDecal;
	e->pfnPrecacheGeneric = PrecacheGeneric;
	e->pfnGetPlayerUserId = GetPlayerUserId;
	e->pfnBuildSoundMsg = BuildSoundMsg;
	e->pfnIsDedicatedServer = IsDedicatedServer;
	e->pfnCVarGetPointer = CVarGetPointer;
	e->pfnGetPlayerWONId = GetPlayerWONId;
	e->pfnInfo_RemoveKey = InfoRemoveKey;
	e->pfnGetPhysicsKeyValue = GetPhysicsKeyValue;
	e->pfnSetPhysicsKeyValue = SetPhysicsKeyValue;
	e->pfnGetPhysicsInfoString = GetPhysicsInfoString;
	e->pfnPrecacheEvent = PrecacheEvent;
	e->pfnPlaybackEvent = PlaybackEvent;
	e->pfnSetFatPVS = SetFatPVS;
	e->pfnSetFatPAS = SetFatPAS;
	e->pfnCheckVisibility = CheckVisibility;
	e->pfnDeltaSetField = DeltaSetField;
	e->pfnDeltaUnsetField = DeltaUnsetField;
	e->pfnDeltaAddEncoder = DeltaAddEncoder;
	e->pfnGetCurrentPlayer = GetCurrentPlayer;
	e->pfnCanSkipPlayer = CanSkipPlayer;
	e->pfnDeltaFindField = DeltaFindField;
	e->pfnDeltaSetFieldByIndex = DeltaSetFieldByIndex;
	e->pfnDeltaUnsetFieldByIndex = DeltaUnsetFieldByIndex;
	e->pfnSetGroupMask = SetGroupMask;
	e->pfnCreateInstancedBaseline = CreateInstancedBaseline;
	e->pfnCvar_DirectSet = CvarDirectSet;
	e->pfnForceUnmodified = ForceUnmodified;
	e->pfnGetPlayerStats = GetPlayerStats;
	e->pfnAddServerCommand = AddServerCommand;
	e->pfnVoice_GetClientListening = VoiceGetClientListening;
	e->pfnVoice_SetClientListening = VoiceSetClientListening;
	e->pfnGetPlayerAuthId = GetPlayerAuthId;
	e->pfnSequenceGet = SequenceGet;
	e->pfnSequencePickSentence = SequencePickSentence;
	e->pfnGetFileSize = GetFileSize;
	e->pfnGetApproxWavePlayLen = GetApproxWavePlayLen;
	e->pfnIsCareerMatch = IsCareerMatch;
	e->pfnGetLocalizedStringLength = GetLocalizedStringLength;
	e->pfnRegisterTutorMessageShown = RegisterTutorMessageShown;
	e->pfnGetTimesTutorMessageShown = GetTimesTutorMessageShown;
	e->ProcessTutorMessageDecayBuffer = ProcessTutorMessageDecayBuffer;
	e->ConstructTutorMessageDecayBuffer = ConstructTutorMessageDecayBuffer;
	e->ResetTutorMessageDecayData = ResetTutorMessageDecayData;
	e->pfnQueryClientCvarValue = QueryClientCvarValue;
	e->pfnQueryClientCvarValue2 = QueryClientCvarValue2;
	e->pfnCheckParm = CheckParm;
	e->pfnPEntityOfEntIndexAllEntities = PEntityOfEntIndexAll;

	// every slot set: a NULL one would be a function the stub forgot
	void **slots = (void **)e;
	for ( size_t i = 0; i < sizeof( *e ) / sizeof( void * ); i++ )
		if ( !slots[i] )
		{
			fprintf( stderr, "stub engine: enginefuncs_t slot %u not set\n", (unsigned)i );
			abort();
		}
}
