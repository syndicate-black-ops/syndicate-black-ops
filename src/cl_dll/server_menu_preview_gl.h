//=========================================================
// SBO 2.0: server menus' model preview: the hardware renderer's backer and frame cut (server_menu_preview_gl.cpp).
//=========================================================

#ifndef SBO_CL_DLL_SERVER_MENU_PREVIEW_GL_H
#define SBO_CL_DLL_SERVER_MENU_PREVIEW_GL_H

//! A GL context with the functions used here (false on the software renderer)
bool ServerMenuPreviewGL_Available();

//! Before a preview model: the screen rectangle (fractions of the screen from the top left) is filled with the
//! backer color and its depth set to the plane backerDist units in front of the eye (so the world drawn there is
//! gone, the model is depth tested only against its own backer, and what the engine draws later behind that plane
//! stays hidden), and a scissor cuts everything drawn until ServerMenuPreviewGL_End to the rectangle.
//! False without a GL context (nothing changed).
bool ServerMenuPreviewGL_Begin(float x0, float y0, float x1, float y1, const unsigned char rgb[3], float backerDist);
//! After the model: the scissor as it was, and the frame's pixels (backer and model) copied into the slot's texture,
//! stamped with the client time: the window paints that copy over its own opaque card (ServerMenuPreviewGL_DrawCopy),
//! so nothing drawn between the 3D view and the window (other HUD panels) shows in the card
void ServerMenuPreviewGL_End(int slot, float stamp);
//! In the window's paint (VGUI2, the same GL context): the slot's copy at the pixels it was taken from, if it was
//! taken at the client time now (within 0.25 s). False: nothing drawn (the caller's opaque fill stays)
//! where: the card's frame now (fractions of the screen from the top left); a copy taken elsewhere (the cards moved
//! since: scrolled, another tab) is not drawn
bool ServerMenuPreviewGL_DrawCopy(int slot, float now, float x0, float y0, float x1, float y1);
constexpr int SERVER_MENU_PREVIEW_SLOTS = 25; //!< 24 cards and the big preview

#endif // SBO_CL_DLL_SERVER_MENU_PREVIEW_GL_H
