#pragma once
#include "extdll.h"
extern const char *g_pStringBase;
inline const char *STRING( string_t s ) { return g_pStringBase + s; }
inline BOOL FStringNull( string_t s ) { return s == 0; }
inline BOOL FStrEq( const char *a, const char *b ) { return !strcmp( a, b ); }
enum ALERT_TYPE { at_console, at_aiconsole };
void ALERT( ALERT_TYPE, const char *fmt, ... );
float RANDOM_FLOAT( float lo, float hi );
void UTIL_StringToVector( float *pVector, const char *pString );
class CBaseEntity;
CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStart, const char *szName );
