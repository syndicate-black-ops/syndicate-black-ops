//=========================================================
// SBO 2.0 player head scale ("headscale" key, docs/MODEL_SCALE.md "Head scale"), shared by the client (the drawn
// head, StudioModelRenderer.cpp) and the server (the head hitbox, animation.cpp).
//
// The head is the bone named "Bip01 Head" and every bone below it (agentf's hair dummies). It is scaled around the
// head bone's origin (the top of the neck): the head grows up and out from the neck, the neck and body stay. The same
// bones and the same point on both sides, so the hitbox matches the drawn head. The player models' head vertices are
// skinned to that bone (agent / hunted 109 of 412, agentf 134 of 506; valve's player.mdl, the server's hitbox model,
// has the head hitbox, group 1, on it).
//
// Message "HeadScale" (server -> client): byte player index (1..32), short scale x HEADSCALE_UNIT (unsigned).
//=========================================================

#ifndef HEAD_SCALE_SHARED_H
#define HEAD_SCALE_SHARED_H

#include <string.h>

#define HEADSCALE_BONE_NAME		"Bip01 Head"
#define HEADSCALE_UNIT			256.0f		// the message's fixed point: 1/256 steps
#define HEADSCALE_MIN			0.25f
#define HEADSCALE_MAX			4.0f

// the head bone's index in hdr (cached: piCache holds the last index found; it is checked by the bone's name, so a
// cache used for several models looks again only when the model changed), -1 when the model has none
inline int HeadScale_FindBone( const studiohdr_t *hdr, int *piCache )
{
	if ( !hdr || hdr->numbones <= 0 || hdr->numbones > MAXSTUDIOBONES )
		return -1;

	const mstudiobone_t *pbones = (const mstudiobone_t *)( (const byte *)hdr + hdr->boneindex );

	if ( piCache && *piCache >= 0 && *piCache < hdr->numbones && !strcmp( pbones[*piCache].name, HEADSCALE_BONE_NAME ) )
		return *piCache;

	int iFound = -1;
	for ( int i = 0; i < hdr->numbones; i++ )
	{
		if ( !strcmp( pbones[i].name, HEADSCALE_BONE_NAME ) )
		{
			iFound = i;
			break;
		}
	}

	if ( piCache && iFound >= 0 )
		*piCache = iFound;
	return iFound;
}

// Scales the head bone and the bones below it around the head bone's origin: each one's position moves to
// head + flPosScale x (position - head), its axes are multiplied by flAxisScale. pValid: the bones the caller's
// matrices hold (NULL: all); the head's own origin is read only when the head bone is one of them.
inline void HeadScale_ScaleBones( float (*bones)[3][4], const studiohdr_t *hdr, int iHead, float flPosScale,
	float flAxisScale, const bool *pValid )
{
	if ( !bones || !hdr || iHead < 0 || iHead >= hdr->numbones || hdr->numbones > MAXSTUDIOBONES )
		return;
	if ( pValid && !pValid[iHead] )
		return;

	const mstudiobone_t *pbones = (const mstudiobone_t *)( (const byte *)hdr + hdr->boneindex );
	bool inHead[ MAXSTUDIOBONES ];
	float head[3] = { bones[iHead][0][3], bones[iHead][1][3], bones[iHead][2][3] };

	for ( int i = 0; i < hdr->numbones; i++ )
	{
		int parent = pbones[i].parent;
		// a model's bones come after their parent (studiomdl); anything else is not followed
		inHead[i] = ( i == iHead ) || ( i > iHead && parent >= 0 && parent < i && inHead[parent] );

		if ( !inHead[i] || ( pValid && !pValid[i] ) )
			continue;

		for ( int r = 0; r < 3; r++ )
		{
			bones[i][r][0] *= flAxisScale;
			bones[i][r][1] *= flAxisScale;
			bones[i][r][2] *= flAxisScale;
			bones[i][r][3] = head[r] + ( bones[i][r][3] - head[r] ) * flPosScale;
		}
	}
}

#endif // HEAD_SCALE_SHARED_H
