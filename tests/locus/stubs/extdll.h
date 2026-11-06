// tests/locus: stub of the SDK headers for the Locus value parsing (src/dlls/locus.cpp, LOCUS_FUZZ)
#pragma once
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
typedef int string_t;
struct Vector
{
	float x, y, z;
	Vector() : x( 0 ), y( 0 ), z( 0 ) {}
	Vector( float a, float b, float c ) : x( a ), y( b ), z( c ) {}
	Vector operator+( const Vector &v ) const { return Vector( x + v.x, y + v.y, z + v.z ); }
	Vector operator-( const Vector &v ) const { return Vector( x - v.x, y - v.y, z - v.z ); }
	Vector operator*( float f ) const { return Vector( x * f, y * f, z * f ); }
	float &operator[]( int i ) { return ( &x )[i]; }
	operator float *() { return &x; }
	float Length() const { return sqrtf( x * x + y * y + z * z ); }
};
