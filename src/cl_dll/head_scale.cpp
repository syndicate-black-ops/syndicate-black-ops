//=========================================================
// SBO 2.0 player head scale, client side: see head_scale.h and dlls/head_scale.cpp.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "head_scale.h"
#include "studio.h"
#include "head_scale_shared.h"

#define HEADSCALE_CLIENT_MAX	32

static float s_flHeadScale[ HEADSCALE_CLIENT_MAX + 1 ];

static int MsgFunc_HeadScale( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iPlayer = READ_BYTE();
	float flScale = (unsigned short)READ_SHORT() / HEADSCALE_UNIT;

	if ( READ_OK() == false || iPlayer < 1 || iPlayer > HEADSCALE_CLIENT_MAX )
		return 1;

	if ( !( flScale >= HEADSCALE_MIN ) || !( flScale <= HEADSCALE_MAX ) )
		flScale = 1.0f;
	s_flHeadScale[iPlayer] = flScale;
	return 1;
}

void HeadScale_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "HeadScale", MsgFunc_HeadScale );
	HeadScale_Reset();
}

void HeadScale_Reset( void )
{
	for ( int i = 0; i <= HEADSCALE_CLIENT_MAX; i++ )
		s_flHeadScale[i] = 1.0f;
}

float HeadScale_Get( int iPlayer )
{
	if ( iPlayer < 1 || iPlayer > HEADSCALE_CLIENT_MAX )
		return 1.0f;
	return s_flHeadScale[iPlayer];
}
