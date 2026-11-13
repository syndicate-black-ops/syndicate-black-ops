//=========================================================
// SBO 2.0 trigger_objective (docs/OBJECTIVES.md): the "Objective" user message, server dlls/trigger_objective.cpp,
// client cl_dll/objective_core.cpp (parser and list) and cl_dll/vgui2/hud_objectives.cpp (HUD list, notice, the full
// list in the scoreboard and the map briefing). One reliable message per objective change, to every client; a client
// that (re)initializes its HUD gets OBJECTIVE_SLOT_CLEAR and then every shown objective through the send queue.
//
//   BYTE   slot: 0 .. OBJECTIVE_MAX - 1 (the objective's place in the map's list); OBJECTIVE_SLOT_CLEAR: forget every
//          objective (nothing follows)
//   BYTE   state (OBJECTIVE_STATE_MASK) | OBJECTIVE_EV_* | OBJECTIVE_SILENT. State OBJECTIVE_HIDDEN: the client forgets
//          the slot (nothing follows)
//   SHORT  order: the mapper's list position (0 = none: after the numbered ones, in the order they became active)
//   SHORT  sequence: when it became active (1, 2, ... in the map's order of events; 0 = never active, e.g. completed
//          at map start)
//   BYTE   counter value, BYTE counter target (target 0 = no counter; a group: its parts done, its parts)
//   BYTE   the group's slot (a part: "partof"), OBJECTIVE_NO_PARENT: none
//   STRING text: the objective's text, or "#NAME" for the titles.txt message NAME (at most OBJECTIVE_MAX_TEXT - 1 bytes)
//=========================================================

#ifndef OBJECTIVE_SHARED_H
#define OBJECTIVE_SHARED_H

#define OBJECTIVE_MSG_NAME		"Objective"

#define OBJECTIVE_MAX			32		// trigger_objective entities per map (more: removed at map start, a console line)
#define OBJECTIVE_MAX_TEXT		128		// text with its terminator
#define OBJECTIVE_MAX_COUNT		255		// counter target
#define OBJECTIVE_MAX_ORDER		32767

#define OBJECTIVE_SLOT_CLEAR	255
#define OBJECTIVE_NO_PARENT		255

#define OBJECTIVE_HIDDEN		0
#define OBJECTIVE_ACTIVE		1
#define OBJECTIVE_COMPLETED		2
#define OBJECTIVE_STATE_MASK	3

#define OBJECTIVE_EV_NEW		0x10	// it became active now: the "NEW OBJECTIVE" notice and its sound (not if silent)
#define OBJECTIVE_EV_DONE		0x20	// it was completed now: the OBJECTIVE COMPLETE notice (not if silent); the line
										// shows it ticked, then leaves the compact list
#define OBJECTIVE_EV_COUNT		0x40	// its counter moved now
#define OBJECTIVE_SILENT		0x80	// the entity's flag 1: no notice, no sound

#define OBJECTIVE_MSG_MAX_BYTES	( 1 + 1 + 2 + 2 + 1 + 1 + 1 + OBJECTIVE_MAX_TEXT )

#endif // OBJECTIVE_SHARED_H
