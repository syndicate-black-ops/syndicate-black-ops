//=========================================================
// SBO 2.0: the spectator's map overview keeps its own field of view when the frame is drawn zoomed
// (overview_gl.cpp, CHudSpectator::DrawOverview).
//=========================================================

#if !defined( OVERVIEW_GL_H )
#define OVERVIEW_GL_H

// Before the overview is drawn: the projection widened by scale (> 1: the picture's x and y divided by it), so a
// frame drawn with a narrower field of view shows the overview as at the wider one. False without a GL context or a
// function (software renderer: nothing changed).
bool OverviewGL_Begin( float scale );
// After the overview: the projection as it was.
void OverviewGL_End( void );

#endif // OVERVIEW_GL_H
