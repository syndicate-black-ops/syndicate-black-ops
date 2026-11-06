//=========================================================
// SBO 2.0 object hints, client side: see objecthint.h, game_shared/objecthint_shared.h and docs/OBJECT_HINTS.md.
//
// Ported from Featureful (FreeSlave/halflife-featureful 30fc2f7: cl_dll/objecthint_manager.cpp, hud_msg.cpp
// MsgFunc_ObjectHint, cvar cl_objecthint). Each hint is a glow sprite (additive, drawn over the world, the same size on
// screen at any distance) on the object; it follows the object as it moves and ends OBJECTHINT_LIFE after the
// server's last refresh. One interaction hint (the object +use would use) and up to OBJECTHINT_CLIENT_MAX others.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_efx.h"
#include "event_api.h"
#include "objecthint.h"
#include "objecthint_shared.h"

#include <string.h>

#define OBJECTHINT_CLIENT_MAX	64

struct client_objecthint_t
{
	TEMPENTITY	*te;		// NULL: free
	int			entindex;
	float		expire;		// client time
	int			useHudColor;
	Vector		offset;		// object origin - hint position
};

static client_objecthint_t s_Interactable;
static client_objecthint_t s_Hints[ OBJECTHINT_CLIENT_MAX ];
static cvar_t *s_pCvarObjectHint = NULL;

struct objecthint_sprite_t
{
	char	name[ OBJECTHINT_MAX_SPRITE ];
	model_t	*model;
};

#define OBJECTHINT_MAX_SPRITES	8
static objecthint_sprite_t s_Sprites[ OBJECTHINT_MAX_SPRITES ];
static int s_iSprites = 0;

static void HudColor( color24 &c )
{
	int r, g, b;
	UnpackRGB( r, g, b, RGB_YELLOWISH );
	c.r = r; c.g = g; c.b = b;
}

// Featureful EnsureSpriteLoaded: one SPR_Load per sprite name and map
static model_t *ObjectHint_LoadSprite( const char *name )
{
	for ( int i = 0; i < s_iSprites; i++ )
	{
		if ( !strcmp( s_Sprites[i].name, name ) )
			return s_Sprites[i].model;
	}
	if ( s_iSprites >= OBJECTHINT_MAX_SPRITES )
		return NULL;

	objecthint_sprite_t &s = s_Sprites[ s_iSprites++ ];
	strncpy( s.name, name, sizeof( s.name ) - 1 );
	s.name[ sizeof( s.name ) - 1 ] = 0;
	HSPRITE h = SPR_Load( name );
	s.model = h ? (model_t *)gEngfuncs.GetSpritePointer( h ) : NULL;
	return s.model;
}

// a sprite path the server may name: "sprites/....spr", no "..", no drive or absolute path
static bool ObjectHint_SpriteNameOk( const char *name )
{
	size_t len = strlen( name );
	if ( len < 12 || len >= OBJECTHINT_MAX_SPRITE )
		return false;
	if ( strncmp( name, "sprites/", 8 ) || strcmp( name + len - 4, ".spr" ) )
		return false;
	if ( strstr( name, ".." ) || strchr( name, ':' ) || strchr( name, '\\' ) )
		return false;
	return true;
}

static void ObjectHint_Kill( client_objecthint_t &h )
{
	if ( h.te )
		h.te->die = gEngfuncs.GetClientTime();
	h.te = NULL;
	h.entindex = 0;
}

static void ObjectHint_Follow( client_objecthint_t &h )
{
	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( h.entindex );
	if ( ent )
		h.te->entity.origin = ent->origin - h.offset;
	if ( h.useHudColor )
		HudColor( h.te->entity.curstate.rendercolor );
}

struct objecthint_msg_t
{
	int		flags;
	int		entindex;
	color24	color;
	float	scaleFactor;
	Vector	center;
	Vector	size;
	char	sprite[ OBJECTHINT_MAX_SPRITE ];
};

// Featureful UpdateObjectHintParams
static void ObjectHint_SetParams( client_objecthint_t &h, const objecthint_msg_t &m )
{
	TEMPENTITY *te = h.te;
	h.useHudColor = ( m.color.r == 0 && m.color.g == 0 && m.color.b == 0 );
	if ( h.useHudColor )
		HudColor( te->entity.curstate.rendercolor );
	else
		te->entity.curstate.rendercolor = m.color;

	te->entity.curstate.scale = m.size.Length() * 0.015625f * m.scaleFactor;

	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( m.entindex );
	h.offset = ent ? ent->origin - m.center : Vector( 0, 0, 0 );
	te->entity.origin = m.center;
	h.expire = gEngfuncs.GetClientTime() + OBJECTHINT_LIFE;
	te->die = h.expire + 1.0f;
}

// Featureful SetHint
static void ObjectHint_Set( const objecthint_msg_t &m )
{
	const bool interactable = ( m.flags & OBJECTHINT_FLAG_CLOSEST ) != 0;

	client_objecthint_t *pExisting = NULL;
	if ( s_Interactable.te && s_Interactable.entindex == m.entindex )
		pExisting = &s_Interactable;
	for ( int i = 0; !pExisting && i < OBJECTHINT_CLIENT_MAX; i++ )
	{
		if ( s_Hints[i].te && s_Hints[i].entindex == m.entindex )
			pExisting = &s_Hints[i];
	}

	if ( pExisting )
	{
		if ( pExisting->te->entity.model && !strcmp( pExisting->te->entity.model->name, m.sprite ) )
		{
			ObjectHint_SetParams( *pExisting, m );
			return;
		}
		ObjectHint_Kill( *pExisting );
	}

	model_t *model = ObjectHint_LoadSprite( m.sprite );
	if ( !model )
		return;

	client_objecthint_t *pSlot = NULL;
	if ( interactable )
	{
		ObjectHint_Kill( s_Interactable );
		pSlot = &s_Interactable;
	}
	else
	{
		for ( int i = 0; !pSlot && i < OBJECTHINT_CLIENT_MAX; i++ )
		{
			if ( !s_Hints[i].te )
				pSlot = &s_Hints[i];
		}
		if ( !pSlot )
			return;
	}

	Vector pos = m.center;
	TEMPENTITY *te = gEngfuncs.pEfxAPI->CL_TempEntAlloc( pos, model );
	if ( !te )
		return;

	te->flags |= FTENT_PERSIST;
	te->entity.curstate.rendermode = kRenderGlow;
	te->entity.curstate.renderfx = kRenderFxNoDissipation;
	te->entity.curstate.renderamt = 255;

	pSlot->te = te;
	pSlot->entindex = m.entindex;
	ObjectHint_SetParams( *pSlot, m );
}

static int MsgFunc_ObjectHint( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	objecthint_msg_t m;
	m.flags = READ_BYTE();
	m.entindex = READ_SHORT();

	if ( m.entindex > 0 )
	{
		m.color.r = READ_BYTE();
		m.color.g = READ_BYTE();
		m.color.b = READ_BYTE();
		m.scaleFactor = READ_COORD();
		m.center.x = READ_COORD();
		m.center.y = READ_COORD();
		m.center.z = READ_COORD();
		m.size.x = READ_COORD();
		m.size.y = READ_COORD();
		m.size.z = READ_COORD();
		const char *sprite = READ_STRING();
		strncpy( m.sprite, sprite ? sprite : "", sizeof( m.sprite ) - 1 );
		m.sprite[ sizeof( m.sprite ) - 1 ] = 0;

		if ( !READ_OK() || !ObjectHint_SpriteNameOk( m.sprite ) || !( m.scaleFactor > 0 ) || m.scaleFactor > 64 )
			return 1;

		// Featureful: cl_objecthint 0 none, 1 all, 2 the interaction hint only; hud_draw 0 none
		const float mode = s_pCvarObjectHint ? s_pCvarObjectHint->value : 1;
		if ( mode && gHUD.m_pCvarDraw->value )
		{
			const bool interactable = ( m.flags & OBJECTHINT_FLAG_CLOSEST ) != 0;
			if ( mode == 2 ? interactable : true )
				ObjectHint_Set( m );
		}
	}
	else if ( m.flags & OBJECTHINT_FLAG_CLOSEST )
	{
		ObjectHint_Kill( s_Interactable );
	}
	return 1;
}

void ObjectHint_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "ObjectHint", MsgFunc_ObjectHint );
	s_pCvarObjectHint = CVAR_CREATE( "cl_objecthint", "1", FCVAR_ARCHIVE );
}

void ObjectHint_Reset( void )
{
	// the engine freed every temporary entity with the old map: forget them, do not touch them
	memset( &s_Interactable, 0, sizeof( s_Interactable ) );
	memset( s_Hints, 0, sizeof( s_Hints ) );
	s_iSprites = 0;
}

// Featureful ObjectHintManager::Update
void ObjectHint_CreateEntities( void )
{
	const float clientTime = gEngfuncs.GetClientTime();
	const bool draw = gHUD.m_pCvarDraw->value != 0 && ( !s_pCvarObjectHint || s_pCvarObjectHint->value != 0 );

	if ( s_Interactable.te )
	{
		if ( !draw || s_Interactable.expire <= clientTime )
			ObjectHint_Kill( s_Interactable );
		else
			ObjectHint_Follow( s_Interactable );
	}
	for ( int i = 0; i < OBJECTHINT_CLIENT_MAX; i++ )
	{
		client_objecthint_t &h = s_Hints[i];
		if ( !h.te )
			continue;
		if ( !draw || h.expire <= clientTime )
			ObjectHint_Kill( h );
		else
			ObjectHint_Follow( h );
	}
}
