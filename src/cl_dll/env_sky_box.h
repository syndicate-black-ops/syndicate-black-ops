//=========================================================
// SBO 2.1 env_sky: the map's 2D skybox on the env_sky faces while the 3D sky is off (env_sky_box.cpp).
// docs/ENV_SKY.md.
//=========================================================

#if !defined( ENV_SKY_BOX_H )
#define ENV_SKY_BOX_H

// HUD_DrawNormalTriangles: on a map with an env_sky whose 3D sky is off, the 2D skybox where the view drew nothing
void EnvSkyBox_Draw( void );

// HUD_DrawNormalTriangles in the sky pass: while the env_sky that is on has a sky fog, its 2D skybox fogged too
void EnvSkyBox_DrawSkyFog( void );

#endif // ENV_SKY_BOX_H
