// Aurora particle system (Spirit of Half-Life, "02/08/02 November235: Particle System", LRC).
//
// SBO 2.0 port of LogicAndTrick's halflife-updated-spirit (branch spirit-1.8-bug-fixes, commit 2482306),
// cl_dll/particlesys.cpp. What changed against it (docs/AURORA.md, "Changes from Spirit of Half-Life"):
//  - every member is initialised (m_bBouncing, m_pMainType, the particle block when the script is missing);
//  - a missing script or sprite gives an empty system and one console line, no crash;
//  - the script text comes from the manager's cache (Spirit leaked each file buffer: it freed the parse cursor);
//  - an unknown key skips its value (Spirit read the value as the next key, so "collision" "bounce" ate the
//    "bounce" that followed it); "collision" none / die / bounce is read;
//  - "name" of a type used before (a placeholder) keeps the type list intact (Spirit cut the list there, so the
//    types before it leaked and could not be found); the name is resolved at the end of the block (a second
//    "name" in one block freed a type still in the list); placeholders count towards the type limit;
//  - the cos/sin lookups wrap any angle (Spirit read outside the table out of -360 .. 449 degrees);
//  - overlays belong to their particle (Spirit also updated and drew them on their own, at a stale position, and
//    they could outlive the particle) and nest at most AURORA_MAX_OVERLAYS deep (a type overlaying itself recursed
//    without end);
//  - a spray rate above the frame rate sprays several children a frame (Spirit: at most one a frame);
//  - the emitter's angles turn the spray the way the angles point (Spirit subtracted them: a yaw of 90 turned
//    it to 270);
//  - bounce traces the move the particle made (Spirit traced the next one from where the particle already was);
//  - frame, color and alpha are kept in range when drawn; per-system and global particle limits;
//  - the emitter is placed from the message (a point, or a followed entity or one of its attachments), and the
//    system is neither updated nor drawn while that place is outside the PVS;
//  - the script is read by our own tokenizer (the engine's rules) that never reads past the end of the text (the
//    engine's stepped past it after an unterminated quote) and caps a token at the buffer's size; a key without a
//    value before a "{" or "}" no longer takes that brace (the blocks stayed in step only by luck); a number that
//    is not finite or beyond 1e30 ("inf", "nan", 1e999) reads as 0, and the angle and frame lookups stay
//    defined for any value;
//  - XashXT / PrimeXT's top-level "lightmodel" (particles tinted by the world light) and "killcondition" (the
//    system stops for good once its emitter is in empty / water / solid) are read.

#include <math.h>	// before cl_util.h (its fabs macro)
#include <string.h>
#include <stdlib.h>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "event_api.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "particlesys.h"

// Spirit's const.h: "drawcondition" special / special1 .. special3. No GoldSrc engine returns these contents, so
// such a type is never drawn (as in Spirit).
#ifndef CONTENT_SPECIAL1
#define CONTENT_SPECIAL1	-20
#define CONTENT_SPECIAL2	-21
#define CONTENT_SPECIAL3	-22
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float ParticleSystem::c_fCosTable[360 + 90];
bool ParticleSystem::c_bCosTableInit = false;

int g_iAuroraActiveParticles = 0;
int g_iAuroraAllocatedParticles = 0;

// set by the manager while it builds a system from a script it has built one from before: the script's problems
// were printed then (a Spawn Use emitter triggered again and again would flood the console otherwise)
bool g_bAuroraQuiet = false;

static void Aurora_Warn( const char *szFile, const char *szText, const char *szWhat )
{
	if ( g_bAuroraQuiet )
		return;
	gEngfuncs.Con_Printf( "Aurora: %s: %s%s\n", szFile, szText, szWhat ? szWhat : "" );
}

#define AURORA_MAX_TOKEN	1024

// the characters that are a token on their own outside quotes (the engine's COM_Parse)
static bool Aurora_IsBreak( char c )
{
	return c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ',' || c == ':';
}

// One token of a script, by the engine's COM_Parse rules: "//" comments to the end of the line, a "quoted" string,
// one break character, or a word up to white space or a break character (a '"' inside a word is part of it:
// Spirit's sparker.aur has "startsize" 14"). Bytes above 127 count as white space outside quotes, as in the engine.
// At most iSize - 1 characters are kept (the rest of a longer token is skipped). Returns where the next token
// starts, or NULL when the text has no more; never past the text's terminator (the engine's parser stepped past it
// after an unterminated quote, and the next call read beyond the buffer). The script comes from the server: any
// text has to be safe here.
static char *Aurora_Token( char *data, char *token, int iSize, bool &bQuoted )
{
	int len = 0;
	token[0] = 0;
	bQuoted = false;
	if ( !data )
		return NULL;

	for ( ;; )
	{
		while ( *data && (signed char)*data <= ' ' )
			data++;
		if ( !*data )
			return NULL;
		if ( data[0] == '/' && data[1] == '/' )
		{
			while ( *data && *data != '\n' )
				data++;
			continue;
		}
		break;
	}

	if ( *data == '"' )
	{
		bQuoted = true;
		data++;
		while ( *data && *data != '"' )
		{
			if ( len < iSize - 1 )
				token[len++] = *data;
			data++;
		}
		token[len] = 0;
		if ( *data == '"' )
			data++;		// an unterminated quote ends at the terminator, and stays there
		return data;
	}

	if ( Aurora_IsBreak( *data ) )
	{
		token[0] = *data;
		token[1] = 0;
		return data + 1;
	}

	do
	{
		if ( len < iSize - 1 )
			token[len++] = *data;
		data++;
	} while ( (signed char)*data > ' ' && !Aurora_IsBreak( *data ) );
	token[len] = 0;
	return data;
}

// a block brace outside quotes
static bool Aurora_IsBrace( const char *szToken, bool bQuoted, char cBrace )
{
	return !bQuoted && szToken[0] == cBrace && !szToken[1];
}

// The value after a key. False, and nothing read, when the key has none: the text ends, or a "{" or "}" follows
// (it is left for the block structure: Spirit and SBO before took it as the value, which ate the end of a type or
// the start of the next one).
static bool Aurora_Value( char *&szFile, char *szValue, int iSize )
{
	bool bQuoted;
	char *pNext = Aurora_Token( szFile, szValue, iSize, bQuoted );
	if ( !pNext || Aurora_IsBrace( szValue, bQuoted, '{' ) || Aurora_IsBrace( szValue, bQuoted, '}' ) )
	{
		szValue[0] = 0;
		return false;
	}
	szFile = pNext;
	return true;
}

ParticleType::ParticleType( ParticleType *pNext )
{
	m_bIsDefined = false;
	m_iRenderMode = kRenderTransAdd;
	m_iDrawCond = 0;
	m_iCollision = COLLISION_NONE;

	m_StartAngle = RandomRange( 45 );
	m_StartRed = m_StartGreen = m_StartBlue = m_StartAlpha = RandomRange( 1 );
	m_EndRed = m_EndGreen = m_EndBlue = m_EndAlpha = RandomRange( 1 );

	m_pSprayType = NULL;
	m_hSprite = 0;
	m_pOverlayType = NULL;

	m_pNext = pNext;
	m_szName[0] = 0;
}

particle *ParticleType::CreateParticle( ParticleSystem *pSys )
{
	if ( !pSys )
		return NULL;

	particle *pPart = pSys->ActivateParticle();
	if ( !pPart )
		return NULL;

	pPart->age = 0.0f;
	pPart->age_death = m_Life.GetInstance();

	InitParticle( pPart, pSys );

	return pPart;
}

void ParticleType::InitParticle( particle *pPart, ParticleSystem *pSys, int iDepth )
{
	const float fLifeRecip = ( pPart->age_death > 0 ) ? 1.0f / pPart->age_death : 0.0f;

	pPart->pType = this;
	pPart->m_bBound = false;

	pPart->velocity = Vector( 0, 0, 0 );
	pPart->accel = Vector( 0.0f, 0.0f, m_Gravity.GetInstance() );

	particle *pOverlay = NULL;
	if ( m_pOverlayType && iDepth < AURORA_MAX_OVERLAYS )
	{
		// create an overlay for this particle: it is drawn on this particle and dies with it
		pOverlay = pSys->AllocParticle();
		if ( pOverlay )
		{
			pOverlay->age = pPart->age;
			pOverlay->age_death = pPart->age_death;
			m_pOverlayType->InitParticle( pOverlay, pSys, iDepth + 1 );
			pOverlay->age_spray = 0.0f;		// an overlay sprays nothing (it doesn't move on its own)
		}
	}
	pPart->m_pOverlay = pOverlay;

	pPart->age_spray = 0.0f;
	if ( m_pSprayType )
	{
		const float fRate = m_SprayRate.GetInstance();
		if ( fRate > 0 )
			pPart->age_spray = 1.0f / fRate;
	}

	pPart->m_fSize = m_StartSize.GetInstance();
	if ( m_EndSize.IsDefined() )
		pPart->m_fSizeStep = m_EndSize.GetOffset( pPart->m_fSize ) * fLifeRecip;
	else
		pPart->m_fSizeStep = m_SizeDelta.GetInstance();

	pPart->frame = m_StartFrame.GetInstance();
	if ( m_EndFrame.IsDefined() )
		pPart->m_fFrameStep = m_EndFrame.GetOffset( pPart->frame ) * fLifeRecip;
	else
		pPart->m_fFrameStep = m_FrameRate.GetInstance();

	pPart->m_fAlpha = m_StartAlpha.GetInstance();
	pPart->m_fAlphaStep = m_EndAlpha.GetOffset( pPart->m_fAlpha ) * fLifeRecip;
	pPart->m_fRed = m_StartRed.GetInstance();
	pPart->m_fRedStep = m_EndRed.GetOffset( pPart->m_fRed ) * fLifeRecip;
	pPart->m_fGreen = m_StartGreen.GetInstance();
	pPart->m_fGreenStep = m_EndGreen.GetOffset( pPart->m_fGreen ) * fLifeRecip;
	pPart->m_fBlue = m_StartBlue.GetInstance();
	pPart->m_fBlueStep = m_EndBlue.GetOffset( pPart->m_fBlue ) * fLifeRecip;

	pPart->m_fAngle = m_StartAngle.GetInstance();
	pPart->m_fAngleStep = m_AngleDelta.GetInstance();

	pPart->m_fDrag = m_Drag.GetInstance();

	pPart->m_vecLight = Vector( 1, 1, 1 );
	pPart->m_fLightAge = 0;		// sampled when it is first drawn

	const float fWindStrength = m_WindStrength.GetInstance();
	const float fWindYaw = m_WindYaw.GetInstance();
	pPart->m_vecWind.x = fWindStrength * ParticleSystem::CosLookup( fWindYaw );
	pPart->m_vecWind.y = fWindStrength * ParticleSystem::SinLookup( fWindYaw );
	pPart->m_vecWind.z = 0;
}

//============================================

// a number of the script: one beyond +-1e30 ("inf", 1e999) or not a number ("nan") is 0
static float Aurora_Number( const char *sz )
{
	const double d = atof( sz );
	return ( d > -1e30 && d < 1e30 ) ? (float)d : 0.0f;	// false for NaN too
}

// "min..max" or one number
RandomRange::RandomRange( const char *szToken )
{
	m_bDefined = true;

	const char *pDots = strstr( szToken, ".." );
	if ( pDots )
	{
		char szMin[64];
		size_t len = pDots - szToken;
		if ( len >= sizeof( szMin ) )
			len = sizeof( szMin ) - 1;
		memcpy( szMin, szToken, len );
		szMin[len] = 0;

		m_fMin = Aurora_Number( szMin );
		m_fMax = Aurora_Number( pDots + 2 );
		return;
	}

	// no range, just record the number
	m_fMin = m_fMax = Aurora_Number( szToken );
}

//============================================

// the table index of any angle (huge or not finite values, which a script's sums can reach, wrap or are 0: the
// cast to int is only defined in int's range)
static int Aurora_AngleIndex( float angle )
{
	if ( !( angle > -1e6f && angle < 1e6f ) )
	{
		angle = (float)fmod( angle, 360.0 );
		if ( angle != angle )
			angle = 0;	// fmod of an infinity, or NaN
	}
	int i = (int)floor( angle ) % 360;
	if ( i < 0 )
		i += 360;
	return i;
}

float ParticleSystem::CosLookup( float angle )
{
	return c_fCosTable[Aurora_AngleIndex( angle )];
}

float ParticleSystem::SinLookup( float angle )
{
	// sin(a) = cos(a - 90) = cos(a + 270)
	const int i = Aurora_AngleIndex( angle );
	return c_fCosTable[i + 270 >= 360 ? i - 90 : i + 270];
}

ParticleSystem::ParticleSystem( int iId, const char *szFilename, const char *szFileText )
{
	int iParticles = AURORA_DEFAULT_PARTICLES;

	m_pNextSystem = NULL;
	m_pActiveParticle = NULL;
	m_fViewerDist = 0;
	m_iId = iId;
	m_bOneShot = false;
	m_flStopTime = 0;
	m_bVisible = false;

	m_pAllParticles = NULL;
	m_iAllocated = 0;
	m_pFreeParticle = NULL;
	m_pMainParticle = NULL;
	m_pFirstType = NULL;
	m_iTypes = 0;
	m_pMainType = NULL;

	memset( &m_Placement, 0, sizeof( m_Placement ) );
	m_Placement.iAttachment = -1;
	m_bOn = false;
	m_iLightModel = 0;
	m_iKillCondition = 0;
	m_bKilled = false;
	m_bTypeLimitWarned = false;
	m_vecEmitter = m_vecEmitterAngles = Vector( 0, 0, 0 );
	m_bHasEmitter = false;
	m_vecMins = m_vecMaxs = Vector( 0, 0, 0 );
	m_bHasBounds = false;

	strncpy( m_szFile, szFilename, sizeof( m_szFile ) - 1 );
	m_szFile[sizeof( m_szFile ) - 1] = 0;

	if ( !c_bCosTableInit )
	{
		for ( int i = 0; i < 360 + 90; i++ )
			c_fCosTable[i] = (float)cos( i * M_PI / 180.0 );
		c_bCosTableInit = true;
	}

	if ( !szFileText )
	{
		// the manager said it once already (Aurora_GetScript); an empty system, nothing to draw
		AllocateParticles( 0 );
		return;
	}

	// a copy with two terminators (the tokenizer stops at the first: the second is only a margin)
	const size_t len = strlen( szFileText );
	char *szCopy = new char[len + 2];
	memcpy( szCopy, szFileText, len );
	szCopy[len] = szCopy[len + 1] = 0;

	char szKey[AURORA_MAX_TOKEN];
	char szToken[AURORA_MAX_TOKEN];
	char *szFile = szCopy;

	for ( ;; )
	{
		bool bQuoted;
		char *pNext = Aurora_Token( szFile, szKey, sizeof( szKey ), bQuoted );
		if ( !pNext )
			break;
		szFile = pNext;

		if ( Aurora_IsBrace( szKey, bQuoted, '{' ) )
		{
			// parse new type
			ParseType( szFile );	// parses the type, moves the file pointer
			continue;
		}
		if ( Aurora_IsBrace( szKey, bQuoted, '}' ) )
		{
			Aurora_Warn( m_szFile, "a \"}\" without its \"{\", ignored", NULL );
			continue;
		}
		if ( !szKey[0] )
			continue;	// an empty key ("")

		if ( !Aurora_Value( szFile, szToken, sizeof( szToken ) ) )
		{
			Aurora_Warn( m_szFile, "key without a value, ignored: ", szKey );
			continue;
		}

		if ( !stricmp( szKey, "particles" ) )
		{
			iParticles = atoi( szToken );
		}
		else if ( !stricmp( szKey, "maintype" ) )
		{
			ParticleType *pTemp = GetType( szToken );
			m_pMainType = pTemp ? pTemp : AddPlaceholderType( szToken );
		}
		else if ( !stricmp( szKey, "lightmodel" ) )
		{
			// XashXT / PrimeXT: 1 tints the particles by the world light, 2 also by projected lights (none in
			// GoldSrc: the same as 1)
			m_iLightModel = atoi( szToken );
			if ( m_iLightModel < 0 )
				m_iLightModel = 0;
		}
		else if ( !stricmp( szKey, "killcondition" ) )
		{
			// XashXT / PrimeXT: the contents at the emitter that stop the system
			ParseKillCondition( szToken );
		}
		else
		{
			// an unknown key: its value is skipped, so the next key is read as one
			Aurora_Warn( m_szFile, "unknown key ", szKey );
		}
	}

	delete[] szCopy;

	if ( !m_pMainType )
		Aurora_Warn( m_szFile, "no \"maintype\", the system emits nothing", NULL );
	else if ( !m_pMainType->m_bIsDefined )
		Aurora_Warn( m_szFile, "the main type is not defined: ", m_pMainType->m_szName );

	for ( ParticleType *pType = m_pFirstType; pType; pType = pType->m_pNext )
	{
		if ( !pType->m_bIsDefined )
			Aurora_Warn( m_szFile, "type used but not defined: ", pType->m_szName );
	}

	if ( iParticles < 1 )
		iParticles = 1;
	if ( iParticles > AURORA_MAX_PER_SYSTEM )
	{
		Aurora_Warn( m_szFile, "\"particles\" lowered to the limit of one system", NULL );
		iParticles = AURORA_MAX_PER_SYSTEM;
	}

	AllocateParticles( iParticles );
}

void ParticleSystem::AllocateParticles( int iParticles )
{
	if ( g_iAuroraAllocatedParticles + iParticles > AURORA_MAX_ALLOCATED )
	{
		iParticles = AURORA_MAX_ALLOCATED - g_iAuroraAllocatedParticles;
		if ( iParticles < 0 )
			iParticles = 0;
		Aurora_Warn( m_szFile, "the particle limit of all systems is reached, this system gets fewer", NULL );
	}

	m_pFreeParticle = NULL;
	m_pActiveParticle = NULL;
	m_pMainParticle = NULL;
	m_iAllocated = iParticles;

	if ( iParticles <= 0 )
	{
		m_pAllParticles = NULL;
		m_iAllocated = 0;
		return;
	}

	m_pAllParticles = new particle[iParticles];
	memset( m_pAllParticles, 0, sizeof( particle ) * iParticles );
	g_iAuroraAllocatedParticles += iParticles;

	// initialise the linked list
	for ( int i = 0; i < iParticles - 1; i++ )
		m_pAllParticles[i].nextpart = &m_pAllParticles[i + 1];
	m_pAllParticles[iParticles - 1].nextpart = NULL;
	m_pFreeParticle = m_pAllParticles;
}

ParticleSystem::~ParticleSystem( void )
{
	// the active count goes down by what this system still had alive
	for ( particle *pPart = m_pActiveParticle; pPart; pPart = pPart->nextpart )
	{
		for ( particle *pLayer = pPart; pLayer; pLayer = pLayer->m_pOverlay )
			g_iAuroraActiveParticles--;
	}
	if ( g_iAuroraActiveParticles < 0 )
		g_iAuroraActiveParticles = 0;

	delete[] m_pAllParticles;
	g_iAuroraAllocatedParticles -= m_iAllocated;
	if ( g_iAuroraAllocatedParticles < 0 )
		g_iAuroraAllocatedParticles = 0;

	ParticleType *pType = m_pFirstType;
	while ( pType )
	{
		ParticleType *pNext = pType->m_pNext;
		delete pType;
		pType = pNext;
	}
}

// a "killcondition" value (top level or in a type): the system's kill condition
void ParticleSystem::ParseKillCondition( const char *szValue )
{
	if ( !stricmp( szValue, "empty" ) )
		m_iKillCondition = CONTENT_EMPTY;
	else if ( !stricmp( szValue, "water" ) )
		m_iKillCondition = CONTENT_WATER;
	else if ( !stricmp( szValue, "solid" ) )
		m_iKillCondition = CONTENT_SOLID;
	else
		Aurora_Warn( m_szFile, "unknown killcondition (empty, water, solid): ", szValue );
}

// returns the ParticleType with the given name, if there is one
ParticleType *ParticleSystem::GetType( const char *szName )
{
	for ( ParticleType *pType = m_pFirstType; pType; pType = pType->m_pNext )
	{
		if ( !stricmp( pType->m_szName, szName ) )
			return pType;
	}
	return NULL;
}

// one console line a script: the first type over the limit
void ParticleSystem::TypeLimitReached( const char *szName )
{
	if ( !m_bTypeLimitWarned )
		Aurora_Warn( m_szFile, "too many particle types, this one and the ones after it are ignored: ", szName );
	m_bTypeLimitWarned = true;
}

ParticleType *ParticleSystem::AddPlaceholderType( const char *szName )
{
	// placeholders count towards the limit too (a script naming thousands of types would slow every lookup)
	if ( m_iTypes >= AURORA_MAX_TYPES )
	{
		TypeLimitReached( szName );
		return NULL;
	}

	m_pFirstType = new ParticleType( m_pFirstType );
	m_iTypes++;
	strncpy( m_pFirstType->m_szName, szName, sizeof( m_pFirstType->m_szName ) - 1 );
	m_pFirstType->m_szName[sizeof( m_pFirstType->m_szName ) - 1] = 0;
	return m_pFirstType;
}

// a "spraytype" / "overlaytype" value: the type of that name, a placeholder until it is defined (NULL past the type
// limit). pThis: the type being parsed, which isn't in the list yet (a type that sprays itself got a second,
// undefined type in Spirit)
static ParticleType *Aurora_TypeRef( ParticleSystem *pSys, ParticleType *pThis, const char *szName )
{
	if ( pThis->m_szName[0] && !stricmp( pThis->m_szName, szName ) )
		return pThis;

	ParticleType *pTemp = pSys->GetType( szName );
	return pTemp ? pTemp : pSys->AddPlaceholderType( szName );
}

// creates a new particletype from the given file
// NB: this changes the value of szFile.
ParticleType *ParticleSystem::ParseType( char *&szFile )
{
	ParticleType *pType = new ParticleType();

	// parse the .aur file
	char szKey[AURORA_MAX_TOKEN];
	char szToken[AURORA_MAX_TOKEN];

	for ( ;; )
	{
		bool bQuoted;
		char *pNext = Aurora_Token( szFile, szKey, sizeof( szKey ), bQuoted );
		if ( !pNext )
		{
			Aurora_Warn( m_szFile, "the last type has no closing \"}\"", NULL );
			break;
		}
		if ( Aurora_IsBrace( szKey, bQuoted, '}' ) )
		{
			szFile = pNext;
			break;
		}
		if ( Aurora_IsBrace( szKey, bQuoted, '{' ) )
		{
			// left for the caller: the next type starts there
			Aurora_Warn( m_szFile, "a type has no closing \"}\" before the next \"{\"", NULL );
			break;
		}
		szFile = pNext;

		if ( !szKey[0] )
			continue;	// an empty key ("")

		if ( !Aurora_Value( szFile, szToken, sizeof( szToken ) ) )
		{
			Aurora_Warn( m_szFile, "key without a value, ignored: ", szKey );
			continue;
		}

		if ( !stricmp( szKey, "name" ) )
		{
			// the type joins the list (or the placeholder of that name) at the end of the block; a second "name"
			// renames it (Spirit merged at each "name": a second one freed a type still in the list)
			strncpy( pType->m_szName, szToken, sizeof( pType->m_szName ) - 1 );
			pType->m_szName[sizeof( pType->m_szName ) - 1] = 0;
		}
		else if ( !stricmp( szKey, "gravity" ) )
		{
			pType->m_Gravity = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "windyaw" ) )
		{
			pType->m_WindYaw = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "windstrength" ) )
		{
			pType->m_WindStrength = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "sprite" ) )
		{
			pType->m_hSprite = Aurora_LoadSprite( szToken );
			if ( !pType->m_hSprite )
				Aurora_Warn( m_szFile, "sprite not found: ", szToken );
		}
		else if ( !stricmp( szKey, "startalpha" ) )
		{
			pType->m_StartAlpha = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "endalpha" ) )
		{
			pType->m_EndAlpha = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "startred" ) )
		{
			pType->m_StartRed = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "endred" ) )
		{
			pType->m_EndRed = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "startgreen" ) )
		{
			pType->m_StartGreen = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "endgreen" ) )
		{
			pType->m_EndGreen = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "startblue" ) )
		{
			pType->m_StartBlue = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "endblue" ) )
		{
			pType->m_EndBlue = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "startsize" ) )
		{
			pType->m_StartSize = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "sizedelta" ) )
		{
			pType->m_SizeDelta = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "endsize" ) )
		{
			pType->m_EndSize = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "startangle" ) )
		{
			pType->m_StartAngle = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "angledelta" ) )
		{
			pType->m_AngleDelta = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "startframe" ) )
		{
			pType->m_StartFrame = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "endframe" ) )
		{
			pType->m_EndFrame = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "framerate" ) )
		{
			pType->m_FrameRate = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "lifetime" ) )
		{
			pType->m_Life = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "spraytype" ) )
		{
			pType->m_pSprayType = Aurora_TypeRef( this, pType, szToken );
		}
		else if ( !stricmp( szKey, "overlaytype" ) )
		{
			pType->m_pOverlayType = Aurora_TypeRef( this, pType, szToken );
		}
		else if ( !stricmp( szKey, "sprayrate" ) )
		{
			pType->m_SprayRate = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "sprayforce" ) )
		{
			pType->m_SprayForce = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "spraypitch" ) )
		{
			pType->m_SprayPitch = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "sprayyaw" ) )
		{
			pType->m_SprayYaw = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "drag" ) )
		{
			pType->m_Drag = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "bounce" ) )
		{
			pType->m_Bounce = RandomRange( szToken );
			// Spirit: a non-zero bounce turns bouncing on ("collision" "bounce" also does, with any bounce)
			if ( ( pType->m_Bounce.m_fMin != 0 || pType->m_Bounce.m_fMax != 0 ) && pType->m_iCollision == COLLISION_NONE )
				pType->m_iCollision = COLLISION_BOUNCE;
		}
		else if ( !stricmp( szKey, "bouncefriction" ) )
		{
			pType->m_BounceFriction = RandomRange( szToken );
		}
		else if ( !stricmp( szKey, "collision" ) )
		{
			if ( !stricmp( szToken, "none" ) )
				pType->m_iCollision = COLLISION_NONE;
			else if ( !stricmp( szToken, "die" ) )
				pType->m_iCollision = COLLISION_DIE;
			else if ( !stricmp( szToken, "bounce" ) )
				pType->m_iCollision = COLLISION_BOUNCE;
			else
				Aurora_Warn( m_szFile, "unknown collision (none, die, bounce): ", szToken );
		}
		else if ( !stricmp( szKey, "rendermode" ) )
		{
			if ( !stricmp( szToken, "additive" ) )
				pType->m_iRenderMode = kRenderTransAdd;
			else if ( !stricmp( szToken, "solid" ) )
				pType->m_iRenderMode = kRenderTransAlpha;
			else if ( !stricmp( szToken, "texture" ) )
				pType->m_iRenderMode = kRenderTransTexture;
			else if ( !stricmp( szToken, "color" ) )
				pType->m_iRenderMode = kRenderTransColor;
			else
				Aurora_Warn( m_szFile, "unknown rendermode (additive, solid, texture, color): ", szToken );
		}
		else if ( !stricmp( szKey, "killcondition" ) )
		{
			// XashXT's editor (pfxEd) saves it in the type blocks, its runtime reads it at the top level: either
			// stops the whole system
			ParseKillCondition( szToken );
		}
		else if ( !stricmp( szKey, "drawcondition" ) )
		{
			if ( !stricmp( szToken, "empty" ) )
				pType->m_iDrawCond = CONTENT_EMPTY;
			else if ( !stricmp( szToken, "water" ) )
				pType->m_iDrawCond = CONTENT_WATER;
			else if ( !stricmp( szToken, "solid" ) )
				pType->m_iDrawCond = CONTENT_SOLID;
			else if ( !stricmp( szToken, "special" ) || !stricmp( szToken, "special1" ) )
				pType->m_iDrawCond = CONTENT_SPECIAL1;
			else if ( !stricmp( szToken, "special2" ) )
				pType->m_iDrawCond = CONTENT_SPECIAL2;
			else if ( !stricmp( szToken, "special3" ) )
				pType->m_iDrawCond = CONTENT_SPECIAL3;
			else
				Aurora_Warn( m_szFile, "unknown drawcondition: ", szToken );
		}
		else
		{
			// an unknown key: its value is skipped, so the next key is read as one
			Aurora_Warn( m_szFile, "unknown key ", szKey );
		}
	}

	ParticleType *pTemp = pType->m_szName[0] ? GetType( pType->m_szName ) : NULL;
	if ( pTemp )
	{
		// there's already a type with this name (a placeholder, or a type defined before)
		if ( pTemp->m_bIsDefined )
			Aurora_Warn( m_szFile, "particle type defined more than once: ", pType->m_szName );

		// copy all our data into the existing type (it stays where it is in the list), throw away the type we were
		// making; a type that sprays or overlays itself does so as the one in the list
		ParticleType *pNext = pTemp->m_pNext;
		*pTemp = *pType;
		pTemp->m_pNext = pNext;
		if ( pTemp->m_pSprayType == pType )
			pTemp->m_pSprayType = pTemp;
		if ( pTemp->m_pOverlayType == pType )
			pTemp->m_pOverlayType = pTemp;
		pTemp->m_bIsDefined = true;
		delete pType;
		return pTemp;
	}

	if ( m_iTypes >= AURORA_MAX_TYPES )
	{
		TypeLimitReached( pType->m_szName );
		delete pType;
		return NULL;
	}

	// a newly-defined type: add it to the list
	pType->m_pNext = m_pFirstType;
	m_pFirstType = pType;
	m_iTypes++;
	pType->m_bIsDefined = true;

	return pType;
}

particle *ParticleSystem::AllocParticle()
{
	if ( g_iAuroraActiveParticles >= AURORA_MAX_ACTIVE )
		return NULL;

	particle *pPart = m_pFreeParticle;
	if ( pPart )
	{
		m_pFreeParticle = pPart->nextpart;
		pPart->nextpart = NULL;
		pPart->m_pOverlay = NULL;
		g_iAuroraActiveParticles++;
	}
	return pPart;
}

particle *ParticleSystem::ActivateParticle()
{
	particle *pActivated = AllocParticle();
	if ( pActivated )
	{
		pActivated->nextpart = m_pActiveParticle;
		m_pActiveParticle = pActivated;
	}
	return pActivated;
}

void ParticleSystem::FreeParticle( particle *pPart )
{
	while ( pPart )
	{
		particle *pOverlay = pPart->m_pOverlay;
		pPart->m_pOverlay = NULL;
		pPart->nextpart = m_pFreeParticle;
		m_pFreeParticle = pPart;
		if ( g_iAuroraActiveParticles > 0 )
			g_iAuroraActiveParticles--;
		pPart = pOverlay;
	}
}

void ParticleSystem::CalculateDistance( const Vector &vecView )
{
	Vector center;
	if ( m_bHasBounds )
		center = ( m_vecMins + m_vecMaxs ) * 0.5f;
	else
		center = m_vecEmitter;

	const Vector offset = vecView - center;
	m_fViewerDist = offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2];
}

bool ParticleSystem::GetEmitter( Vector &origin, Vector &angles )
{
	if ( !m_Placement.iTarget )
	{
		origin = m_Placement.origin;
		angles = m_Placement.angles;
		return true;
	}

	// a followed entity: only while it is in this frame's packet (a removed or hidden entity isn't)
	cl_entity_t *pTarget = gEngfuncs.GetEntityByIndex( m_Placement.iTarget );
	cl_entity_t *pLocal = gEngfuncs.GetLocalPlayer();
	if ( !pTarget || !pLocal || !pTarget->model || pTarget->curstate.messagenum < pLocal->curstate.messagenum )
		return false;

	angles = m_Placement.angles + pTarget->angles;

	if ( m_Placement.iAttachment >= 0 && m_Placement.iAttachment < 4 && pTarget->model->type == mod_studio )
	{
		// the studio renderer sets the attachments when it draws the model
		origin = pTarget->attachment[m_Placement.iAttachment];
		return true;
	}

	// the offset turns with the target (the server measured it in the target's frame)
	Vector forward, right, up;
	AngleVectors( pTarget->angles, forward, right, up );
	origin = pTarget->origin + forward * m_Placement.origin.x - right * m_Placement.origin.y + up * m_Placement.origin.z;
	return true;
}

void ParticleSystem::StartOneShot( float flNow )
{
	float flDuration = m_pMainType ? m_pMainType->m_Life.GetInstance() : 0;
	if ( flDuration <= 0 )
		flDuration = 5;
	else if ( flDuration > 120 )
		flDuration = 120;

	m_bOneShot = true;
	m_bOn = true;
	m_flStopTime = flNow + flDuration;
}

void ParticleSystem::CheckKillCondition( void )
{
	if ( !m_iKillCondition || m_bKilled || !m_bOn || !m_bHasEmitter )
		return;

	if ( gEngfuncs.PM_PointContents( m_vecEmitter, NULL ) != m_iKillCondition )
		return;

	// as XashXT: the emitter stops for good (switching it on again does nothing), the particles alive finish
	m_bKilled = true;
	if ( m_bOneShot )
	{
		m_bOn = false;
		m_flStopTime = gEngfuncs.GetClientTime();
	}
}

bool ParticleSystem::UpdateSystem( float frametime )
{
	Vector vecOld = m_vecEmitter;
	m_bHasEmitter = GetEmitter( m_vecEmitter, m_vecEmitterAngles );
	if ( !m_bHasEmitter )
		m_vecEmitter = vecOld;

	// a one-shot copy (Spawn Use) stops emitting after its time, and goes once its particles are gone
	if ( m_bOneShot && m_bOn && gEngfuncs.GetClientTime() >= m_flStopTime )
		m_bOn = false;

	// visible: placed (a followed target is in this frame) and the emitter or the particles are in the PVS
	m_bVisible = m_bHasEmitter;
	if ( m_bVisible && gEngfuncs.pTriAPI->BoxInPVS )
	{
		Vector mins = m_vecEmitter - Vector( 16, 16, 16 );
		Vector maxs = m_vecEmitter + Vector( 16, 16, 16 );
		if ( m_bHasBounds && m_pActiveParticle )
		{
			for ( int i = 0; i < 3; i++ )
			{
				if ( m_vecMins[i] < mins[i] ) mins[i] = m_vecMins[i];
				if ( m_vecMaxs[i] > maxs[i] ) maxs[i] = m_vecMaxs[i];
			}
		}
		m_bVisible = gEngfuncs.pTriAPI->BoxInPVS( mins, maxs ) != 0;
	}

	// out of sight: the system stands still (Spirit did the same, but still drew it); a one-shot copy runs on, so
	// that it ends
	if ( !m_bVisible && !m_bOneShot )
		return true;

	CheckKillCondition();
	const bool bEmit = m_bOn && !m_bKilled;

	if ( bEmit && !m_pMainParticle && m_pMainType && m_bHasEmitter )
	{
		m_pMainParticle = m_pMainType->CreateParticle( this );
		if ( m_pMainParticle )
		{
			m_pMainParticle->m_bBound = true;
			m_pMainParticle->origin = m_vecEmitter;
			m_pMainParticle->velocity = Vector( 0, 0, 0 );
			m_pMainParticle->age_death = -1;	// never die
		}
	}
	else if ( !bEmit && m_pMainParticle )
	{
		m_pMainParticle = NULL;		// it dies in its update (bound, not the main particle any more)
	}

	particle *pParticle = m_pActiveParticle;
	particle *pLast = NULL;
	bool bFirst = true;
	float fMaxSize = 0;

	while ( pParticle )
	{
		if ( UpdateParticle( pParticle, frametime ) )
		{
			// bounds for the PVS test and the distance sort
			if ( bFirst )
			{
				m_vecMins = m_vecMaxs = pParticle->origin;
				bFirst = false;
			}
			else
			{
				for ( int i = 0; i < 3; i++ )
				{
					if ( pParticle->origin[i] < m_vecMins[i] ) m_vecMins[i] = pParticle->origin[i];
					if ( pParticle->origin[i] > m_vecMaxs[i] ) m_vecMaxs[i] = pParticle->origin[i];
				}
			}
			if ( pParticle->m_fSize > fMaxSize )
				fMaxSize = pParticle->m_fSize;

			pLast = pParticle;
			pParticle = pParticle->nextpart;
		}
		else // deactivate it
		{
			particle *pNext = pParticle->nextpart;
			if ( pLast )
				pLast->nextpart = pNext;
			else
				m_pActiveParticle = pNext;

			if ( pParticle == m_pMainParticle )
				m_pMainParticle = NULL;

			FreeParticle( pParticle );
			pParticle = pNext;
		}
	}

	m_bHasBounds = !bFirst;
	if ( m_bHasBounds )
	{
		const Vector vecSize( fMaxSize, fMaxSize, fMaxSize );
		m_vecMins = m_vecMins - vecSize;
		m_vecMaxs = m_vecMaxs + vecSize;
	}

	// a one-shot copy that stopped and has nothing left goes; one whose particles keep spraying each other (a type
	// that sprays itself) goes AURORA_ONESHOT_GRACE seconds after it stopped
	if ( m_bOneShot && !m_bOn && ( !m_pActiveParticle || gEngfuncs.GetClientTime() > m_flStopTime + AURORA_ONESHOT_GRACE ) )
		return false;

	return true;
}

void ParticleSystem::DrawSystem( const Vector &vecView, const Vector &forward, const Vector &right, const Vector &up )
{
	if ( !m_bVisible )
		return;

	for ( particle *pParticle = m_pActiveParticle; pParticle; pParticle = pParticle->nextpart )
	{
		// behind the view
		if ( DotProduct( pParticle->origin - vecView, forward ) < -fabs( pParticle->m_fSize ) * 1.5f )
			continue;

		DrawParticle( pParticle, right, up );
	}
}

// size, color, alpha, frame and spin over the particle's life (an overlay steps with its particle)
void ParticleSystem::StepParticle( particle *part, float frametime )
{
	part->m_fSize += part->m_fSizeStep * frametime;
	part->m_fAlpha += part->m_fAlphaStep * frametime;
	part->m_fRed += part->m_fRedStep * frametime;
	part->m_fGreen += part->m_fGreenStep * frametime;
	part->m_fBlue += part->m_fBlueStep * frametime;
	part->frame += part->m_fFrameStep * frametime;
	if ( part->m_fAngleStep )
	{
		part->m_fAngle = (float)fmod( part->m_fAngle + part->m_fAngleStep * frametime, 360.0 );
		if ( part->m_fAngle < 0 )
			part->m_fAngle += 360;
	}
}

bool ParticleSystem::UpdateParticle( particle *part, float frametime )
{
	if ( frametime <= 0 )
		return true;

	part->age += frametime;

	if ( part->m_bBound )
	{
		// the emitter's main particle
		if ( part != m_pMainParticle )
			return false;	// switched off

		if ( m_bHasEmitter )
		{
			Vector vecMove = m_vecEmitter - part->origin;
			// a jump (a train sent back to its start, a monster teleported) doesn't throw the children
			if ( vecMove.Length() > 256 )
				part->velocity = Vector( 0, 0, 0 );
			else
				part->velocity = vecMove / frametime;
			part->origin = m_vecEmitter;
		}
	}
	else
	{
		// not tied to an entity, check whether it's time to die
		if ( part->age_death >= 0 && part->age > part->age_death )
			return false;

		// apply acceleration and velocity
		Vector vecOldPos = part->origin;
		if ( part->m_fDrag )
			VectorMA( part->velocity, -part->m_fDrag * frametime, part->velocity - part->m_vecWind, part->velocity );
		VectorMA( part->velocity, frametime, part->accel, part->velocity );
		VectorMA( part->origin, frametime, part->velocity, part->origin );

		if ( part->pType->Collides() )
		{
			// the move it just made, against the world and the brush entities (as the weather does)
			pmtrace_t tr;
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecOldPos, part->origin, PM_STUDIO_IGNORE, -1, &tr );
			if ( tr.fraction < 1 && !tr.allsolid )
			{
				if ( part->pType->m_iCollision == COLLISION_DIE )
					return false;

				part->origin = tr.endpos + tr.plane.normal * 0.5f;
				const float bounceforce = DotProduct( tr.plane.normal, part->velocity );
				const float newspeed = 1 - part->pType->m_BounceFriction.GetInstance();
				part->velocity = part->velocity * newspeed;
				VectorMA( part->velocity, -bounceforce * ( newspeed + part->pType->m_Bounce.GetInstance() ), tr.plane.normal, part->velocity );
			}
		}
	}

	// spray children
	ParticleType *pSprayType = part->pType->m_pSprayType;
	if ( pSprayType && part->age_spray > 0 && part->age > part->age_spray )
	{
		int iSprayed = 0;
		while ( part->age > part->age_spray )
		{
			const float fRate = part->pType->m_SprayRate.GetInstance();
			if ( fRate <= 0 )
			{
				part->age_spray = 0;	// no rate: no more children
				break;
			}

			if ( iSprayed >= AURORA_MAX_SPRAY_PER_FRAME )
			{
				part->age_spray = part->age + 1.0f / fRate;	// too far behind: start over from now
				break;
			}

			// how long ago in this frame the child was due: it has moved that long already
			float fLate = part->age - part->age_spray;
			if ( fLate > frametime )
				fLate = frametime;
			part->age_spray += 1.0f / fRate;
			iSprayed++;

			particle *pChild = pSprayType->CreateParticle( this );
			if ( !pChild )
				continue;	// no free particle: the time passes anyway

			pChild->origin = part->origin;
			pChild->velocity = part->velocity;

			const float fSprayForce = part->pType->m_SprayForce.GetInstance();
			if ( fSprayForce )
			{
				// the emitter's angles turn the script's direction (pitch: positive is down, as the angles key)
				const float fSprayPitch = part->pType->m_SprayPitch.GetInstance() + m_vecEmitterAngles.x;
				const float fSprayYaw = part->pType->m_SprayYaw.GetInstance() + m_vecEmitterAngles.y;
				const float fForceCosPitch = fSprayForce * CosLookup( fSprayPitch );
				pChild->velocity.x += CosLookup( fSprayYaw ) * fForceCosPitch;
				pChild->velocity.y += SinLookup( fSprayYaw ) * fForceCosPitch;
				pChild->velocity.z -= SinLookup( fSprayPitch ) * fSprayForce;
			}

			if ( fLate > 0 )
			{
				VectorMA( pChild->origin, fLate, pChild->velocity, pChild->origin );
				pChild->age = fLate;
			}
		}
	}

	for ( particle *pLayer = part; pLayer; pLayer = pLayer->m_pOverlay )
		StepParticle( pLayer, frametime );

	return true;
}

static float Aurora_Clamp01( float f )
{
	return f < 0 ? 0 : ( f > 1 ? 1 : f );
}

const Vector &ParticleSystem::ParticleLight( particle *part )
{
	if ( part->age >= part->m_fLightAge )
	{
		part->m_fLightAge = part->age + AURORA_LIGHT_INTERVAL;

		// the world's lightmap light there (with its light styles), 0 .. 255 each
		float flLight[3] = { 255, 255, 255 };
		if ( gEngfuncs.pTriAPI->LightAtPoint )
		{
			Vector vecPos = part->origin;
			gEngfuncs.pTriAPI->LightAtPoint( vecPos, flLight );
		}
		part->m_vecLight = Vector( Aurora_Clamp01( flLight[0] / 255.0f ), Aurora_Clamp01( flLight[1] / 255.0f ),
			Aurora_Clamp01( flLight[2] / 255.0f ) );
	}
	return part->m_vecLight;
}

void ParticleSystem::DrawParticle( particle *part, const Vector &right, const Vector &up )
{
	const float fSize = part->m_fSize;
	Vector point1, point2, point3, point4;
	Vector origin = part->origin;

	// nothing to draw?
	if ( fSize <= 0 )
		return;

	const float fCosSize = CosLookup( part->m_fAngle ) * fSize;
	const float fSinSize = SinLookup( part->m_fAngle ) * fSize;

	// calculate the four corners of the sprite
	VectorMA( origin, fSinSize, up, point1 );
	VectorMA( point1, -fCosSize, right, point1 );

	VectorMA( origin, fCosSize, up, point2 );
	VectorMA( point2, fSinSize, right, point2 );

	VectorMA( origin, -fSinSize, up, point3 );
	VectorMA( point3, fCosSize, right, point3 );

	VectorMA( origin, -fCosSize, up, point4 );
	VectorMA( point4, -fSinSize, right, point4 );

	int iContents = 0;

	// "lightmodel": every layer takes the light at the particle
	Vector vecLight( 1, 1, 1 );
	if ( m_iLightModel )
		vecLight = ParticleLight( part );

	for ( particle *pDraw = part; pDraw; pDraw = pDraw->m_pOverlay )
	{
		ParticleType *pType = pDraw->pType;
		if ( !pType || pType->m_hSprite == 0 )
			continue;

		if ( pType->m_iDrawCond )
		{
			if ( iContents == 0 )
				iContents = gEngfuncs.PM_PointContents( origin, NULL );

			if ( iContents != pType->m_iDrawCond )
				continue;
		}

		struct model_s *pModel = (struct model_s *)gEngfuncs.GetSpritePointer( pType->m_hSprite );
		if ( !pModel || pModel->numframes <= 0 )
			continue;

		// past the sprite's last frame: loop back
		const int iFrames = pModel->numframes;
		float fFrame = (float)fmod( pDraw->frame, (float)iFrames );
		if ( fFrame < 0 )
			fFrame += iFrames;
		if ( !( fFrame >= 0 && fFrame < iFrames ) )
			fFrame = 0;		// an infinite frame (the script's frame rate summed up) is NaN here
		pDraw->frame = fFrame;
		int iFrame = (int)fFrame;
		if ( iFrame >= iFrames )
			iFrame = iFrames - 1;

		if ( !gEngfuncs.pTriAPI->SpriteTexture( pModel, iFrame ) )
			continue;

		// set the mode before the color: the engine's Color4f depends on it
		gEngfuncs.pTriAPI->RenderMode( pType->m_iRenderMode );
		gEngfuncs.pTriAPI->Color4f( Aurora_Clamp01( pDraw->m_fRed ) * vecLight.x, Aurora_Clamp01( pDraw->m_fGreen ) * vecLight.y,
			Aurora_Clamp01( pDraw->m_fBlue ) * vecLight.z, Aurora_Clamp01( pDraw->m_fAlpha ) );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		gEngfuncs.pTriAPI->Vertex3fv( point1 );

		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		gEngfuncs.pTriAPI->Vertex3fv( point2 );

		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		gEngfuncs.pTriAPI->Vertex3fv( point3 );

		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		gEngfuncs.pTriAPI->Vertex3fv( point4 );
		gEngfuncs.pTriAPI->End();
	}
}
