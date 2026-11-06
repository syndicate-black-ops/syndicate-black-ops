//=========================================================
// SBO 2.0: weapon hand switch (cl_righthand). Ported from OpenAG (commits 8df40ea, ebf8d34, 49b27b2: the cvar, the
// view model mirror in StudioSetupBones, the shell flip in EV_GetDefaultShellInfo); SBO adds the userinfo key for
// the server (RPG rocket side), the watched player's hand in first-person spectating and the "_lh" skin family
// for the view models' lettering. docs/WEAPON_HAND.md.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "weapon_hand.h"

#include <string.h>
#include <stdlib.h>

#include "pm_shared.h"	// OBS_IN_EYE

static cvar_t *cl_righthand = NULL;

void WeaponHand_Init( void )
{
	// FCVAR_USERINFO: the engine puts it in the userinfo (as cl_lw, cl_autowepswitch), so the server knows the hand
	// for the RPG rocket and the other clients for first-person spectating
	cl_righthand = CVAR_CREATE( "cl_righthand", "0", FCVAR_ARCHIVE | FCVAR_USERINFO );
}

bool WeaponHand_LocalLeft( void )
{
	// > 0, the engine's own condition (OpenAG ebf8d34)
	return cl_righthand && cl_righthand->value > 0.0f;
}

bool WeaponHand_PlayerLeft( int playerIndex )
{
	if ( playerIndex < 1 || playerIndex > gEngfuncs.GetMaxClients() || !gEngfuncs.PlayerInfo_ValueForKey )
		return false;

	const char *value = gEngfuncs.PlayerInfo_ValueForKey( playerIndex, "cl_righthand" );
	return value && *value && atof( value ) > 0.0;
}

bool WeaponHand_ViewLeft( void )
{
	if ( !HUD_IsHardware() )
		return false;	// the software renderer has no mirror (StudioSetupBones), nor do the shells

	// first-person spectating, main view or inset (ev_common.cpp IS_FIRSTPERSON_SPEC): the view model is the
	// watched player's weapon
	if ( g_iUser1 == OBS_IN_EYE || ( g_iUser1 && gHUD.m_Spectator.m_pip && gHUD.m_Spectator.m_pip->value == INSET_IN_EYE ) )
		return WeaponHand_PlayerLeft( g_iUser2 );

	return WeaponHand_LocalLeft();
}

// name is base with "_lh" before the extension (or at the end when base has none), compared without case
static bool IsLeftHandName( const char *name, const char *base )
{
	const char *dot = strrchr( base, '.' );
	size_t stem = dot ? (size_t)( dot - base ) : strlen( base );

	if ( strnicmp( name, base, stem ) != 0 || strnicmp( name + stem, "_lh", 3 ) != 0 )
		return false;

	return stricmp( name + stem + 3, base + stem ) == 0;
}

int WeaponHand_MirroredSkin( const studiohdr_t *hdr, int skin )
{
	if ( !hdr || !hdr->textureindex || hdr->numskinfamilies < 2 || ( hdr->numskinfamilies & 1 ) || hdr->numskinref <= 0 )
		return skin;

	const int families = hdr->numskinfamilies / 2;
	if ( skin < 0 || skin >= families )
		return skin;

	const mstudiotexture_t *textures = (const mstudiotexture_t *)( (const byte *)hdr + hdr->textureindex );
	const short *skins = (const short *)( (const byte *)hdr + hdr->skinindex );
	bool found = false;

	for ( int f = families; f < hdr->numskinfamilies; f++ )
	{
		for ( int j = 0; j < hdr->numskinref; j++ )
		{
			const int a = skins[ ( f - families ) * hdr->numskinref + j ];
			const int b = skins[ f * hdr->numskinref + j ];

			if ( a == b )
				continue;
			if ( a < 0 || a >= hdr->numtextures || b < 0 || b >= hdr->numtextures )
				return skin;
			if ( !IsLeftHandName( textures[ b ].name, textures[ a ].name ) )
				return skin;	// a skin family of another kind: not an "_lh" model
			found = true;
		}
	}

	return found ? skin + families : skin;
}
