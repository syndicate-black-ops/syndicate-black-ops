//=========================================================
// env_static -- a decorative studio model drawn by the clients only: no model precache slot, no entity updates,
// and no edict for a prop without a name. Not solid (a clip brush gives collision where needed).
//
// The idea and the keys are XDM's env_static (Xawari, XDM source dlls/environment.cpp CEnvStatic and
// cl_dll/cl_msg_fx.cpp __MsgFunc_StaticEnt; Half-Life 1 SDK license per the owner): the server sends each prop in a
// message, the client loads the model by name. The code is written for SBO (XDM keeps an edict per prop and draws
// with its own render system). docs/ENV_STATIC.md.
//
//   * Map load (Spawn): the model is checked (a studio model file of version 10) and offered for download with
//     PRECACHE_GENERIC (plus its texture file and sequence groups), never PRECACHE_MODEL. The prop goes into a
//     table; a prop without a targetname then removes its edict at once (the slot is reused by the next entity).
//   * The clients get the table after their HUD is (re)initialised, a few props at a time (EnvStatic_SendQueued,
//     client_sendqueue.h, like env_particle), so a map with hundreds of props never fills a
//     client's reliable channel. A named prop's "Use" shows or hides it for everybody (one message).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"studio.h"
#include	"static_props_shared.h"
#include	"client_sendqueue.h"

extern int gmsgStaticProp;

#define SF_ENVSTATIC_START_INVISIBLE	2	// XDM

#define ENVSTATIC_QUEUE_DELAY		0.5f	// after the client's InitHUD, before its first props
#define ENVSTATIC_SEND_INTERVAL		0.1f
#define ENVSTATIC_QUEUE_BYTES		600		// props to one client per ENVSTATIC_SEND_INTERVAL (bytes)

struct static_model_t
{
	string_t	name;
	BOOL		valid;
	int			firstProp;	// the first prop with this model: its name is sent before it
};

struct static_prop_t
{
	int		model;		// index in s_Models
	Vector	origin;
	Vector	angles;
	int		rendermode, renderfx, renderamt;
	Vector	rendercolor;
	int		body, skin, sequence;
	float	scale, framerate;
	BOOL	visible;
};

static static_model_t s_Models[ STATICPROP_MAX_MODELS ];
static int s_iModels;
static static_prop_t s_Props[ STATICPROP_MAX_PROPS ];
static int s_iProps;
static BOOL s_bFullNoted;

// a new map: no props (Spawn of the world, before any env_static)
void EnvStatic_Reset( void )
{
	s_iModels = 0;
	s_iProps = 0;
	s_bFullNoted = FALSE;
}

static int Clamp( int v, int lo, int hi ) { return v < lo ? lo : ( v > hi ? hi : v ); }

// The model file: a studio model of version 10 that the engine would load. Offered for download with its texture
// file (models without textures keep them in <name>T.mdl) and its sequence groups (<name>01.mdl ...).
static BOOL EnvStatic_CheckModel( const char *szModel )
{
	int length = 0;
	byte *pFile = LOAD_FILE_FOR_ME( (char *)szModel, &length );
	if ( !pFile )
	{
		ALERT( at_console, "env_static: model \"%s\" not found\n", szModel );
		return FALSE;
	}

	BOOL ok = FALSE;
	int numtextures = 0, numseqgroups = 0;

	if ( length >= (int)sizeof( studiohdr_t ) )
	{
		studiohdr_t *pHdr = (studiohdr_t *)pFile;
		if ( !memcmp( &pHdr->id, "IDST", 4 ) && pHdr->version == 10 && pHdr->length > 0 && pHdr->length <= length )
		{
			ok = TRUE;
			numtextures = pHdr->numtextures;
			numseqgroups = pHdr->numseqgroups;
		}
	}
	FREE_FILE( pFile );

	if ( !ok )
	{
		ALERT( at_console, "env_static: \"%s\" is not a studio model the game can load\n", szModel );
		return FALSE;
	}

	PRECACHE_GENERIC( (char *)szModel );

	char base[128];
	size_t n = strlen( szModel );
	if ( n < 4 || n - 4 >= sizeof( base ) - 8 )
		return TRUE;
	memcpy( base, szModel, n - 4 );
	base[ n - 4 ] = '\0';

	char extra[ sizeof( base ) ];
	if ( numtextures == 0 )
	{
		snprintf( extra, sizeof( extra ), "%sT.mdl", base );
		PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( extra ) ) );
	}
	for ( int i = 1; i < numseqgroups && i < MAXSTUDIOGROUPS; i++ )
	{
		snprintf( extra, sizeof( extra ), "%s%02d.mdl", base, i );
		PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( extra ) ) );
	}
	return TRUE;
}

// TRUE if an earlier prop already checked this model and found it usable
static BOOL EnvStatic_ModelKnownGood( string_t iszModel )
{
	for ( int i = 0; i < s_iModels; i++ )
	{
		if ( !stricmp( STRING( s_Models[i].name ), STRING( iszModel ) ) )
			return s_Models[i].valid;
	}
	return FALSE;
}

// the model's index in s_Models, -1 if the model can't be used
static int EnvStatic_Model( string_t iszModel )
{
	for ( int i = 0; i < s_iModels; i++ )
	{
		if ( !stricmp( STRING( s_Models[i].name ), STRING( iszModel ) ) )
			return s_Models[i].valid ? i : -1;
	}

	if ( s_iModels >= STATICPROP_MAX_MODELS )
	{
		ALERT( at_console, "env_static: more than %d different models, \"%s\" left out\n", STATICPROP_MAX_MODELS, STRING( iszModel ) );
		return -1;
	}

	static_model_t *m = &s_Models[ s_iModels ];
	m->name = iszModel;
	m->firstProp = -1;
	m->valid = EnvStatic_CheckModel( STRING( iszModel ) );
	s_iModels++;
	return m->valid ? s_iModels - 1 : -1;
}

static void EnvStatic_WriteModel( int iModel )
{
	WRITE_BYTE( STATICPROP_MODEL );
	WRITE_BYTE( iModel );
	WRITE_STRING( STRING( s_Models[ iModel ].name ) );
}

static void EnvStatic_WriteProp( int iProp )
{
	const static_prop_t *p = &s_Props[ iProp ];

	WRITE_BYTE( STATICPROP_PROP );
	WRITE_SHORT( iProp );
	WRITE_BYTE( p->model );
	for ( int i = 0; i < 3; i++ )
		WRITE_LONG( (int)( p->origin[i] * 8.0f ) );
	for ( int i = 0; i < 3; i++ )
		WRITE_ANGLE( p->angles[i] );
	WRITE_BYTE( Clamp( p->rendermode, 0, 255 ) );
	WRITE_BYTE( Clamp( p->renderfx, 0, 255 ) );
	for ( int i = 0; i < 3; i++ )
		WRITE_BYTE( Clamp( (int)p->rendercolor[i], 0, 255 ) );
	WRITE_BYTE( Clamp( p->renderamt, 0, 255 ) );
	WRITE_BYTE( Clamp( p->body, 0, 255 ) );
	WRITE_BYTE( Clamp( p->skin, 0, 255 ) );
	WRITE_SHORT( Clamp( p->sequence, 0, 32767 ) );
	WRITE_SHORT( Clamp( (int)( p->scale * 256.0f ), 0, 32767 ) );
	WRITE_SHORT( Clamp( (int)( p->framerate * 256.0f ), -32768, 32767 ) );
	WRITE_BYTE( p->visible ? 1 : 0 );
}

//=========================================================

class CEnvStatic : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int m_iProp;	// index in s_Props, -1 none
};

LINK_ENTITY_TO_CLASS( env_static, CEnvStatic );

void CEnvStatic::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "type" ) )	// XDM's detail type: read, not used
		pkvd->fHandled = TRUE;
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvStatic::Spawn( void )
{
	m_iProp = -1;

	// SBO 2.0: no model, the editor placeholder (models/editor/, the FGD default) or a file the server doesn't have is
	// never offered for download (once per model: a model an earlier prop checked is not loaded again)
	if ( !EnvStatic_ModelKnownGood( pev->model ) && UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	const char *szModel = STRING( pev->model );
	size_t n = szModel ? strlen( szModel ) : 0;

	if ( n < 5 || stricmp( szModel + n - 4, ".mdl" ) )
	{
		ALERT( at_console, "env_static at (%.0f %.0f %.0f) without a studio model (\"%s\"): removed\n", pev->origin.x, pev->origin.y, pev->origin.z, szModel ? szModel : "" );
		pev->flags |= FL_KILLME;
		return;
	}

	int iModel = EnvStatic_Model( pev->model );

	if ( iModel >= 0 && s_iProps < STATICPROP_MAX_PROPS )
	{
		static_prop_t *p = &s_Props[ s_iProps ];
		p->model = iModel;
		p->origin = pev->origin;
		p->angles = pev->angles;
		p->rendermode = (int)pev->rendermode;
		p->renderfx = (int)pev->renderfx;
		p->renderamt = (int)pev->renderamt;
		p->rendercolor = pev->rendercolor;
		p->body = pev->body;
		p->skin = pev->skin;
		p->sequence = pev->sequence;
		p->scale = pev->scale;
		p->framerate = pev->framerate;
		p->visible = !( pev->spawnflags & SF_ENVSTATIC_START_INVISIBLE );
		if ( s_Models[ iModel ].firstProp < 0 )
			s_Models[ iModel ].firstProp = s_iProps;
		m_iProp = s_iProps++;
	}
	else if ( iModel >= 0 && !s_bFullNoted )
	{
		ALERT( at_console, "env_static: more than %d props, the others are left out\n", STATICPROP_MAX_PROPS );
		s_bFullNoted = TRUE;
	}

	// nothing for the engine to send: no model, not solid, not drawn
	pev->model = iStringNull;
	pev->modelindex = 0;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	// a prop nobody can trigger needs no edict: the table has it
	if ( FStringNull( pev->targetname ) || m_iProp < 0 )
		pev->flags |= FL_KILLME;
}

// XDM: On shows, Off hides, Toggle toggles; for everybody
void CEnvStatic::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_iProp < 0 || m_iProp >= s_iProps )
		return;

	static_prop_t *p = &s_Props[ m_iProp ];
	if ( !ShouldToggle( useType, p->visible ) )
		return;

	p->visible = !p->visible;

	if ( !gmsgStaticProp )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgStaticProp );
		WRITE_BYTE( STATICPROP_VISIBLE );
		WRITE_SHORT( m_iProp );
		WRITE_BYTE( p->visible ? 1 : 0 );
	MESSAGE_END();
}

//=========================================================
// Sending to a client
//=========================================================

// the client's HUD was (re)initialised (client_sendqueue.cpp): the cursor is the next prop (-1: the CLEAR first)
static int EnvStatic_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	if ( !gmsgStaticProp || !s_iProps )
		return TRUE;

	if ( iCursor < 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStaticProp, NULL, pPlayer->edict() );
			WRITE_BYTE( STATICPROP_CLEAR );
		MESSAGE_END();
		iCursor = 0;
		iBudget -= 4;
	}

	for ( int i = iCursor; i < s_iProps; i++ )
	{
		const int model = s_Props[i].model;

		// the model's name goes before the first prop that uses it (the client forgot all with the CLEAR)
		const BOOL bFirstOfModel = s_Models[ model ].firstProp == i;

		const int iSize = STATICPROP_PROP_BYTES + ( bFirstOfModel ? 8 + (int)strlen( STRING( s_Models[ model ].name ) ) : 0 );
		if ( iSize > iBudget && iBudget < ENVSTATIC_QUEUE_BYTES )
		{
			iCursor = i;
			return FALSE;
		}

		if ( bFirstOfModel )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStaticProp, NULL, pPlayer->edict() );
				EnvStatic_WriteModel( model );
			MESSAGE_END();
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgStaticProp, NULL, pPlayer->edict() );
			EnvStatic_WriteProp( i );
		MESSAGE_END();

		iBudget -= iSize;
	}

	return TRUE;
}

static SendQueue_Sender s_StaticSender( SENDQUEUE_STATIC, EnvStatic_SendQueued, -1, ENVSTATIC_QUEUE_DELAY,
	ENVSTATIC_SEND_INTERVAL, ENVSTATIC_QUEUE_BYTES );
