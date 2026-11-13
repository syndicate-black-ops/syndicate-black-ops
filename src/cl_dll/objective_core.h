//=========================================================
// SBO 2.0 trigger_objective, client side without engine or VGUI2 headers: the "Objective" message parser, the
// objective list with its groups, what the panel shows (compact and full lines, fades, the NEW OBJECTIVE /
// OBJECTIVE COMPLETE notices). tests/objective/run_test.sh runs it with ASan/UBSan on good, cut and hostile messages.
// Format: game_shared/objective_shared.h; docs/OBJECTIVES.md. The panel (cl_dll/vgui2/hud_objectives.cpp) draws from
// it.
//=========================================================

#if !defined( OBJECTIVE_CORE_H )
#define OBJECTIVE_CORE_H

#include "../game_shared/objective_shared.h"

namespace objective
{
// What the panel does with the list (seconds of HUD time: the clock stands still while the panel can't be seen)
constexpr int COMPACT_LINES = 5;			// the compact panel: at most this many lines, then "+N more"
constexpr float DONE_HOLD = 4.0f;			// a completed line stays this long in the compact panel (ticked) ...
constexpr float DONE_FADE = 1.5f;			// ... then fades out over this
constexpr float NOTICE_FADE_IN = 0.2f;		// a notice slides / fades in, holds, fades out
constexpr float NOTICE_HOLD = 2.4f;
constexpr float NOTICE_FADE_OUT = 0.4f;
constexpr int NOTICE_QUEUE = 8;				// notices waiting (more at once: the oldest waiting is dropped)

enum NoticeKind
{
	NOTICE_NEW = 0,		// NEW OBJECTIVE
	NOTICE_DONE,		// OBJECTIVE COMPLETE
};

struct Message
{
	int		slot;			// 0 .. OBJECTIVE_MAX - 1, or OBJECTIVE_SLOT_CLEAR
	int		state;			// OBJECTIVE_HIDDEN / _ACTIVE / _COMPLETED
	int		flags;			// OBJECTIVE_EV_* | OBJECTIVE_SILENT
	int		order;
	int		sequence;
	int		count;
	int		target;
	int		parent;			// the group's slot, -1 = none
	char	text[ OBJECTIVE_MAX_TEXT ];
};

// Reads one message. False: cut short, an unknown slot or state; nothing usable.
bool Parse( const unsigned char *data, int size, Message &out );

struct Entry
{
	bool	used;
	int		state;
	int		order;
	int		sequence;
	int		count;
	int		target;			// 0 = no counter (a group: its parts)
	int		parent;			// the group's slot, -1 = none
	bool	silent;
	float	doneTime;		// HUD clock when it was completed in front of this client; < 0: not on the compact list
	char	text[ OBJECTIVE_MAX_TEXT ];
};

// One line of the panel: an objective, or a part under its group
struct Line
{
	int		slot;
	bool	part;
};

class List
{
public:
	List() { Reset(); }

	// a new map or connection: nothing
	void	Reset( void );
	void	Apply( const Message &msg );
	// advances the HUD clock by dt seconds while the panel area can be seen (else it stands still: a notice or a
	// completed line waits for the player to see it). True: a notice has just begun (the caller plays its sound).
	bool	Tick( float dt, bool visible );

	const Entry &Get( int slot ) const { return m_Entries[ slot ]; }
	int		Revision( void ) const { return m_iRevision; }	// changes with every change of the list
	int		ActiveCount( void ) const;	// active objectives, groups counted once (their parts not)
	int		ShownCount( void ) const;	// active + completed, any
	int		TopCount( int &done ) const;	// objectives not under a group (done: completed ones)
	int		NewestActive( void ) const;	// the most recently activated active objective not under a group, -1: none

	// The compact panel: active objectives with their active parts, and completed ones still fading, at most
	// maxLines lines; more: objectives left out. The full panel: every active objective with all its parts, then the
	// completed ones; at most maxLines, more as above. Lines in the list's order: numbered ("order" > 0) by number,
	// then in the order they became active; parts the same under their group.
	int		CompactLines( Line *out, int maxLines, int &more ) const;
	int		FullLines( Line *out, int maxLines, int &more ) const;
	// a compact line's opacity, 0 .. 1 (completed lines fade out)
	float	RowAlpha( int slot ) const;

	// the notice shown now: false if none; slot, kind and opacity 0 .. 1
	bool	Notice( int &slot, int &kind, float &alpha ) const;

	float	Clock( void ) const { return m_flClock; }

	// slots in the list's order (every used one)
	int		AllRows( int *slots, int max ) const;

private:
	bool	Before( int a, int b ) const;	// display order
	void	Sort( int *slots, int n ) const;
	bool	IsTop( int slot ) const;		// not under a shown group
	int		Parts( int group, int *slots, int max ) const;	// its shown parts, in order
	void	DropNotice( int slot );
	void	QueueNotice( int slot, int kind );

	Entry	m_Entries[ OBJECTIVE_MAX ];
	int		m_iRevision;
	float	m_flClock;
	int		m_Notices[ NOTICE_QUEUE ];	// waiting, the current one first
	int		m_NoticeKinds[ NOTICE_QUEUE ];
	int		m_iNotices;
	float	m_flNoticeStart;			// < 0: the first in the queue has not begun
};
} // namespace objective

#endif // OBJECTIVE_CORE_H
