//=========================================================
// SBO 2.0 inventory, client side: the "Inventory" message and the two item lists, without engine headers
// (tests/inventory/run_test.sh runs them with ASan/UBSan on good, cut and hostile messages). Format:
// game_shared/inventory_shared.h; docs/INVENTORY.md.
//=========================================================

#if !defined( INVENTORY_CORE_H )
#define INVENTORY_CORE_H

#include "inventory_shared.h"

namespace inventory
{
struct Item
{
	char	name[ INVENTORY_NAME_LEN ];	// "" = a free slot (the used ones come first, in the order they came)
	char	icon[ INVENTORY_NAME_LEN ];
	int		count;						// 1 .. INVENTORY_COUNT_MAX
};

struct Lists
{
	Item	own[ INVENTORY_SLOTS ];		// the local player's items
	Item	shared[ INVENTORY_SLOTS ];	// the team's shared pool
};

struct Msg
{
	int		count;			// 0 .. INVENTORY_COUNT_MAX
	char	name[ INVENTORY_NAME_LEN ];
	char	icon[ INVENTORY_NAME_LEN ];
	int		flags;			// INVENTORY_FLAG_*
};

// Reads one message. False: cut short or no item name; nothing usable. Names longer than the limit are cut; a
// count out of range is clamped to 0 .. INVENTORY_COUNT_MAX.
bool Parse( const unsigned char *data, int size, Msg &out );

// Applies a message to its list: count 0 removes the item, else it is set (a new item goes after the others; with
// INVENTORY_SLOTS items already it is left out). True: the list changed.
bool Apply( Lists &lists, const Msg &msg );

void Clear( Lists &lists );

// used slots of a list (they come first)
int Used( const Item *list );
} // namespace inventory

#endif // INVENTORY_CORE_H
