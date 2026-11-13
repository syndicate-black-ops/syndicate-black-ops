//=========================================================
// SBO 2.0 inventory, client side: the message and the item lists. See inventory_core.h.
//=========================================================

#include "inventory_core.h"

#include <string.h>

namespace
{
struct Reader
{
	const unsigned char	*data;
	int					size;
	int					pos;
	bool				bad;

	int Byte()
	{
		if ( pos + 1 > size )
		{
			bad = true;
			return 0;
		}
		return data[pos++];
	}

	int Short()		// signed
	{
		if ( pos + 2 > size )
		{
			bad = true;
			return 0;
		}
		const int v = data[pos] | ( data[pos + 1] << 8 );
		pos += 2;
		return v >= 32768 ? v - 65536 : v;
	}

	// a string cut to len - 1 bytes; the rest of it is skipped
	void String( char *out, int len )
	{
		int n = 0;
		for ( ;; )
		{
			if ( pos >= size )
			{
				bad = true;
				break;
			}
			const unsigned char c = data[pos++];
			if ( !c )
				break;
			if ( n < len - 1 )
				out[n++] = (char)c;
		}
		out[n] = '\0';
	}
};

int FindItem( const inventory::Item *list, const char *name )
{
	for ( int i = 0; i < INVENTORY_SLOTS; i++ )
		if ( list[i].name[0] && !strcmp( list[i].name, name ) )
			return i;
	return -1;
}
} // namespace

bool inventory::Parse( const unsigned char *data, int size, Msg &out )
{
	memset( &out, 0, sizeof( out ) );
	if ( !data || size < 4 )
		return false;

	Reader r = { data, size, 0, false };
	int count = r.Short();
	r.String( out.name, (int)sizeof( out.name ) );
	r.String( out.icon, (int)sizeof( out.icon ) );
	out.flags = r.Byte();
	if ( r.bad || !out.name[0] )
	{
		memset( &out, 0, sizeof( out ) );
		return false;
	}
	if ( count < 0 )
		count = 0;
	if ( count > INVENTORY_COUNT_MAX )
		count = INVENTORY_COUNT_MAX;
	out.count = count;
	return true;
}

int inventory::Used( const Item *list )
{
	int n = 0;
	while ( n < INVENTORY_SLOTS && list[n].name[0] )
		n++;
	return n;
}

bool inventory::Apply( Lists &lists, const Msg &msg )
{
	if ( !msg.name[0] )
		return false;
	Item *list = ( msg.flags & INVENTORY_FLAG_SHARED ) ? lists.shared : lists.own;
	const int i = FindItem( list, msg.name );

	if ( msg.count <= 0 )
	{
		if ( i < 0 )
			return false;
		// keep the order: the ones after it move up
		for ( int j = i; j < INVENTORY_SLOTS - 1; j++ )
			list[j] = list[j + 1];
		memset( &list[ INVENTORY_SLOTS - 1 ], 0, sizeof( Item ) );
		return true;
	}

	int count = msg.count > INVENTORY_COUNT_MAX ? INVENTORY_COUNT_MAX : msg.count;
	if ( i >= 0 )
	{
		const bool changed = list[i].count != count || strcmp( list[i].icon, msg.icon );
		list[i].count = count;
		strncpy( list[i].icon, msg.icon, INVENTORY_NAME_LEN - 1 );
		list[i].icon[ INVENTORY_NAME_LEN - 1 ] = '\0';
		return changed;
	}

	const int n = Used( list );
	if ( n >= INVENTORY_SLOTS )
		return false;	// full: the server never sends more than INVENTORY_SLOTS items per list
	Item &item = list[n];
	memset( &item, 0, sizeof( item ) );
	strncpy( item.name, msg.name, INVENTORY_NAME_LEN - 1 );
	strncpy( item.icon, msg.icon, INVENTORY_NAME_LEN - 1 );
	item.count = count;
	return true;
}

void inventory::Clear( Lists &lists )
{
	memset( &lists, 0, sizeof( lists ) );
}
