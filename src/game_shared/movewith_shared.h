//=========================================================
// Entity parenting (movewith), shared by the server (dlls/movewith.cpp) and the client (cl_dll/movewith_client.cpp):
// the iuser4 encoding of a client-composed child and the pose math. docs/MOVEWITH.md.
//
// Rotation matrices use Quake's AngleMatrix layout (columns forward, left, up), the way brush entities and sprites
// are drawn and collided. Studio models are drawn with the pitch negated (StudioSetUpTransform), so callers flip the
// pitch of a studio model's angles before MW_AngleMatrix and after MW_MatrixAngles.
//=========================================================
#ifndef MOVEWITH_SHARED_H
#define MOVEWITH_SHARED_H

#include <math.h>
#include <string.h>

// entity_state_t::iuser4 of a child the client places on its parent (delta.lst: 14 bits). 0 = not composed: the
// state's origin and angles are the entity's own. Otherwise they are the child's constant local pose: origin in the
// parent's frame, rotation as Quake angles (no studio pitch flip).
#define MOVEWITH_STATE_PARENT_MASK		0x07FF	// parent entity index (1 .. 2047)
#define MOVEWITH_STATE_ATTACHMENT_SHIFT	11		// 1 .. 4: on that attachment of the parent's studio model; 0: on its origin
#define MOVEWITH_STATE_ATTACHMENT_MASK	0x07

typedef float mwmatrix_t[3][3];

#define MW_PI	3.14159265358979323846

inline void MW_AngleMatrix( const float *angles, mwmatrix_t m )
{
	float a = angles[1] * (float)( MW_PI / 180.0 );
	float sy = sinf( a ), cy = cosf( a );
	a = angles[0] * (float)( MW_PI / 180.0 );
	float sp = sinf( a ), cp = cosf( a );
	a = angles[2] * (float)( MW_PI / 180.0 );
	float sr = sinf( a ), cr = cosf( a );

	m[0][0] = cp * cy;
	m[1][0] = cp * sy;
	m[2][0] = -sp;
	m[0][1] = sr * sp * cy - cr * sy;
	m[1][1] = sr * sp * sy + cr * cy;
	m[2][1] = sr * cp;
	m[0][2] = cr * sp * cy + sr * sy;
	m[1][2] = cr * sp * sy - sr * cy;
	m[2][2] = cr * cp;
}

// a - b, wrapped to -180 .. 180
inline float MW_AngleDiff( float a, float b )
{
	float d = fmodf( a - b, 360.0f );

	if ( d >= 180.0f )
		d -= 360.0f;
	else if ( d < -180.0f )
		d += 360.0f;
	return d;
}

inline float MW_AnglesDistance( const float *a, const float *b )
{
	return fabsf( MW_AngleDiff( a[0], b[0] ) ) + fabsf( MW_AngleDiff( a[1], b[1] ) ) + fabsf( MW_AngleDiff( a[2], b[2] ) );
}

// Angles of a rotation matrix. Of the two Euler solutions the one nearer to "near" is taken, each angle unwrapped to
// within 180 degrees of it, so something turning about any axis gets small, continuous angle steps.
inline void MW_MatrixAngles( const mwmatrix_t m, const float *nearAngles, float *out )
{
	float a1[3], a2[3];
	float cp = sqrtf( m[0][0] * m[0][0] + m[1][0] * m[1][0] );
	const float toDeg = (float)( 180.0 / MW_PI );

	if ( cp > 0.001f )
	{
		a1[0] = atan2f( -m[2][0], cp ) * toDeg;
		a1[1] = atan2f( m[1][0], m[0][0] ) * toDeg;
		a1[2] = atan2f( m[2][1], m[2][2] ) * toDeg;
	}
	else
	{
		a1[0] = ( m[2][0] < 0 ) ? 90.0f : -90.0f;
		a1[1] = atan2f( -m[0][1], m[1][1] ) * toDeg;
		a1[2] = 0;
	}

	a2[0] = 180.0f - a1[0];
	a2[1] = a1[1] + 180.0f;
	a2[2] = a1[2] + 180.0f;

	const float *a = ( MW_AnglesDistance( a2, nearAngles ) < MW_AnglesDistance( a1, nearAngles ) ) ? a2 : a1;

	for ( int i = 0; i < 3; i++ )
		out[i] = nearAngles[i] + MW_AngleDiff( a[i], nearAngles[i] );
}

inline void MW_MatMul( const mwmatrix_t a, const mwmatrix_t b, mwmatrix_t out )
{
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
}

// a transposed, times b
inline void MW_MatTMul( const mwmatrix_t a, const mwmatrix_t b, mwmatrix_t out )
{
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			out[i][j] = a[0][i] * b[0][j] + a[1][i] * b[1][j] + a[2][i] * b[2][j];
}

inline void MW_Rotate( const mwmatrix_t m, const float *v, float *out )
{
	float r[3];

	for ( int i = 0; i < 3; i++ )
		r[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
	out[0] = r[0]; out[1] = r[1]; out[2] = r[2];
}

// m transposed times v (into the frame of m)
inline void MW_IRotate( const mwmatrix_t m, const float *v, float *out )
{
	float r[3];

	for ( int i = 0; i < 3; i++ )
		r[i] = m[0][i] * v[0] + m[1][i] * v[1] + m[2][i] * v[2];
	out[0] = r[0]; out[1] = r[1]; out[2] = r[2];
}

inline void MW_MatCopy( const mwmatrix_t in, mwmatrix_t out )
{
	memcpy( out, in, sizeof( mwmatrix_t ) );
}

#endif // MOVEWITH_SHARED_H
