//========= SBO 2.0: server menus' model preview =========//
//
// Purpose: the turnable 3D model a server menu with the preview flag shows (docs/SERVER_MENUS.md "Model preview"): the
//          player's own model (or a menu item's player model) with the item's costume layers on its bones, drawn by
//          the client's studio renderer as part of the 3D view, in the screen area the window leaves free.
//
//          Models are loaded by name from the client's own files (IEngineStudio.Mod_ForName, as env_static and the
//          engine's own player models): no precache, nothing from the server but the path. A path that is not on the
//          client's disk, or not a studio model, gives an empty preview. At most servermenu::MAX_PREVIEW_MODELS
//          files per map; the list is forgotten at InitHUD.
//
//          How it is drawn: an entity of this file is added to the frame (HUD_CreateEntities). When the engine draws
//          it, R_StudioDrawModel hands it to CStudioModelRenderer::StudioDrawPreview, which places it at that frame's
//          view: a small copy 10 units in front of the eye (in front of any wall the player stands at), fitted into
//          its frame from its posed vertices (any turn stays inside), on an opaque backer in the window's palette.
//          Hardware renderer: the frame is cleared to the backer color with a scissor, its depth set to the backer's
//          plane, and the model cut to the frame (server_menu_preview_gl.cpp), so it is tested only against its own
//          backer; the frame's pixels are then copied into a texture that the window paints into its own opaque
//          card (ServerMenuPreview_PaintCopy), so nothing drawn between the 3D view and the window shows there.
//          Software renderer: no preview model is drawn (the window sets no spec there: CanClip false); the
//          window paints the backers itself, with the names. StudioPreviewBacker (a TriAPI quad) stays for a draw
//          without the GL scissor.
//          The view model is not drawn while the preview is up.
//
// Plain declarations (both header worlds include it). Implemented in server_menu_preview.cpp (SBO world).
//=================================================================//

#ifndef SBO_CL_DLL_SERVER_MENU_PREVIEW_H
#define SBO_CL_DLL_SERVER_MENU_PREVIEW_H

struct ServerMenuPreviewLayer
{
	char path[64]; //!< "" = none
	int body, skin, sequence;
};

//! A screen rectangle in fractions of the screen (0..1, from the top left)
struct ServerMenuPreviewRect
{
	float x0, y0, x1, y1;
};

//! What the window wants drawn
struct ServerMenuPreviewSpec
{
	bool active;
	ServerMenuPreviewRect frame; //!< the model's frame: it is fitted into it and drawn on an opaque backer there
	unsigned char backer[3];     //!< the backer's color (the window's palette), also the cards' backers
	float yaw;                   //!< degrees the player turned it (0: facing the view)
	ServerMenuPreviewLayer base; //!< layer 0: a player model to show instead of the player's own ("" = his own)
	ServerMenuPreviewLayer layers[3]; //!< layers 1..3: drawn on the base's bones
};

//! One card's small model, turning slowly: its item's player model, else its costume piece as worn (on the bones of
//! the player's own model, only the piece drawn)
struct ServerMenuCardSpec
{
	ServerMenuPreviewRect frame; //!< the card's picture box: the model is fitted into it, on the backer
	ServerMenuPreviewRect clip;  //!< the part of the box inside the scrolled item area (drawn only there)
	float yaw;
	ServerMenuPreviewLayer model;
	bool playerModel; //!< a player model: its idle sequence (else the layer's own sequence, on the player's bones)
};
constexpr int SERVER_MENU_MAX_CARD_MODELS = 24;

//! The window: what to draw (every frame while the window is up; a spec with active false hides it)
void ServerMenuPreview_Set(const ServerMenuPreviewSpec &spec);
//! The window: the visible cards' models this frame (count 0: none)
void ServerMenuPreview_SetCards(const ServerMenuCardSpec *cards, int count);
//! InitHUD: the loaded model list is forgotten (a new map), the preview hidden
void ServerMenuPreview_Reset();
//! True while a preview is drawn this frame (the window shows "empty" when it is not)
bool ServerMenuPreview_HasModel();
//! True when a model can be cut to its rectangle (the hardware renderer: scissor): the cards scrolled partly out of
//! the item area show their visible part, painted into the window from their copies. False on the software renderer:
//! no card models (the cards show their backer).
bool ServerMenuPreview_CanClip();
//! The software renderer (no preview model): a menu with a preview is up, so the view model is left out as it is
//! with the preview on the hardware renderer
void ServerMenuPreview_SetHideViewModel(bool bHide);
//! The window's paint (hardware renderer): the copy of this frame's model frame (slot: the card's index in
//! ServerMenuPreview_SetCards, SERVER_MENU_MAX_CARD_MODELS for the big preview) over the window's opaque card; false
//! when there is none this frame (the opaque fill stays)
bool ServerMenuPreview_PaintCopy(int slot, const ServerMenuPreviewRect &where);

#ifdef SBO_SERVER_MENU_PREVIEW_ENGINE
// SBO world only (engine types)
struct cl_entity_s;
struct model_s;

//! HUD_CreateEntities: adds the preview's entity to the frame
void ServerMenuPreview_CreateEntities();
//! R_StudioDrawModel: is this the preview's entity / should the view model be left out
bool ServerMenuPreview_IsEntity(const struct cl_entity_s *ent);
//! The renderer, after the engine's light for an entity: a preview model gets at least an even light (a model shown
//! where the player stands in the dark, or a dark texture like the teleporter pad, reads like the others)
void ServerMenuPreview_LightFloor(const struct cl_entity_s *ent, struct alight_s *lighting);
bool ServerMenuPreview_HidesViewModel();

//! For StudioDrawPreview: the loaded models of this frame
struct ServerMenuPreviewDraw
{
	struct model_s *base;
	bool drawBase;  //!< false: the base only lends its bones (a card's costume piece is drawn alone, as worn)
	int baseBody, baseSkin, colormap;
	int baseSequence; //!< -1: the model's idle sequence
	int layers;
	struct model_s *layerModel[3];
	int layerBody[3], layerSkin[3], layerSequence[3];
	ServerMenuPreviewRect frame, clip;
	unsigned char backer[3];
	float yaw;
	float fov; //!< horizontal field of view for a 4:3 screen (the engine widens it for wider screens)
	float aspect; //!< screen width / height
	int slot;     //!< the card's index, or SERVER_MENU_MAX_CARD_MODELS for the big preview (its frame copy)
};
//! The posed vertices' box of a draw (model space, pivot at the box's center in x/y) and the largest distance of a
//! vertex from the vertical axis through that pivot: kept per model / body / sequence until the next map
struct ServerMenuPreviewBounds
{
	float mins[3], maxs[3];
	float radius;
};
bool ServerMenuPreview_FindBounds(const ServerMenuPreviewDraw &draw, ServerMenuPreviewBounds &out);
void ServerMenuPreview_StoreBounds(const ServerMenuPreviewDraw &draw, const ServerMenuPreviewBounds &bounds);

//! ent: the entity being drawn (the big preview's or a card's); nullptr: the big preview
bool ServerMenuPreview_GetDraw(const struct cl_entity_s *ent, ServerMenuPreviewDraw &out);
#endif

#endif // SBO_CL_DLL_SERVER_MENU_PREVIEW_H
