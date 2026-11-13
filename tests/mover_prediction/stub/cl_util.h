// Stub for tests/mover_prediction
#ifndef MP_STUB_CL_UTIL_H
#define MP_STUB_CL_UTIL_H
#define VectorAdd(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
#define AngleVectors (*gEngfuncs.pfnAngleVectors)
#endif
