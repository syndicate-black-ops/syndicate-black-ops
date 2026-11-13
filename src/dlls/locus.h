//=========================================================
// SBO 2.0 Locus system (docs/LOCUS.md): keys that take a literal value or the name of an entity that computes one.
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) dlls/locus.cpp,
// itself from Spirit of Half-Life (Laurie Cheers). In the docs and FGD:
//   [LP] position: "x y z", "x y z .. x y z" (random between), or an entity: its CalcPosition (origin by default)
//   [LV] velocity / vector: as [LP], CalcVelocity (velocity by default)
//   [LR] number: "5", "1..3" (random between, each side may be an entity), or an entity: its CalcRatio
// Entity names may be "!activator" / "*locus" (the activator of the chain), "*player" (the activator when it is a
// player: Featureful's multiplayer rule; Spirit took the first client).
//=========================================================

#ifndef LOCUS_H
#define LOCUS_H

class CBaseEntity;

// a literal number or vector starts with a digit or a minus sign
BOOL		Locus_IsLikelyNumber( const char *szText );

// The entity named szName after pStart, where "!activator" / "*locus" and "*player" are the activator
CBaseEntity *Locus_FindEntity( CBaseEntity *pStart, const char *szName, CBaseEntity *pActivator );

// pEntity: the entity whose key it is (for console notes). FALSE: no value (bad key, a calc entity failed); the
// result is left alone. Results are finite (a NaN or infinite result is a failure).
BOOL		TryCalcLocus_Position( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, BOOL showError = TRUE );
BOOL		TryCalcLocus_Velocity( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, BOOL showError = TRUE );
BOOL		TryCalcLocus_Ratio( CBaseEntity *pLocus, const char *szText, float &result, BOOL showError = TRUE );
BOOL		TryCalcLocus_Color( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, BOOL showError = TRUE );

// A Locus key that names what to work from (the keys whose FGD default is "*locus") means "*locus" when it is blank
// or left out: Locus_KeyOrLocus reads it so, Locus_DefaultToLocus stores it so (at spawn)
const char	*Locus_KeyOrLocus( string_t iszKey );
void		Locus_DefaultToLocus( string_t &iszKey );

// Featureful UTIL_StringToRandomVector: "x y z" or "x y z .. x y z"
void		Locus_StringToRandomVector( float *pVector, const char *pString );

#endif // LOCUS_H
