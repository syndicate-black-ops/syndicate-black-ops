//=========================================================
// SBO 2.0: server menus' model preview: see server_menu_preview.h and docs/SERVER_MENUS.md.
//
// SBO world. The window (vgui2/sbo_server_menu.cpp) sets what to draw through sbo::SetServerMenuPreview
// (sbo_server_menu_bridge.cpp), the renderer draws it (CStudioModelRenderer::StudioDrawPreview).
//=========================================================

#define SBO_SERVER_MENU_PREVIEW_ENGINE

#include "server_menu_shared.h"
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "cl_entity.h"
#include "com_model.h"
#include "studio.h"
#include "r_studioint.h"
#include "server_menu_preview.h"
#include "server_menu_preview_gl.h"

#include <string.h>

extern engine_studio_api_t IEngineStudio;

namespace
{
ServerMenuPreviewSpec s_Spec;
cl_entity_t s_Ent;
ServerMenuCardSpec s_Cards[SERVER_MENU_MAX_CARD_MODELS];
int s_nCards = 0;
cl_entity_t s_CardEnt[SERVER_MENU_MAX_CARD_MODELS];

struct LoadedModel
{
	char name[64];
	model_t *model; //!< NULL: not on the client's disk or not a studio model (not looked for again this map)
};
LoadedModel s_Models[servermenu::MAX_PREVIEW_MODELS];

//! Fitted boxes (ServerMenuPreview_FindBounds): what was drawn, its box
struct BoundsKey
{
	model_t *base;
	int drawBase, baseBody, baseSequence, layers;
	model_t *layerModel[3];
	int layerBody[3], layerSequence[3];
};
struct BoundsEntry
{
	BoundsKey key;
	ServerMenuPreviewBounds bounds;
};
constexpr int MAX_BOUNDS = 64;
BoundsEntry s_Bounds[MAX_BOUNDS];
int s_nBounds = 0, s_iNextBounds = 0;

BoundsKey MakeKey(const ServerMenuPreviewDraw &d)
{
	BoundsKey k;
	memset(&k, 0, sizeof(k));
	k.base = d.base;
	k.drawBase = d.drawBase ? 1 : 0;
	k.baseBody = d.drawBase ? d.baseBody : 0;
	k.baseSequence = d.baseSequence;
	k.layers = d.layers;
	for (int i = 0; i < d.layers && i < 3; i++)
	{
		k.layerModel[i] = d.layerModel[i];
		k.layerBody[i] = d.layerBody[i];
		k.layerSequence[i] = d.layerSequence[i];
	}
	return k;
}
int s_nModels = 0;
bool s_bFullReported = false;

//! A model by name from the client's own files, at most MAX_PREVIEW_MODELS different names per map
model_t *LoadByName(const char *path)
{
	if (!path || !path[0] || strlen(path) >= sizeof(s_Models[0].name))
		return NULL;
	for (int i = 0; i < s_nModels; i++)
	{
		if (!strcmp(s_Models[i].name, path))
			return s_Models[i].model;
	}
	if (s_nModels >= servermenu::MAX_PREVIEW_MODELS)
	{
		if (!s_bFullReported)
		{
			s_bFullReported = true;
			gEngfuncs.Con_Printf("Server menu preview: more than %d models on this map; %s is not shown\n",
				servermenu::MAX_PREVIEW_MODELS, path);
		}
		return NULL;
	}
	LoadedModel &m = s_Models[s_nModels++];
	strcpy(m.name, path); // length checked above
	m.model = NULL;
	if (IEngineStudio.Mod_ForName)
	{
		// crash_if_missing 0: NULL for a file that is not there (env_static loads its models the same way)
		model_t *mod = IEngineStudio.Mod_ForName(path, 0);
		if (mod && mod->type == mod_studio)
			m.model = mod;
	}
	if (!m.model)
		gEngfuncs.Con_DPrintf("Server menu preview: %s is not on this client; empty preview\n", path);
	return m.model;
}

//! The local player's own model, as the engine draws him (models/player/<model>/<model>.mdl, loaded by the engine)
model_t *LocalPlayerModel(int &body, int &skin, int &colormap)
{
	body = skin = colormap = 0;
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if (!local || local->index < 1 || local->index > gEngfuncs.GetMaxClients() || !IEngineStudio.SetupPlayerModel)
		return NULL;
	model_t *mod = IEngineStudio.SetupPlayerModel(local->index - 1);
	if (!mod || mod->type != mod_studio)
		return NULL;
	body = local->curstate.body;
	skin = local->curstate.skin;
	hud_player_info_t info;
	memset(&info, 0, sizeof(info));
	gEngfuncs.pfnGetPlayerInfo(local->index, &info);
	colormap = (info.topcolor & 0xFF) | ((info.bottomcolor & 0xFF) << 8);
	return mod;
}
} // namespace

void ServerMenuPreview_Set(const ServerMenuPreviewSpec &spec)
{
	s_Spec = spec;
	s_Spec.base.path[sizeof(s_Spec.base.path) - 1] = 0;
	for (ServerMenuPreviewLayer &l : s_Spec.layers)
		l.path[sizeof(l.path) - 1] = 0;
}

void ServerMenuPreview_SetCards(const ServerMenuCardSpec *cards, int count)
{
	s_nCards = 0;
	for (int i = 0; cards && i < count && i < SERVER_MENU_MAX_CARD_MODELS; i++)
	{
		s_Cards[s_nCards] = cards[i];
		s_Cards[s_nCards].model.path[sizeof(s_Cards[0].model.path) - 1] = 0;
		s_nCards++;
	}
}

static bool s_bHideViewModel = false; // ServerMenuPreview_SetHideViewModel

void ServerMenuPreview_Reset()
{
	memset(&s_Spec, 0, sizeof(s_Spec));
	s_bHideViewModel = false;
	s_nCards = 0;
	s_nModels = 0;
	s_bFullReported = false;
	s_nBounds = s_iNextBounds = 0;
}

bool ServerMenuPreview_FindBounds(const ServerMenuPreviewDraw &draw, ServerMenuPreviewBounds &out)
{
	const BoundsKey k = MakeKey(draw);
	for (int i = 0; i < s_nBounds; i++)
	{
		if (!memcmp(&s_Bounds[i].key, &k, sizeof(k)))
		{
			out = s_Bounds[i].bounds;
			return true;
		}
	}
	return false;
}

void ServerMenuPreview_StoreBounds(const ServerMenuPreviewDraw &draw, const ServerMenuPreviewBounds &bounds)
{
	BoundsEntry &e = s_Bounds[s_iNextBounds];
	e.key = MakeKey(draw);
	e.bounds = bounds;
	s_iNextBounds = (s_iNextBounds + 1) % MAX_BOUNDS;
	if (s_nBounds < MAX_BOUNDS)
		s_nBounds++;
}

bool ServerMenuPreview_GetDraw(const cl_entity_t *ent, ServerMenuPreviewDraw &out)
{
	memset(&out, 0, sizeof(out));
	out.baseSequence = -1;
	out.drawBase = true;
	if (!s_Spec.active)
		return false;
	out.fov = gHUD.m_iFOV > 0 ? (float)gHUD.m_iFOV : 90.0f;
	out.aspect = ScreenHeight > 0 ? (float)ScreenWidth / ScreenHeight : 4.0f / 3.0f;
	memcpy(out.backer, s_Spec.backer, sizeof(out.backer));
	if (ent && ent >= s_CardEnt && ent < s_CardEnt + SERVER_MENU_MAX_CARD_MODELS)
	{
		const int n = (int)(ent - s_CardEnt);
		if (n >= s_nCards)
			return false;
		const ServerMenuCardSpec &c = s_Cards[n];
		out.slot = n;
		model_t *mod = LoadByName(c.model.path);
		if (!mod)
			return false;
		out.frame = c.frame;
		out.clip = c.clip;
		out.yaw = c.yaw;
		if (!c.playerModel)
		{
			// a costume piece sits right only on a player's bones: its own sequence's root bone points anywhere
			// (the packed costume models turn it per item), so it is drawn as worn by the player's own model
			int body, skin, colormap;
			model_t *player = LocalPlayerModel(body, skin, colormap);
			if (player)
			{
				out.base = player;
				out.drawBase = false;
				out.layers = 1;
				out.layerModel[0] = mod;
				out.layerBody[0] = c.model.body;
				out.layerSkin[0] = c.model.skin;
				out.layerSequence[0] = c.model.sequence;
				return true;
			}
		}
		out.base = mod;
		out.baseBody = c.model.body;
		out.baseSkin = c.model.skin;
		out.baseSequence = c.playerModel ? -1 : c.model.sequence;
		return true;
	}
	if (s_Spec.base.path[0])
	{
		out.base = LoadByName(s_Spec.base.path);
		out.baseBody = s_Spec.base.body;
		out.baseSkin = s_Spec.base.skin;
	}
	else
		out.base = LocalPlayerModel(out.baseBody, out.baseSkin, out.colormap);
	if (!out.base)
		return false;
	for (const ServerMenuPreviewLayer &l : s_Spec.layers)
	{
		model_t *mod = LoadByName(l.path);
		if (!mod)
			continue;
		out.layerModel[out.layers] = mod;
		out.layerBody[out.layers] = l.body;
		out.layerSkin[out.layers] = l.skin;
		out.layerSequence[out.layers] = l.sequence;
		out.layers++;
	}
	out.frame = s_Spec.frame;
	out.clip = s_Spec.frame;
	out.slot = SERVER_MENU_MAX_CARD_MODELS;
	out.yaw = s_Spec.yaw;
	return true;
}

bool ServerMenuPreview_CanClip()
{
	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() && ServerMenuPreviewGL_Available();
}

bool ServerMenuPreview_PaintCopy(int slot, const ServerMenuPreviewRect &where)
{
	return ServerMenuPreview_CanClip() &&
	       ServerMenuPreviewGL_DrawCopy(slot, gEngfuncs.GetClientTime(), where.x0, where.y0, where.x1, where.y1);
}

bool ServerMenuPreview_HasModel()
{
	ServerMenuPreviewDraw draw;
	return ServerMenuPreview_GetDraw(nullptr, draw);
}

void ServerMenuPreview_CreateEntities()
{
	if (!s_Spec.active)
		return;
	// the cards' models (each loaded by name; a card whose file is missing draws nothing)
	for (int i = 0; i < s_nCards; i++)
	{
		ServerMenuPreviewDraw card;
		if (!ServerMenuPreview_GetDraw(&s_CardEnt[i], card))
			continue;
		cl_entity_t *e = &s_CardEnt[i];
		memset(e, 0, sizeof(*e));
		e->model = card.base;
		e->curstate.rendermode = kRenderNormal;
		e->curstate.renderamt = 255;
		e->curstate.scale = 1.0f;
		e->curstate.framerate = 1.0f;
		gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, e);
	}
	ServerMenuPreviewDraw draw;
	if (!ServerMenuPreview_GetDraw(&s_Ent, draw))
		return;
	// StudioDrawPreview places it at the view of the frame it is drawn in; until then in front of the eye
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	memset(&s_Ent, 0, sizeof(s_Ent));
	s_Ent.model = draw.base;
	s_Ent.curstate.rendermode = kRenderNormal;
	s_Ent.curstate.renderamt = 255;
	s_Ent.curstate.scale = 1.0f;
	s_Ent.curstate.framerate = 1.0f;
	if (local)
	{
		VectorCopy(local->origin, s_Ent.origin);
		VectorCopy(local->origin, s_Ent.curstate.origin);
	}
	gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, &s_Ent);
}

void ServerMenuPreview_LightFloor(const cl_entity_t *ent, alight_t *lighting)
{
	if (!lighting || !ServerMenuPreview_IsEntity(ent))
		return;
	if (lighting->ambientlight < 96)
		lighting->ambientlight = 96;
	if (lighting->shadelight < 160)
		lighting->shadelight = 160;
	lighting->color[0] = lighting->color[1] = lighting->color[2] = 1.0f;
}

bool ServerMenuPreview_IsEntity(const cl_entity_t *ent)
{
	return ent && s_Spec.active &&
	       (ent == &s_Ent || (ent >= s_CardEnt && ent < s_CardEnt + SERVER_MENU_MAX_CARD_MODELS));
}

void ServerMenuPreview_SetHideViewModel(bool bHide)
{
	s_bHideViewModel = bHide;
}

bool ServerMenuPreview_HidesViewModel()
{
	return s_Spec.active || s_bHideViewModel;
}
