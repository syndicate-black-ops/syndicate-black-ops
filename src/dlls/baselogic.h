//=========================================================
// CBaseLogic: shared functions used by logic entities such as trigger_changevalue, trigger_condition and
// trigger_entity_iterator (docs/LOGIC_ENTITIES.md). Ported from SevenKewp (https://github.com/wootguy/SevenKewp, Half-Life 1 SDK
// license) dlls/triggers/CBaseLogic.{h,cpp} and CBaseEntity::FindLogicEntity.
//
// SBO: SevenKewp keeps the activator and the caller of the last Use on every entity; here the logic entity keeps
// its own (set by its Use). Fixed buffers instead of std::string / std::vector.
//=========================================================

#ifndef BASELOGIC_H
#define BASELOGIC_H

// generic flags that should be converted to from the child entity's own flags
#define SF_LOGIC_DONT_USE_X 1
#define SF_LOGIC_DONT_USE_Y 2
#define SF_LOGIC_DONT_USE_Z 4

// trigger_copyvalue values
enum float_to_str_conv
{
	FLT2STR_6DP = 0,	// 6 decimal places when converted to string
	FLT2STR_5DP = 1,
	FLT2STR_4DP = 4,
	FLT2STR_3DP = 7,
	FLT2STR_2DP = 10,
	FLT2STR_1DP = 13,
	FLT2STR_ROUND = 16,	// round to nearest whole number when converted to string/int
	FLT2STR_CEIL = 17,
	FLT2STR_FLOOR = 18,
};

#define LOGIC_STRING_LEN	128

class CBaseLogic : public CPointEntity
{
public:
	// combination of SF_LOGIC_DONT_USE_* flags for vectors. Child flags are converted here
	virtual int GetVectorDontUseFlags() { return 0; }

	// The entity with the name, or the !activator / the !caller (start NULL only: there is one of each).
	// SevenKewp: a missing !activator / !caller is the world.
	CBaseEntity *FindLogicEntity( CBaseEntity *pStart, string_t targetname );

	// same as FireTargets but also handles !activator and !caller
	void FireLogicTargets( const char *targetName, USE_TYPE useType, float value );

	// returns a single coordinate if 2x don't-use flags are set, else the length of the vector
	float VectorToFloat( Vector v );

	int FloatToInteger( float v, int flt2str_mode );

	// out: LOGIC_STRING_LEN characters
	void FloatToString( float f, int flt2str_mode, char *out );
	void VectorToString( Vector v, int flt2str_mode, char *out );

	EHANDLE m_hActivator;
	EHANDLE m_hCaller;
};

#endif // BASELOGIC_H
