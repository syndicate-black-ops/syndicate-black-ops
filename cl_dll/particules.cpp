//
// particules.cpp
//
// SBO: particle emitters (flames, smoke, rain/snow) drawn with the triangle API
//

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "usercmd.h"
#include "pm_defs.h"

#include <stdlib.h>

//=============================================================================
// CParticulesEmetteur (flames)
//=============================================================================

bool CParticulesEmetteur::SetEmetteur( int iNumParticules, int iHeight, Vector vecOrigin, float flSpeed, int iLife, int iSpread, float r, float g, float b, float a, int bActive )
{
	m_iNumParticules = iNumParticules;
	m_iHeight = iHeight;
	m_vecOrigin = vecOrigin;
	m_flSpeed = flSpeed;
	m_iLife = iLife;
	m_iSpread = iSpread;
	m_flR = r;
	m_flG = g;
	m_flB = b;
	m_flA = a;
	VectorClear( m_vecVelocity );

	m_flR = m_flR / 255;
	m_flG = m_flG / 255;
	m_flB = m_flB / 255;
	m_flA = m_flA / 100;

	if ( bActive )
		m_bActive = true;
	else
		m_bActive = false;

	m_pParticules = new Particule[iNumParticules];

	for ( int i = 0; i < iNumParticules; i++ )
	{
		m_pParticules[i].active = true;
		m_pParticules[i].origin.x = gEngfuncs.pfnRandomFloat( -iSpread, iSpread ) + vecOrigin.x;
		m_pParticules[i].origin.y = gEngfuncs.pfnRandomFloat( -iSpread, iSpread ) + vecOrigin.y;
		m_pParticules[i].origin.z = m_iHeight / 2 + vecOrigin.z;
		m_pParticules[i].age = 0;
		m_pParticules[i].speed = gEngfuncs.pfnRandomFloat( -6, 8 ) + m_flSpeed;
		m_pParticules[i].r = m_flR;
		m_pParticules[i].g = m_flG;
		m_pParticules[i].b = m_flB;
		m_pParticules[i].size = iHeight;
		m_pParticules[i].alpha = m_flA;
		m_pParticules[i].active = true;
	}

	m_hSprite = SPR_Load( "sprites/p_beam.spr" );
	m_pNext = NULL;

	return true;
}

void CParticulesEmetteur::Render( double flTime )
{
	double time = flTime;
	Vector point, forward, right, up, angles;
	int i;

	gEngfuncs.GetViewAngles( angles );
	AngleVectors( angles, forward, right, up );

	if ( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( m_hSprite ), 1 ) )
		return;

	for ( i = 0; i < m_iNumParticules; i++ )
	{
		if ( m_pParticules[i].active )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			gEngfuncs.pTriAPI->Color4f( m_pParticules[i].r, m_pParticules[i].g, m_pParticules[i].b, 1.0 );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
			VectorMA( m_pParticules[i].origin, m_pParticules[i].size, up, point );
			VectorMA( point, -m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
			VectorMA( m_pParticules[i].origin, m_pParticules[i].size, up, point );
			VectorMA( point, m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
			VectorMA( m_pParticules[i].origin, -m_pParticules[i].size, up, point );
			VectorMA( point, m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
			VectorMA( m_pParticules[i].origin, -m_pParticules[i].size, up, point );
			VectorMA( point, -m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->End();
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		}
	}
}

void CParticulesEmetteur::InitializeParts( int iParticule )
{
	m_pParticules[iParticule].origin.x = gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + m_vecOrigin.x;
	m_pParticules[iParticule].origin.y = gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + m_vecOrigin.y;
	m_pParticules[iParticule].origin.z = m_iHeight / 2 + m_vecOrigin.z;
	m_pParticules[iParticule].speed = m_flSpeed;
	m_pParticules[iParticule].age = 0;
	m_pParticules[iParticule].r = m_flR;
	m_pParticules[iParticule].g = m_flG;
	m_pParticules[iParticule].b = m_flB;
	m_pParticules[iParticule].size = m_iHeight;
	m_pParticules[iParticule].alpha = m_flA;

	if ( !m_bActive )
		m_pParticules[iParticule].active = false;
}

void CParticulesEmetteur::ChangeState( void )
{
	if ( m_bActive )
		m_bActive = false;
	else
		m_bActive = true;
}

void CParticulesEmetteur::EvolueParticules( double flTime )
{
	int i;

	for ( i = 0; i < m_iNumParticules; i++ )
	{
		if ( m_pParticules[i].active )
		{
			m_pParticules[i].origin.z += m_pParticules[i].speed * flTime;
			m_pParticules[i].age += flTime * 90 + (int)gEngfuncs.pfnRandomFloat( -40, 75 );

			m_pParticules[i].size = m_iHeight - ( m_iHeight * m_pParticules[i].age ) / m_iLife;
			if ( m_pParticules[i].size <= m_iHeight / 2 )
				m_pParticules[i].size = m_iHeight / 2;
			if ( m_pParticules[i].size >= m_iHeight )
				m_pParticules[i].size = m_iHeight;

			m_pParticules[i].alpha = m_flA - ( m_pParticules[i].age * m_flA ) / m_iLife;
			if ( m_pParticules[i].alpha <= 0 )
				m_pParticules[i].alpha = 0;
			if ( m_pParticules[i].alpha >= m_flA )
				m_pParticules[i].alpha = m_flA;
		}
		else if ( m_bActive )
		{
			m_pParticules[i].active = true;
		}

		if ( m_pParticules[i].age >= m_iLife )
			InitializeParts( i );
	}
}

CParticulesEmetteur::~CParticulesEmetteur()
{
	if ( m_pParticules )
		delete m_pParticules;
	if ( m_pNext )
		delete m_pNext;
}

//=============================================================================
// CSmokeEmit (smoke)
//=============================================================================

bool CSmokeEmit::SetEmetteur( int iNumParticules, int iHeight, Vector vecOrigin, float flSpeed, int iLife, int iParam8, int iParam9, int iParam10, int iParam11, int iParam12, int iParam13, int iParam14, int iParam15, int iParam16, int iParam17, int bActive )
{
	m_iNumParticules = iNumParticules;
	m_iHeight = iHeight;
	m_vecOrigin = vecOrigin;
	m_flSpeed = flSpeed;
	m_iLife = iLife;
	m_iParam8 = iParam8;
	VectorClear( m_vecVelocity );
	m_iR = iParam9;
	m_iG = iParam10;
	m_iB = iParam11;
	m_iParam12 = iParam12;
	m_iParam13 = iParam13;
	m_iParam14 = iParam14;
	m_flWindX = iParam15;
	m_flWindX = m_flWindX / 10;
	m_flWindY = iParam16;
	m_flWindY = m_flWindY / 10;
	m_flAlpha = iParam17;
	m_flAlpha = m_flAlpha / 100;

	if ( bActive )
		m_bActive = true;
	else
		m_bActive = false;

	m_pParticules = new Particule[iNumParticules];

	if ( iParam14 )
		m_iSpread = 1;
	else
		m_iSpread = 0;

	iParam9 = iParam9 / 255;
	iParam10 = iParam10 / 255;
	iParam11 = iParam11 / 255;

	for ( int i = 0; i < iNumParticules; i++ )
	{
		m_pParticules[i].active = false;
		m_pParticules[i].origin.x = gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + vecOrigin.x;
		m_pParticules[i].origin.y = gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + vecOrigin.y;
		m_pParticules[i].origin.z = m_iHeight / 2 + vecOrigin.z;
		m_pParticules[i].age = 0;
		m_pParticules[i].speed = gEngfuncs.pfnRandomFloat( -8, 8 ) + m_flSpeed;
		m_pParticules[i].r = iParam9;
		m_pParticules[i].g = iParam10;
		m_pParticules[i].b = iParam11;
		m_pParticules[i].size = iHeight;
		m_pParticules[i].alpha = m_flAlpha;
		m_pParticules[i].wind = 1;
	}

	m_flLastEmit = gHUD.m_flTime;
	m_hSprite = SPR_Load( "sprites/ballsmoke.spr" );
	m_pNext = NULL;

	return true;
}

void CSmokeEmit::Render( double flTime )
{
	double time = flTime;
	Vector point, forward, right, up, angles;
	int i;

	gEngfuncs.GetViewAngles( angles );
	AngleVectors( angles, forward, right, up );

	for ( i = 0; i < m_iNumParticules; i++ )
	{
		m_iFrame = ( SPR_Frames( m_hSprite ) * m_pParticules[i].age ) / m_iLife;
		if ( m_iFrame > SPR_Frames( m_hSprite ) )
			m_iFrame = SPR_Frames( m_hSprite ) - 5;

		if ( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( m_hSprite ), m_iFrame ) )
			return;

		if ( m_pParticules[i].active )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			gEngfuncs.pTriAPI->Color4f( m_pParticules[i].r, m_pParticules[i].g, m_pParticules[i].b, 1.0 );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
			VectorMA( m_pParticules[i].origin, m_pParticules[i].size, up, point );
			VectorMA( point, -m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
			VectorMA( m_pParticules[i].origin, m_pParticules[i].size, up, point );
			VectorMA( point, m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
			VectorMA( m_pParticules[i].origin, -m_pParticules[i].size, up, point );
			VectorMA( point, m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
			VectorMA( m_pParticules[i].origin, -m_pParticules[i].size, up, point );
			VectorMA( point, -m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->End();
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		}
	}
}

void CSmokeEmit::Reset( int iParticule )
{
	m_pParticules[iParticule].origin.x = gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + m_vecOrigin.x;
	m_pParticules[iParticule].origin.y = gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + m_vecOrigin.y;
	m_pParticules[iParticule].origin.z = m_iHeight / 2 + m_vecOrigin.z;
	m_pParticules[iParticule].speed = gEngfuncs.pfnRandomFloat( -8, 8 ) + m_flSpeed;
	m_pParticules[iParticule].age = 0;
	m_pParticules[iParticule].r = m_iR;
	m_pParticules[iParticule].g = m_iG;
	m_pParticules[iParticule].b = m_iB;
	m_pParticules[iParticule].size = m_iHeight;
	m_pParticules[iParticule].alpha = m_flAlpha;
	m_pParticules[iParticule].active = false;
}

bool CSmokeEmit::ProduceSmokePuffNew( void )
{
	for ( int i = 0; i < m_iNumParticules; i++ )
	{
		if ( !m_pParticules[i].active )
		{
			m_pParticules[i].active = true;
			return true;
		}
	}

	return false;
}

void CSmokeEmit::UpdateMouvementPhysics( double flTime )
{
	for ( int i = 0; i < m_iNumParticules; i++ )
	{
		if ( m_pParticules[i].active )
		{
			if ( m_iParam13 )
				m_pParticules[i].origin.z += m_pParticules[i].speed * flTime - m_pParticules[i].wind;
			else
				m_pParticules[i].origin.z += m_pParticules[i].speed * flTime;

			m_pParticules[i].age += flTime * 90;

			if ( m_iParam14 )
			{
				if ( m_flWindX == 0 )
					m_pParticules[i].origin.x += gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread );
				if ( m_flWindX < 0 )
					m_pParticules[i].origin.x += gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + gEngfuncs.pfnRandomFloat( m_flWindX, 0 );
				if ( m_flWindX > 0 )
					m_pParticules[i].origin.x += gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + gEngfuncs.pfnRandomFloat( 0, m_flWindX );

				if ( m_flWindY == 0 )
					m_pParticules[i].origin.y += gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread );
				if ( m_flWindY < 0 )
					m_pParticules[i].origin.y += gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + gEngfuncs.pfnRandomFloat( m_flWindX, 0 );
				if ( m_flWindY > 0 )
					m_pParticules[i].origin.y += gEngfuncs.pfnRandomFloat( -m_iSpread, m_iSpread ) + gEngfuncs.pfnRandomFloat( 0, m_flWindX );
			}
			else
			{
				if ( m_flWindX == 0 )
					m_pParticules[i].origin.x += 0;
				if ( m_flWindX < 0 )
					m_pParticules[i].origin.x += gEngfuncs.pfnRandomFloat( m_flWindX, 0 );
				if ( m_flWindX > 0 )
					m_pParticules[i].origin.x += gEngfuncs.pfnRandomFloat( 0, m_flWindX );

				if ( m_flWindY == 0 )
					m_pParticules[i].origin.y += 0;
				if ( m_flWindY < 0 )
					m_pParticules[i].origin.y += gEngfuncs.pfnRandomFloat( m_flWindX, 0 );
				if ( m_flWindY > 0 )
					m_pParticules[i].origin.y += gEngfuncs.pfnRandomFloat( 0, m_flWindX );
			}

			m_pParticules[i].size += ( m_iParam12 * m_pParticules[i].age ) / ( m_iLife * m_pParticules[i].speed );
			if ( m_pParticules[i].size >= m_iParam12 )
				m_pParticules[i].size = m_iParam12;

			m_pParticules[i].alpha = m_flAlpha - ( m_flAlpha * m_pParticules[i].age ) / m_iLife;
			if ( m_pParticules[i].alpha <= 0 )
				m_pParticules[i].alpha = 0;
			if ( m_pParticules[i].alpha >= m_flAlpha )
				m_pParticules[i].alpha = m_flAlpha;

			m_pParticules[i].wind += 0.2;

			if ( m_pParticules[i].age >= m_iLife )
				Reset( i );
		}
	}

	if ( gHUD.m_flTime - m_flLastEmit >= 0.05 && m_bActive )
	{
		m_flLastEmit = gHUD.m_flTime;
		ProduceSmokePuffNew();
	}
}

void CSmokeEmit::ChangeState( void )
{
	if ( m_bActive )
		m_bActive = false;
	else
		m_bActive = true;
}

CSmokeEmit::~CSmokeEmit()
{
	if ( m_pParticules )
		delete m_pParticules;
	if ( m_pNext )
		delete m_pNext;
}

//=============================================================================
// CFallingPart (rain / snow around the local player)
//=============================================================================

bool CFallingPart::SetEmetteur( int iNumParticules, float flSpeed, int iSize, int iHeight, int iRain )
{
	m_pPlayer = gEngfuncs.GetLocalPlayer();
	m_iNumParticules = iNumParticules;
	m_iSize = iSize;
	m_vecOrigin = m_pPlayer->origin;
	m_flSpeed = flSpeed;
	m_iHeight = iHeight;
	m_iMaxAge = 1000;
	m_iRain = iRain;
	m_vecDir.y = 0;
	m_vecDir.x = 0;
	m_vecDir.z = -1;
	m_vecUp = -m_vecDir;

	m_pParticules = (Particule *)malloc( iNumParticules * sizeof( Particule ) );
	if ( !m_pParticules )
		return false;

	for ( int i = 0; i < iNumParticules; i++ )
	{
		m_pParticules[i].active = true;
		m_pParticules[i].origin.x = gEngfuncs.pfnRandomFloat( -500, 500 ) + m_vecOrigin.x;
		m_pParticules[i].origin.y = gEngfuncs.pfnRandomFloat( -500, 500 ) + m_vecOrigin.y;
		m_pParticules[i].origin.z = m_iHeight + m_vecOrigin.z;
		m_pParticules[i].age = 0;
		m_pParticules[i].speed = gEngfuncs.pfnRandomFloat( -8, 15 ) + m_flSpeed;
		m_pParticules[i].r = 1.0;
		m_pParticules[i].g = 1.0;
		m_pParticules[i].b = 1.0;
		m_pParticules[i].size = iSize;
		m_pParticules[i].alpha = 0.8;
		m_pParticules[i].active = false;
		m_pParticules[i].dist = 0;
	}

	m_flLastTime = gHUD.m_flTime;
	m_flInterval = 0;
	m_flInterval = m_iNumParticules;
	m_flInterval = iHeight / m_flInterval;
	m_flInterval = m_flInterval / m_flSpeed;

	if ( !m_iRain )
		m_hSprite = SPR_Load( "sprites/SBO_snow.spr" );
	else
		m_hSprite = SPR_Load( "sprites/SBO_rain.spr" );

	m_pNext = NULL;

	return true;
}

void CFallingPart::Render( double flTime )
{
	double time = flTime;
	Vector point, forward, right, up, angles;
	int i;

	gEngfuncs.GetViewAngles( angles );
	AngleVectors( angles, forward, right, up );

	if ( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( m_hSprite ), 0 ) )
		return;

	for ( i = 0; i < m_iNumParticules; i++ )
	{
		if ( m_pParticules[i].active )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			gEngfuncs.pTriAPI->Color4f( m_pParticules[i].r, m_pParticules[i].g, m_pParticules[i].b, 1.0 );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
			VectorMA( m_pParticules[i].origin, m_pParticules[i].size, up, point );
			VectorMA( point, -m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
			VectorMA( m_pParticules[i].origin, m_pParticules[i].size, up, point );
			VectorMA( point, m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
			VectorMA( m_pParticules[i].origin, -m_pParticules[i].size, up, point );
			VectorMA( point, m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->Brightness( m_pParticules[i].alpha );
			gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
			VectorMA( m_pParticules[i].origin, -m_pParticules[i].size, up, point );
			VectorMA( point, -m_pParticules[i].size, right, point );
			gEngfuncs.pTriAPI->Vertex3fv( point );

			gEngfuncs.pTriAPI->End();
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		}
	}
}

// SBO: traces down from 13 points around the player; true when fewer than 10 hit something (unused)
bool CFallingPart::IsPlayerOutside( void )
{
	Vector vecPoints[13];
	bool bHit[13];
	int iCount = 0;
	pmtrace_s tr;
	int i, j, k;

	for ( i = 0; i < 13; i++ )
		vecPoints[i] = m_pPlayer->origin;

	vecPoints[1].x += 750;
	vecPoints[1].y -= 750;
	vecPoints[2].y -= 750;
	vecPoints[3].x -= 750;
	vecPoints[3].y -= 750;
	vecPoints[4].x -= 750;
	vecPoints[5] = -vecPoints[1];
	vecPoints[6] = -vecPoints[2];
	vecPoints[7] = -vecPoints[3];
	vecPoints[8] = -vecPoints[4];
	vecPoints[9].x += 375;
	vecPoints[9].y -= 375;
	vecPoints[10].x -= 375;
	vecPoints[10].y -= 375;
	vecPoints[11] = -vecPoints[9];
	vecPoints[12] = -vecPoints[12];

	for ( j = 0; j < 13; j++ )
	{
		tr = *gEngfuncs.PM_TraceLine( vecPoints[j], vecPoints[j] + m_vecUp * m_iHeight, PM_TRACELINE_ANYVISIBLE, 2, 2 );
		if ( tr.fraction != 1.0 )
			bHit[j] = true;
		else
			bHit[j] = false;
	}

	for ( k = 0; k < 13; k++ )
	{
		if ( bHit[k] )
			iCount++;
	}

	if ( iCount >= 10 )
		return false;
	else
		return true;
}

bool CFallingPart::ProduceFlakeOne( void )
{
	for ( int i = 0; i < m_iNumParticules; i++ )
	{
		if ( !m_pParticules[i].active )
		{
			m_pParticules[i].active = true;
			Reset( i );
			return true;
		}
	}

	return false;
}

bool CFallingPart::ActivateMultiple( void )
{
	int i = 0;

	while ( ProduceFlakeOne() )
	{
		i++;
		if ( i == 1 )
			return true;
	}

	return false;
}

void CFallingPart::LookForSlotsTable( int *piFree )
{
	int iCount = 0;

	ClearFreeSlotsTable( piFree );

	if ( !ActivateMultiple() )
		return;

	for ( int i = 0; i < m_iNumParticules; i++ )
	{
		if ( !m_pParticules[i].active )
		{
			piFree[iCount] = i;
			iCount++;
		}
		if ( iCount >= 1 )
			break;
	}
}

void CFallingPart::ClearFreeSlotsTable( int *piFree )
{
	for ( int i = 0; i < 1; i++ )
		piFree[i] = 0;
}

void CFallingPart::ReinitDeadList( int *piFree )
{
	for ( int i = 0; i < 1; i++ )
	{
		m_pParticules[piFree[i]].active = true;
		Reset( piFree[i] );
	}
}

void CFallingPart::Reset( int iParticule )
{
	pmtrace_s tr;

	m_pParticules[iParticule].origin.x = m_pPlayer->origin.x + gEngfuncs.pfnRandomFloat( -750, 750 );
	m_pParticules[iParticule].origin.y = m_pPlayer->origin.y + gEngfuncs.pfnRandomFloat( -750, 750 );
	m_pParticules[iParticule].origin.z = m_pPlayer->origin.z + m_iHeight;
	m_pParticules[iParticule].startz = m_pParticules[iParticule].origin.z;
	m_pParticules[iParticule].speed = gEngfuncs.pfnRandomFloat( -8, 15 ) + m_flSpeed;
	m_pParticules[iParticule].age = 0;
	m_pParticules[iParticule].dist = 0;
	m_pParticules[iParticule].maxdist = 800;

	tr = *gEngfuncs.PM_TraceLine( m_pParticules[iParticule].origin, m_pParticules[iParticule].origin + m_vecDir * 8000, PM_TRACELINE_ANYVISIBLE, 2, 2 );

	if ( tr.startsolid == 1 )
		Reset( iParticule );
	else
		m_pParticules[iParticule].maxdist = m_pParticules[iParticule].origin.z - tr.endpos.z;
}

// SBO: distance to the walls around the player along +x, -x, +y, -y (unused)
void CFallingPart::GetExtentsRect( float *pflXMax, float *pflXMin, float *pflYMax, float *pflYMin )
{
	pmtrace_s tr;
	Vector vecRight, vecLeft, vecForward, vecBack;

	vecRight.y = vecRight.z = 0;
	vecRight.x = 1;
	vecLeft = -vecRight;
	vecForward.x = vecForward.z = 0;
	vecForward.y = 1;
	vecBack = -vecForward;

	tr = *gEngfuncs.PM_TraceLine( m_pPlayer->origin, m_pPlayer->origin + vecRight * 750, PM_TRACELINE_ANYVISIBLE, 2, 2 );
	*pflXMax = tr.endpos.x - m_pPlayer->origin.x;

	tr = *gEngfuncs.PM_TraceLine( m_pPlayer->origin, m_pPlayer->origin + vecLeft * 750, PM_TRACELINE_ANYVISIBLE, 2, 2 );
	*pflXMin = tr.endpos.x - m_pPlayer->origin.x;

	tr = *gEngfuncs.PM_TraceLine( m_pPlayer->origin, m_pPlayer->origin + vecForward * 750, PM_TRACELINE_ANYVISIBLE, 2, 2 );
	*pflYMax = tr.endpos.y - m_pPlayer->origin.y;

	tr = *gEngfuncs.PM_TraceLine( m_pPlayer->origin, m_pPlayer->origin + vecBack * 750, PM_TRACELINE_ANYVISIBLE, 2, 2 );
	*pflYMin = tr.endpos.y - m_pPlayer->origin.y;
}

void CFallingPart::RefreshMouvements( double flTime )
{
	pmtrace_s tr;

	for ( int i = 0; i < m_iNumParticules; i++ )
	{
		if ( m_pParticules[i].active )
		{
			m_pParticules[i].origin.z -= m_pParticules[i].speed * flTime;
			m_pParticules[i].dist = m_pParticules[i].startz - m_pParticules[i].origin.z;

			if ( !m_iRain )
			{
				m_pParticules[i].origin.x += gEngfuncs.pfnRandomFloat( -1, 1 );
				m_pParticules[i].origin.y += gEngfuncs.pfnRandomFloat( -1, 1 );
			}

			m_pParticules[i].age += flTime * 90;

			if ( m_pParticules[i].dist >= m_pParticules[i].maxdist )
				m_pParticules[i].active = false;
			if ( m_pParticules[i].age >= m_iMaxAge )
				m_pParticules[i].active = false;
		}
	}

	if ( gHUD.m_flTime - m_flLastTime >= m_flInterval )
	{
		LookForSlotsTable( m_iFree );
		if ( !m_iFree )
			return;
		ReinitDeadList( m_iFree );
		m_flLastTime = gHUD.m_flTime;
	}
}

CFallingPart::~CFallingPart()
{
	free( m_pParticules );
	if ( m_pNext )
		delete m_pNext;
}

//=============================================================================
// Emitter lists
//=============================================================================

CListeEmetteur::CListeEmetteur()
{
	m_pFirst = NULL;
	m_pLast = NULL;
}

void CListeEmetteur::Ajouter( CParticulesEmetteur *pEmetteur )
{
	if ( !m_pFirst )
	{
		m_pFirst = pEmetteur;
		m_pLast = pEmetteur;
		m_pLast->m_pNext = NULL;
	}
	else
	{
		m_pLast->m_pNext = pEmetteur;
		m_pLast = m_pLast->m_pNext;
		m_pLast->m_pNext = NULL;
	}
}

void CListeEmetteur::Afficher( float flTime )
{
	CParticulesEmetteur *pEmetteur = NULL;

	pEmetteur = m_pFirst;
	while ( pEmetteur )
	{
		pEmetteur->Render( flTime );
		pEmetteur->EvolueParticules( flTime );
		pEmetteur = pEmetteur->m_pNext;
	}
}

void CListeEmetteur::Clear( void )
{
	if ( m_pFirst )
		delete m_pFirst;
	m_pFirst = NULL;
	m_pLast = NULL;
}

void CListeEmetteur::ChangeState( int iId )
{
	CParticulesEmetteur *pEmetteur = NULL;

	pEmetteur = m_pFirst;
	while ( pEmetteur )
	{
		if ( pEmetteur->m_iId == iId )
		{
			pEmetteur->ChangeState();
			break;
		}
		pEmetteur = pEmetteur->m_pNext;
	}
}

CListeEmetteur::~CListeEmetteur()
{
	if ( m_pFirst )
		delete m_pFirst;
}

CListeFumee::CListeFumee()
{
	m_pFirst = NULL;
	m_pLast = NULL;
}

void CListeFumee::Ajouter( CSmokeEmit *pEmit )
{
	if ( !m_pFirst )
	{
		m_pFirst = pEmit;
		m_pLast = pEmit;
		m_pLast->m_pNext = NULL;
	}
	else
	{
		m_pLast->m_pNext = pEmit;
		m_pLast = m_pLast->m_pNext;
		m_pLast->m_pNext = NULL;
	}
}

void CListeFumee::Afficher( float flTime )
{
	CSmokeEmit *pEmit = NULL;

	pEmit = m_pFirst;
	while ( pEmit )
	{
		pEmit->Render( flTime );
		pEmit->UpdateMouvementPhysics( flTime );
		pEmit = pEmit->m_pNext;
	}
}

void CListeFumee::ChangeState( int iId )
{
	CSmokeEmit *pEmit = NULL;

	pEmit = m_pFirst;
	while ( pEmit )
	{
		if ( pEmit->m_iId == iId )
		{
			pEmit->ChangeState();
			break;
		}
		pEmit = pEmit->m_pNext;
	}
}

void CListeFumee::Clear( void )
{
	if ( m_pFirst )
		delete m_pFirst;
	m_pFirst = NULL;
	m_pLast = NULL;
}

CListeFumee::~CListeFumee()
{
	if ( m_pFirst )
		delete m_pFirst;
}

CListeFall::CListeFall()
{
	m_pFirst = NULL;
	m_pLast = NULL;
}

void CListeFall::Ajouter( CFallingPart *pPart )
{
	if ( !m_pFirst )
	{
		m_pFirst = pPart;
		m_pLast = pPart;
		m_pLast->m_pNext = NULL;
	}
	else
	{
		m_pLast->m_pNext = pPart;
		m_pLast = m_pLast->m_pNext;
		m_pLast->m_pNext = NULL;
	}
}

void CListeFall::Afficher( float flTime )
{
	CFallingPart *pPart = NULL;

	pPart = m_pFirst;
	while ( pPart )
	{
		pPart->Render( flTime );
		pPart->RefreshMouvements( flTime );
		pPart = pPart->m_pNext;
	}
}

void CListeFall::Clear( void )
{
	if ( m_pFirst )
		delete m_pFirst;
	m_pFirst = NULL;
	m_pLast = NULL;
}

CListeFall::~CListeFall()
{
	if ( m_pFirst )
		delete m_pFirst;
}
