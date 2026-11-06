//=========================================================
// SBO 2.0: point_worldtext: the depth of the letters (worldtext_gl.cpp). docs/POINT_WORLDTEXT.md.
//=========================================================

#if !defined( WORLDTEXT_GL_H )
#define WORLDTEXT_GL_H

// Around the texts of a view (HUD_DrawNormalTriangles): texture unit 0 active with 2D texturing, unit 1 off; End puts
// back what Begin read. Nothing changes when that is the state already, without a GL context or with one texture unit.
void WorldTextGL_Begin( void );
void WorldTextGL_End( void );

// After the triangle API's RenderMode for a depth-only copy of the letters: no color, depth written where a glyph
// pixel's alpha (texture times vertex color) is over one half. False (nothing changed: draw no copy) without a GL
// context or a function.
bool WorldTextGL_DepthBegin( void );
// After the copy: the color and depth write masks and the alpha test as they were before WorldTextGL_DepthBegin.
void WorldTextGL_DepthEnd( void );

#endif // WORLDTEXT_GL_H
