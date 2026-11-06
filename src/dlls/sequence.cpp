//=========================================================
// Sequence files: the scripts of trigger_sequence (CS: Condition Zero Deleted Scenes), docs/SEQUENCES.md.
//
// In Condition Zero the engine reads sequences/global.seq and sequences/<map>.seq and hands the game the parsed
// entries (g_engfuncs.pfnSequenceGet, pfnSequencePickSentence). Those two functions are not there on every engine
// SBO runs on (the dedicated server's table has NULL in both slots), so the game reads and parses the files
// itself. Nothing here calls the engine: the file access, the console and the random numbers are the host's
// (sequence.h), which is the game (trigger_sequence.cpp) or the parser test (tests/sequences).
//
// What this is based on:
//   * Valve's common/Sequence.h (Half-Life SDK): the structures this parser fills, the command and modifier
//     numbers, the modifier bits, and the four functions a sequence manager offers (SequenceGet,
//     Sequence_ParseFile, Sequence_OnLevelLoad, SequencePickSentence). Their names and fields are used as they are.
//   * czeror-sdk's dlls/CTriggerSequence.cpp (Half-Life SDK license): how the game uses those structures, which is
//     what a parsed entry has to look like.
//   * The TWHL tutorial "Scripting with sequence files": the syntax (blocks, comments, '#' '$' '@', sentences) and
//     what each command and modifier means.
//   * Condition Zero Deleted Scenes' own sequence files, which are read as test input (all of them load).
// The parser is written for SBO from those; its design, its function and variable names and its messages are its
// own. It does not contain code from any other implementation.
//
// How it works: the text of a file is read in one pass into entries, each a list of commands (Sequence_LoadText).
// Then the macros are put in place (Sequence_ExpandMacros), every line is given the modifiers in force where it
// stands (Sequence_ResolveModifiers), and the entries are checked (Sequence_CheckEntry). An error is reported with
// the file and the line, and nothing of that file is kept; the game goes on.
//=========================================================

#ifdef SEQUENCE_STANDALONE
// tests/sequences: no SDK headers
typedef int qboolean;
typedef unsigned char byte;
#define _DEF_BYTE_
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#include <strings.h>
#define stricmp strcasecmp
#else
#include	"extdll.h"
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdarg.h>
#include	<ctype.h>

#include	"sequence.h"

// The longest name / value (an entry, command, target or sound name, a number) and quoted string
#define SEQUENCE_NAME_MAX		256
#define SEQUENCE_STRING_MAX		1024

// A sentence group's name and one sentence line
#define SEQUENCE_GROUP_MAX		64
#define SEQUENCE_SENTENCE_MAX	1024

// Macros expanded into one entry before it counts as a macro that expands itself
#define SEQUENCE_MAX_EXPANSIONS	1024

// The words of the format, as Valve's Sequence.h lists them (sequenceCommandEnum_e), with their spelling in a file
static const sequenceCommandMapping_s s_keywords[] =
{
	{ SEQUENCE_COMMAND_PAUSE,			"pause",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_FIRETARGETS,		"firetargets",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_KILLTARGETS,		"killtargets",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_TEXT,			"text",			SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SOUND,			"sound",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_GOSUB,			"gosub",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SENTENCE,		"sentence",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_REPEAT,			"repeat",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SETDEFAULTS,		"setdefaults",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_MODIFIER,		"modifier",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_POSTMODIFIER,	"postmodifier",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_NOOP,			"noop",			SEQUENCE_TYPE_COMMAND },

	{ SEQUENCE_MODIFIER_EFFECT,			"effect",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_POSITION,		"position",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_COLOR,			"color",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_COLOR2,			"color2",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FADEIN,			"fadein",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FADEOUT,		"fadeout",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_HOLDTIME,		"holdtime",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FXTIME,			"fxtime",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_SPEAKER,		"speaker",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_LISTENER,		"listener",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_TEXTCHANNEL,	"channel",		SEQUENCE_TYPE_MODIFIER },
};

// Every text modifier and the three others: what an entry's defaults command sets
#define SEQUENCE_ALL_MODIFIER_BITS	( SEQUENCE_MODIFIER_EFFECT_BIT | SEQUENCE_MODIFIER_POSITION_BIT \
	| SEQUENCE_MODIFIER_COLOR_BIT | SEQUENCE_MODIFIER_COLOR2_BIT | SEQUENCE_MODIFIER_FADEIN_BIT \
	| SEQUENCE_MODIFIER_FADEOUT_BIT | SEQUENCE_MODIFIER_HOLDTIME_BIT | SEQUENCE_MODIFIER_FXTIME_BIT \
	| SEQUENCE_MODIFIER_SPEAKER_BIT | SEQUENCE_MODIFIER_LISTENER_BIT | SEQUENCE_MODIFIER_TEXTCHANNEL_BIT )

// The loaded entries (newest first: a map's entry hides global.seq's entry of the same name) and sentence groups
static sequenceEntry_s		*s_entries = NULL;
static sentenceGroupEntry_s	*s_sentenceGroups = NULL;
static unsigned int			s_sentenceCount = 0;	// the next sentence's index

// The file being parsed
static sequenceEntry_s		*s_pendingEntries = NULL;	// its entries, newest first; joined to the list when it is done
static sequenceCommandLine_s s_fileDefaults;	// "$" lines outside the entries
static char					s_fileName[64];	// without "sequences/" and ".seq"
static qboolean				s_fileIsGlobal;
static const char			*s_cursor;
static int					s_line;
static qboolean				s_failed;			// the first error stops the file


//=========================================================
// Messages
//=========================================================

static void Sequence_Printf( const char *format, ... )
{
	va_list	argptr;
	char	text[1024];

	va_start( argptr, format );
	vsnprintf( text, sizeof( text ), format, argptr );
	va_end( argptr );
	text[ sizeof( text ) - 1 ] = '\0';

	Sequence_HostPrint( text );
}

// An error in the file being parsed, at the line the scan is on. Only the first one is shown: what follows it
// are its consequences.
static void Sequence_ParseError( const char *format, ... )
{
	va_list	argptr;
	char	text[512];

	if ( s_failed )
		return;
	s_failed = TRUE;

	va_start( argptr, format );
	vsnprintf( text, sizeof( text ), format, argptr );
	va_end( argptr );
	text[ sizeof( text ) - 1 ] = '\0';

	Sequence_Printf( "sequences/%s.seq, line %d: %s\n", s_fileName, s_line, text );
}

// An error found after the parsing (macros, loops): no line
static void Sequence_FileError( const char *format, ... )
{
	va_list	argptr;
	char	text[512];

	if ( s_failed )
		return;
	s_failed = TRUE;

	va_start( argptr, format );
	vsnprintf( text, sizeof( text ), format, argptr );
	va_end( argptr );
	text[ sizeof( text ) - 1 ] = '\0';

	Sequence_Printf( "sequences/%s.seq: %s\n", s_fileName, text );
}

// How a character is shown in an error
static const char *Sequence_CharName( char ch )
{
	static char name[8];

	if ( !ch )
		return "the end of the file";

	if ( isprint( (unsigned char)ch ) )
		sprintf( name, "'%c'", ch );
	else
		sprintf( name, "0x%02x", (unsigned char)ch );

	return name;
}


//=========================================================
// Memory
//=========================================================

static char *Sequence_CopyString( const char *string )
{
	if ( !string )
		return NULL;

	char *copy = (char *)malloc( strlen( string ) + 1 );
	strcpy( copy, string );
	return copy;
}

static void Sequence_ReplaceString( char **dest, const char *string )
{
	char *copy = Sequence_CopyString( string );

	free( *dest );
	*dest = copy;
}

static sequenceCommandLine_s *Sequence_NewCommand( int commandType )
{
	sequenceCommandLine_s *command = (sequenceCommandLine_s *)calloc( 1, sizeof( sequenceCommandLine_s ) );

	command->commandType = commandType;
	return command;
}

// Frees a command and its strings
static void Sequence_DeleteCommand( sequenceCommandLine_s *command )
{
	free( command->fireTargetNames );
	free( command->killTargetNames );
	free( command->speakerName );
	free( command->listenerName );
	free( command->soundFileName );
	free( command->sentenceName );
	free( (char *)command->clientMessage.pName );
	free( (char *)command->clientMessage.pMessage );
	free( command );
}

// Frees an entry and its commands
static void Sequence_DeleteEntry( sequenceEntry_s *entry )
{
	sequenceCommandLine_s *command, *next;

	for ( command = entry->firstCommand; command; command = next )
	{
		next = command->nextCommandLine;
		Sequence_DeleteCommand( command );
	}

	free( entry->entryName );
	free( entry->fileName );
	free( entry );
}

// Takes the sentences of a kind out of the groups, and the groups left empty. minIndex: also every sentence from
// that index on (the sentences of a file that failed).
static void Sequence_RemoveSentences( qboolean includeGlobal, unsigned int minIndex )
{
	sentenceGroupEntry_s **groupLink = &s_sentenceGroups;

	while ( *groupLink )
	{
		sentenceGroupEntry_s	*group = *groupLink;
		sentenceEntry_s			**sentenceLink = &group->firstSentence;

		while ( *sentenceLink )
		{
			sentenceEntry_s *sentence = *sentenceLink;

			if ( includeGlobal || !sentence->isGlobal || sentence->index >= minIndex )
			{
				*sentenceLink = sentence->nextEntry;
				group->numSentences--;

				free( sentence->data );
				free( sentence );
			}
			else
				sentenceLink = &sentence->nextEntry;
		}

		if ( !group->firstSentence )
		{
			*groupLink = group->nextEntry;

			free( group->groupName );
			free( group );
		}
		else
			groupLink = &group->nextEntry;
	}
}

// Forgets the entries and sentences of the map's file, or all of them.
void Sequence_Clear( qboolean includeGlobal )
{
	sequenceEntry_s **link = &s_entries;

	while ( *link )
	{
		sequenceEntry_s *entry = *link;

		if ( includeGlobal || !entry->isGlobal )
		{
			*link = entry->nextEntry;
			Sequence_DeleteEntry( entry );
		}
		else
			link = &entry->nextEntry;
	}

	Sequence_RemoveSentences( includeGlobal, (unsigned int)-1 );

	if ( !s_sentenceGroups )
		s_sentenceCount = 0;
}


//=========================================================
// Scanning
//=========================================================

// Takes the comments out of a file's text: "//" to the end of the line and "/* */", wherever they are (inside a quoted string
// too). The line ends stay, for the line numbers.
static void Sequence_RemoveComments( char *buffer )
{
	char *read = buffer;
	char *write = buffer;

	while ( *read )
	{
		if ( read[0] == '/' && read[1] == '/' )
		{
			while ( *read && *read != '\n' )
				read++;
		}
		else if ( read[0] == '/' && read[1] == '*' )
		{
			read += 2;

			while ( *read && !( read[0] == '*' && read[1] == '/' ) )
			{
				if ( *read == '\n' )
					*write++ = '\n';
				read++;
			}

			if ( *read )
				read += 2;
		}
		else
			*write++ = *read++;
	}

	*write = '\0';
}

// What a name or an unquoted value is made of: letters, digits, '.', '-', '_', '/' and the backslash
static qboolean Sequence_IsWordChar( char ch )
{
	if ( isalnum( (unsigned char)ch ) )
		return TRUE;

	switch ( ch )
	{
	case '.':
	case '-':
	case '_':
	case '/':
	case '\\':
		return TRUE;
	}

	return FALSE;
}

// Moves on to the next character that is not blank. TRUE if a line ended on the way.
static qboolean Sequence_SkipBlanks( void )
{
	qboolean newLine = FALSE;

	for ( ; *s_cursor && isspace( (unsigned char)*s_cursor ); s_cursor++ )
	{
		if ( *s_cursor == '\n' )
		{
			s_line++;
			newLine = TRUE;
		}
	}

	return newLine;
}

// The next character that is not whitespace; 0 at the end of the file (and after an error,
// which ends every loop that reads symbols).
static char Sequence_NextSymbol( void )
{
	if ( s_failed )
		return '\0';

	Sequence_SkipBlanks();

	char ch = *s_cursor;
	if ( ch )
		s_cursor++;

	return ch;
}

// A name or an unquoted value. FALSE: there is none here, or it is too long (an error, reported).
static qboolean Sequence_ReadWord( char *token, int size )
{
	int length = 0;

	token[0] = '\0';

	if ( s_failed )
		return FALSE;

	Sequence_SkipBlanks();

	if ( !Sequence_IsWordChar( *s_cursor ) )
	{
		if ( *s_cursor == '#' || *s_cursor == '$' )
			Sequence_ParseError( "only one '%c' per line, at the beginning of the line (the commands after a ',' have none)", *s_cursor );
		else
			Sequence_ParseError( "expected a name or a value, found %s", Sequence_CharName( *s_cursor ) );
		return FALSE;
	}

	while ( Sequence_IsWordChar( *s_cursor ) )
	{
		if ( length >= size - 1 )
		{
			token[ length ] = '\0';
			Sequence_ParseError( "\"%s...\" is too long (%d characters at most)", token, size - 1 );
			return FALSE;
		}

		token[ length++ ] = *s_cursor++;
	}

	token[ length ] = '\0';
	return TRUE;
}

// A number. A value that is not one is an error.
static float Sequence_ReadNumber( void )
{
	char	token[SEQUENCE_NAME_MAX];
	char	*end;

	if ( !Sequence_ReadWord( token, sizeof( token ) ) )
		return 0;

	float value = (float)strtod( token, &end );
	if ( end == token || *end )
	{
		Sequence_ParseError( "\"%s\" is not a number", token );
		return 0;
	}

	return value;
}

static int Sequence_ReadWholeNumber( void )
{
	char	token[SEQUENCE_NAME_MAX];
	char	*end;

	if ( !Sequence_ReadWord( token, sizeof( token ) ) )
		return 0;

	// Condition Zero's own files have "channel = 2.786" and "color = .1 .1 .1": the whole part counts
	double value = strtod( token, &end );
	if ( end == token || *end )
	{
		Sequence_ParseError( "\"%s\" is not a number", token );
		return 0;
	}

	return (int)value;
}

// One of a color's three values
static byte Sequence_ReadColorByte( void )
{
	int value = Sequence_ReadWholeNumber();

	if ( !s_failed && ( value < 0 || value > 255 ) )
	{
		Sequence_ParseError( "color value %d: a color is three numbers from 0 to 255", value );
		return 0;
	}

	return (byte)value;
}

// An unquoted name, kept as a string of its own.
static void Sequence_ReadName( char **dest )
{
	char token[SEQUENCE_NAME_MAX];

	if ( Sequence_ReadWord( token, sizeof( token ) ) )
		Sequence_ReplaceString( dest, token );
}

// A quoted string: everything between two quotes, line ends too (there are no escapes).
static void Sequence_ReadQuoted( char **dest )
{
	char	string[SEQUENCE_STRING_MAX];
	int		length = 0;
	int		startLine;

	char ch = Sequence_NextSymbol();
	if ( s_failed )
		return;

	if ( ch != '\"' )
	{
		Sequence_ParseError( "expected a quoted string (\"...\"), found %s", Sequence_CharName( ch ) );
		return;
	}

	startLine = s_line;

	for ( ; *s_cursor && *s_cursor != '\"'; s_cursor++ )
	{
		if ( *s_cursor == '\n' )
			s_line++;

		// the line ends of a file saved on Windows
		if ( *s_cursor == '\r' )
			continue;

		if ( length >= (int)sizeof( string ) - 1 )
		{
			s_line = startLine;
			Sequence_ParseError( "quoted string too long (%d characters at most)", (int)sizeof( string ) - 1 );
			return;
		}

		string[ length++ ] = *s_cursor;
	}

	if ( !*s_cursor )
	{
		s_line = startLine;
		Sequence_ParseError( "quoted string without its closing quote" );
		return;
	}

	s_cursor++;
	string[ length ] = '\0';

	Sequence_ReplaceString( dest, string );
}


//=========================================================
// Commands and modifiers
//=========================================================

// The command or modifier of a name. The names are not case sensitive.
static const sequenceCommandMapping_s *Sequence_FindKeyword( const char *commandName )
{
	for ( int i = 0; i < (int)( sizeof( s_keywords ) / sizeof( s_keywords[0] ) ); i++ )
	{
		if ( !stricmp( s_keywords[i].commandName, commandName ) )
			return &s_keywords[i];
	}

	return NULL;
}

// What a file's defaults are before its first "$" line.
static void Sequence_InitFileDefaults( sequenceCommandLine_s *defaults )
{
	free( defaults->speakerName );
	free( defaults->listenerName );
	memset( defaults, 0, sizeof( *defaults ) );

	defaults->clientMessage.effect = 0;
	defaults->clientMessage.r1 = defaults->clientMessage.g1 = defaults->clientMessage.b1 = defaults->clientMessage.a1 = 255;
	defaults->clientMessage.r2 = defaults->clientMessage.g2 = defaults->clientMessage.b2 = defaults->clientMessage.a2 = 255;
	defaults->clientMessage.x = 0.5;
	defaults->clientMessage.y = 0.5;
	defaults->clientMessage.fadein = 0.2;
	defaults->clientMessage.fadeout = 0.2;
	defaults->clientMessage.holdtime = 1.6;
	defaults->clientMessage.fxtime = 1.0;
}

// Copies the modifiers named by a bit field (SEQUENCE_MODIFIER_*_BIT) from one command to another. The texts of
// the destination (its message, the macro it names) are its own and stay.
static void Sequence_ApplyModifiers( sequenceCommandLine_s *destination, const sequenceCommandLine_s *source, int bits )
{
	client_textmessage_t *to = &destination->clientMessage;
	const client_textmessage_t *from = &source->clientMessage;

	if ( bits & SEQUENCE_MODIFIER_EFFECT_BIT )
		to->effect = from->effect;

	if ( bits & SEQUENCE_MODIFIER_POSITION_BIT )
	{
		to->x = from->x;
		to->y = from->y;
	}

	if ( bits & SEQUENCE_MODIFIER_COLOR_BIT )
	{
		to->r1 = from->r1;
		to->g1 = from->g1;
		to->b1 = from->b1;
		to->a1 = from->a1;
	}

	if ( bits & SEQUENCE_MODIFIER_COLOR2_BIT )
	{
		to->r2 = from->r2;
		to->g2 = from->g2;
		to->b2 = from->b2;
		to->a2 = from->a2;
	}

	if ( bits & SEQUENCE_MODIFIER_FADEIN_BIT )
		to->fadein = from->fadein;

	if ( bits & SEQUENCE_MODIFIER_FADEOUT_BIT )
		to->fadeout = from->fadeout;

	if ( bits & SEQUENCE_MODIFIER_HOLDTIME_BIT )
		to->holdtime = from->holdtime;

	if ( bits & SEQUENCE_MODIFIER_FXTIME_BIT )
		to->fxtime = from->fxtime;

	if ( bits & SEQUENCE_MODIFIER_TEXTCHANNEL_BIT )
		destination->textChannel = source->textChannel;

	if ( bits & SEQUENCE_MODIFIER_SPEAKER_BIT )
		Sequence_ReplaceString( &destination->speakerName, source->speakerName );

	if ( bits & SEQUENCE_MODIFIER_LISTENER_BIT )
		Sequence_ReplaceString( &destination->listenerName, source->listenerName );
}

// The value(s) after "name =", into a command. A modifier also sets its bit in the command's bit field: which
// of its modifier fields mean something.
static void Sequence_ReadValue( const sequenceCommandMapping_s *keyword, sequenceCommandLine_s *command )
{
	// a modifier's bit: SEQUENCE_MODIFIER_EFFECT_BIT is 1 << 1, and so on in the enum's order
	if ( keyword->commandType == SEQUENCE_TYPE_MODIFIER )
		command->modifierBitField |= 1 << ( keyword->commandEnum - SEQUENCE_COMMAND_NOOP );

	switch ( keyword->commandEnum )
	{
	case SEQUENCE_COMMAND_PAUSE:
		command->delay = Sequence_ReadNumber();
		if ( !s_failed && command->delay < 0 )
			Sequence_ParseError( "pause = %g: a pause cannot be negative", command->delay );
		break;

	case SEQUENCE_COMMAND_FIRETARGETS:
		Sequence_ReadQuoted( &command->fireTargetNames );
		break;

	case SEQUENCE_COMMAND_KILLTARGETS:
		Sequence_ReadQuoted( &command->killTargetNames );
		break;

	case SEQUENCE_COMMAND_TEXT:
		Sequence_ReadQuoted( (char **)&command->clientMessage.pMessage );
		break;

	case SEQUENCE_COMMAND_SOUND:
		Sequence_ReadName( &command->soundFileName );
		break;

	// the entry to expand here: what "@name" is
	case SEQUENCE_COMMAND_GOSUB:
		Sequence_ReadName( (char **)&command->clientMessage.pName );
		break;

	case SEQUENCE_COMMAND_SENTENCE:
		Sequence_ReadName( &command->sentenceName );
		break;

	case SEQUENCE_COMMAND_REPEAT:
		command->repeatCount = Sequence_ReadWholeNumber();
		break;

	case SEQUENCE_MODIFIER_EFFECT:
		command->clientMessage.effect = Sequence_ReadWholeNumber();
		break;

	case SEQUENCE_MODIFIER_POSITION:
		command->clientMessage.x = Sequence_ReadNumber();
		command->clientMessage.y = Sequence_ReadNumber();
		break;

	case SEQUENCE_MODIFIER_COLOR:
		command->clientMessage.r1 = Sequence_ReadColorByte();
		command->clientMessage.g1 = Sequence_ReadColorByte();
		command->clientMessage.b1 = Sequence_ReadColorByte();
		command->clientMessage.a1 = 255;
		break;

	case SEQUENCE_MODIFIER_COLOR2:
		command->clientMessage.r2 = Sequence_ReadColorByte();
		command->clientMessage.g2 = Sequence_ReadColorByte();
		command->clientMessage.b2 = Sequence_ReadColorByte();
		command->clientMessage.a2 = 255;
		break;

	case SEQUENCE_MODIFIER_FADEIN:
		command->clientMessage.fadein = Sequence_ReadNumber();
		break;

	case SEQUENCE_MODIFIER_FADEOUT:
		command->clientMessage.fadeout = Sequence_ReadNumber();
		break;

	case SEQUENCE_MODIFIER_HOLDTIME:
		command->clientMessage.holdtime = Sequence_ReadNumber();
		break;

	case SEQUENCE_MODIFIER_FXTIME:
		command->clientMessage.fxtime = Sequence_ReadNumber();
		break;

	case SEQUENCE_MODIFIER_SPEAKER:
		Sequence_ReadName( &command->speakerName );
		break;

	case SEQUENCE_MODIFIER_LISTENER:
		Sequence_ReadName( &command->listenerName );
		break;

	case SEQUENCE_MODIFIER_TEXTCHANNEL:
		command->textChannel = Sequence_ReadWholeNumber();
		break;

	// setdefaults, modifier, postmodifier, noop: what the parser makes of the lines, not commands of a file
	default:
		Sequence_ParseError( "\"%s\" cannot be used in a file (it is what the parser makes of other lines)", keyword->commandName );
		break;
	}
}

// A new command at the end of an entry
static sequenceCommandLine_s *Sequence_AppendCommand( sequenceEntry_s *entry, int commandType )
{
	sequenceCommandLine_s *command = Sequence_NewCommand( commandType );
	sequenceCommandLine_s **link = &entry->firstCommand;

	while ( *link )
		link = &(*link)->nextCommandLine;

	*link = command;
	return command;
}

// The "name =" in front of a value: which command or modifier it is. On a "$" line only a modifier will do.
// NULL: an error, reported.
static const sequenceCommandMapping_s *Sequence_ReadKeyword( qboolean modifierLine )
{
	char name[SEQUENCE_NAME_MAX];

	if ( !Sequence_ReadWord( name, sizeof( name ) ) )
		return NULL;

	const sequenceCommandMapping_s *keyword = Sequence_FindKeyword( name );
	if ( !keyword )
	{
		Sequence_ParseError( modifierLine ? "unknown modifier \"%s\"" : "unknown command \"%s\"", name );
		return NULL;
	}

	if ( modifierLine && keyword->commandType != SEQUENCE_TYPE_MODIFIER )
	{
		Sequence_ParseError( "\"%s\" is a command: it goes on a '#' line, not on a '$' line", name );
		return NULL;
	}

	char symbol = Sequence_NextSymbol();
	if ( s_failed )
		return NULL;

	if ( symbol != '=' )
	{
		Sequence_ParseError( modifierLine ? "after the modifier \"%s\": expected '=', found %s"
			: "after the command \"%s\": expected '=', found %s", name, Sequence_CharName( symbol ) );
		return NULL;
	}

	return keyword;
}

// A "$" line: "name = value(s)", and more of them after a ','. Outside the entries (no entry given) they change
// the file's defaults. Inside an entry each becomes a modifier command of its own, which holds from there to the
// end of the entry. After a value only a ',' or the end of the line may follow. Returns the first symbol of what
// comes next; 0 after an error.
static char Sequence_ParseModifierList( sequenceEntry_s *entry )
{
	char symbol = ',';

	while ( symbol == ',' )
	{
		sequenceCommandLine_s *target = entry ? Sequence_AppendCommand( entry, SEQUENCE_COMMAND_MODIFIER ) : &s_fileDefaults;

		const sequenceCommandMapping_s *keyword = Sequence_ReadKeyword( TRUE );
		if ( !keyword )
			return '\0';

		Sequence_ReadValue( keyword, target );
		if ( s_failed )
			return '\0';

		if ( !Sequence_SkipBlanks() && *s_cursor && *s_cursor != ',' )
		{
			Sequence_ParseError( "after the value of the modifier \"%s\": expected ',' or the end of the line, found %s",
				keyword->commandName, Sequence_CharName( *s_cursor ) );
			return '\0';
		}

		symbol = Sequence_NextSymbol();
	}

	return symbol;
}

// A "#" line or an "@name" line: one command of the entry, and everything the line gives it.
//   "#name = value(s)", and more of them after a ',': what is a command (pause, text, firetargets, ...) is set on
//   the line's command. What is a modifier (color, speaker, ...) becomes a command of its own behind it, marked as
//   changing that one line only.
//   "@name": the line's command names the entry to put in its place (Sequence_ExpandMacros); after a ',' it takes
//   more, as a "#" line does.
// The line ends where something that is no ',' follows a value. Returns that symbol; 0 after an error.
static char Sequence_ParseActionLine( sequenceEntry_s *entry, qboolean isMacro )
{
	// Valve's Sequence.h numbers "pause" 0; it is the type of every line of this kind, with or without a delay
	sequenceCommandLine_s *line = Sequence_AppendCommand( entry, SEQUENCE_COMMAND_PAUSE );
	char symbol = ',';

	if ( isMacro )
	{
		Sequence_ReadName( (char **)&line->clientMessage.pName );
		symbol = Sequence_NextSymbol();
	}

	while ( symbol == ',' )
	{
		const sequenceCommandMapping_s *keyword = Sequence_ReadKeyword( FALSE );
		if ( !keyword )
			return '\0';

		sequenceCommandLine_s *target = line;
		if ( keyword->commandType == SEQUENCE_TYPE_MODIFIER )
			target = Sequence_AppendCommand( entry, SEQUENCE_COMMAND_POSTMODIFIER );

		Sequence_ReadValue( keyword, target );

		symbol = Sequence_NextSymbol();
	}

	return symbol;
}

// An entry's first command: the file's defaults as they are where the entry begins, every one of them. It is what
// the entry's lines start from (Sequence_ResolveModifiers).
static void Sequence_AddEntryDefaults( sequenceEntry_s *entry )
{
	sequenceCommandLine_s *command = Sequence_AppendCommand( entry, SEQUENCE_COMMAND_SETDEFAULTS );

	Sequence_ApplyModifiers( command, &s_fileDefaults, SEQUENCE_ALL_MODIFIER_BITS );
	command->modifierBitField = SEQUENCE_ALL_MODIFIER_BITS;
}

// The entry of a name: the file being parsed first, then what is loaded (the map's before global.seq's)
static sequenceEntry_s *Sequence_FindEntry( const char *entryName )
{
	sequenceEntry_s *scan;

	for ( scan = s_pendingEntries; scan; scan = scan->nextEntry )
	{
		if ( !stricmp( entryName, scan->entryName ) )
			return scan;
	}

	for ( scan = s_entries; scan; scan = scan->nextEntry )
	{
		if ( !stricmp( entryName, scan->entryName ) )
			return scan;
	}

	return NULL;
}

// "%name { lines }": an entry. Returns the first symbol after its '}'; 0 after an error.
static char Sequence_ParseEntryBlock( void )
{
	char name[SEQUENCE_NAME_MAX];

	if ( !Sequence_ReadWord( name, sizeof( name ) ) )
		return '\0';

	for ( sequenceEntry_s *scan = s_pendingEntries; scan; scan = scan->nextEntry )
	{
		if ( !stricmp( name, scan->entryName ) )
		{
			Sequence_ParseError( "the entry \"%s\" is in the file twice", name );
			return '\0';
		}
	}

	char symbol = Sequence_NextSymbol();
	if ( s_failed )
		return '\0';

	if ( symbol != '{' )
	{
		Sequence_ParseError( "after the entry name \"%s\": expected '{', found %s", name, Sequence_CharName( symbol ) );
		return '\0';
	}

	sequenceEntry_s *entry = (sequenceEntry_s *)calloc( 1, sizeof( sequenceEntry_s ) );

	entry->entryName = Sequence_CopyString( name );
	entry->fileName = Sequence_CopyString( s_fileName );
	entry->isGlobal = s_fileIsGlobal;

	entry->nextEntry = s_pendingEntries;
	s_pendingEntries = entry;

	Sequence_AddEntryDefaults( entry );

	symbol = Sequence_NextSymbol();

	while ( symbol != '}' && !s_failed )
	{
		switch ( symbol )
		{
		case '#':
			symbol = Sequence_ParseActionLine( entry, FALSE );
			break;

		case '@':
			symbol = Sequence_ParseActionLine( entry, TRUE );
			break;

		case '$':
			symbol = Sequence_ParseModifierList( entry );
			break;

		case '\0':
			Sequence_ParseError( "the entry \"%s\" has no closing '}'", entry->entryName );
			break;

		default:
			Sequence_ParseError( "in an entry a line begins with '#' (command), '$' (modifier) or '@' (macro); found %s",
				Sequence_CharName( symbol ) );
			break;
		}
	}

	return Sequence_NextSymbol();
}


//=========================================================
// Sentences
//=========================================================

// The sentence group of a name (not case sensitive)
sentenceGroupEntry_s *Sequence_GetSentenceGroup( const char *groupName )
{
	if ( !groupName )
		return NULL;

	for ( sentenceGroupEntry_s *group = s_sentenceGroups; group; group = group->nextEntry )
	{
		if ( !stricmp( group->groupName, groupName ) )
			return group;
	}

	return NULL;
}

// A sentence more in a group, which is made if this is its first
static void Sequence_AddSentence( const char *groupName, const char *data )
{
	sentenceGroupEntry_s *group = Sequence_GetSentenceGroup( groupName );

	if ( !group )
	{
		sentenceGroupEntry_s **link = &s_sentenceGroups;

		while ( *link )
			link = &(*link)->nextEntry;

		group = (sentenceGroupEntry_s *)calloc( 1, sizeof( sentenceGroupEntry_s ) );
		group->groupName = Sequence_CopyString( groupName );
		*link = group;
	}

	sentenceEntry_s *sentence = (sentenceEntry_s *)calloc( 1, sizeof( sentenceEntry_s ) );

	sentence->data = Sequence_CopyString( data );
	sentence->isGlobal = s_fileIsGlobal;
	sentence->index = s_sentenceCount++;

	sentenceEntry_s **link = &group->firstSentence;

	while ( *link )
		link = &(*link)->nextEntry;

	*link = sentence;
	group->numSentences++;
}

// One line of a sentence block, "name sentence": the name, then the rest of the line. The group is the name
// without what follows its last letter or '_' (the number: HELLO0, HELLO1 are the group HELLO), as in sentences.txt.
// TRUE at the block's '}'.
static qboolean Sequence_ParseSentence( void )
{
	char	groupName[SEQUENCE_GROUP_MAX];
	char	data[SEQUENCE_SENTENCE_MAX];
	int		length;

	Sequence_SkipBlanks();

	if ( *s_cursor == '}' )
	{
		s_cursor++;
		return TRUE;
	}

	if ( !*s_cursor )
	{
		Sequence_ParseError( "the sentence block has no closing '}'" );
		return TRUE;
	}

	if ( !Sequence_IsWordChar( *s_cursor ) )
	{
		Sequence_ParseError( "expected a sentence name or '}', found %s", Sequence_CharName( *s_cursor ) );
		return TRUE;
	}

	if ( !Sequence_ReadWord( groupName, sizeof( groupName ) ) )
		return TRUE;

	length = strlen( groupName );
	while ( length > 0 && !isalpha( (unsigned char)groupName[ length - 1 ] ) && groupName[ length - 1 ] != '_' )
		length--;

	if ( !length )
	{
		Sequence_ParseError( "the sentence name \"%s\" has no letter", groupName );
		return TRUE;
	}

	groupName[ length ] = '\0';

	// the sentence: to the end of the line, without the blanks around it
	while ( *s_cursor == ' ' || *s_cursor == '\t' )
		s_cursor++;

	length = 0;
	while ( *s_cursor && *s_cursor != '\n' )
	{
		if ( length >= (int)sizeof( data ) - 1 )
		{
			Sequence_ParseError( "sentence too long (%d characters at most)", (int)sizeof( data ) - 1 );
			return TRUE;
		}

		data[ length++ ] = *s_cursor++;
	}

	while ( length > 0 && isspace( (unsigned char)data[ length - 1 ] ) )
		length--;
	data[ length ] = '\0';

	if ( !length )
	{
		Sequence_ParseError( "the sentence \"%s\" is empty (name and sentence go on one line)", groupName );
		return TRUE;
	}

	Sequence_AddSentence( groupName, data );
	return FALSE;
}

// "!sentences { lines }"
static char Sequence_ParseSentencesBlock( void )
{
	char token[SEQUENCE_NAME_MAX];

	if ( !Sequence_ReadWord( token, sizeof( token ) ) )
		return '\0';

	if ( stricmp( token, "sentences" ) )
	{
		Sequence_ParseError( "unknown block \"!%s\" (there is only \"!sentences\")", token );
		return '\0';
	}

	char symbol = Sequence_NextSymbol();
	if ( s_failed )
		return '\0';

	if ( symbol != '{' )
	{
		Sequence_ParseError( "after \"!sentences\": expected '{', found %s", Sequence_CharName( symbol ) );
		return '\0';
	}

	while ( !s_failed && !Sequence_ParseSentence() )
		;

	return Sequence_NextSymbol();
}

// The sound file of a sentence: its first word, "directory/name" (the engine's sentence rule: "vox/" without a
// directory, ".wav" added). wordCount: how many words the sentence has. FALSE: no word.
qboolean Sequence_SentenceSound( const char *data, char *sound, int soundSize, int *wordCount )
{
	char		word[SEQUENCE_SOUND_NAME_MAX];
	int			words = 0;
	qboolean	tooLong = FALSE;

	word[0] = '\0';

	while ( data && *data )
	{
		while ( *data && ( isspace( (unsigned char)*data ) || *data == ',' ) )
			data++;

		// a sentence's "(p120 t50)" settings
		if ( *data == '(' )
		{
			while ( *data && *data != ')' )
				data++;
			if ( *data )
				data++;
			continue;
		}

		if ( !*data )
			break;

		int length = 0;
		while ( *data && !isspace( (unsigned char)*data ) && *data != ',' && *data != '(' )
		{
			if ( !words )
			{
				if ( length >= (int)sizeof( word ) - 1 )
					tooLong = TRUE;
				else
					word[ length++ ] = *data;
			}
			data++;
		}

		if ( !words )
			word[ length ] = '\0';
		words++;
	}

	if ( wordCount )
		*wordCount = words;

	if ( !words || !word[0] || tooLong )
		return FALSE;

	int length = strlen( word );
	qboolean hasDirectory = ( strchr( word, '/' ) != NULL );
	qboolean hasExtension = ( length > 4 && !stricmp( word + length - 4, ".wav" ) );

	// a sentence's period ("stop."); '!' and '?' belong to the file's name ("hgrunt/grenade!")
	while ( !hasExtension && length > 1 && word[ length - 1 ] == '.' )
		word[ --length ] = '\0';

	if ( snprintf( sound, soundSize, "%s%s%s", hasDirectory ? "" : "vox/", word, hasExtension ? "" : ".wav" ) >= soundSize )
		return FALSE;

	return TRUE;
}

// A sentence that is one sound file and nothing else: one word, no "(p120)" settings. The game plays it as a
// sound; every other sentence needs the engine's sentence player (docs/SEQUENCES.md, "Sounds").
qboolean Sequence_IsSimpleSentence( const char *data )
{
	char	sound[SEQUENCE_SOUND_NAME_MAX];
	int		words = 0;

	return Sequence_SentenceSound( data, sound, sizeof( sound ), &words ) && words == 1 && !strchr( data, '(' );
}

// Whether a sentence can be sent to the clients to be played by their own sentence player (the message SeqSpeak,
// game_shared/sequence_speak.h). There it becomes part of a console command, so nothing but what a sentence is
// made of may be in it: Sequence_IsSpeakChar, SEQUENCE_SPEAK_MAX, the rule the client checks again. Only the
// first word may name a directory, as in sentences.txt. why: what is wrong, for the console.
qboolean Sequence_IsSpeakableSentence( const char *data, const char **why )
{
	static const char *dummy;
	char		sound[SEQUENCE_SOUND_NAME_MAX];
	qboolean	firstWord = TRUE, inWord = FALSE;
	int			length = 0, words = 0;

	if ( !why )
		why = &dummy;

	for ( const char *scan = data; scan && *scan; scan++, length++ )
	{
		char ch = *scan;

		if ( ch == ' ' || ch == '\t' )
		{
			if ( inWord )
				firstWord = FALSE;
			inWord = FALSE;
			continue;
		}

		if ( !Sequence_IsSpeakChar( ch ) )
		{
			*why = "it has a character a sentence cannot have";
			return FALSE;
		}

		if ( ch == '/' && !firstWord )
		{
			*why = "only its first word may name a directory";
			return FALSE;
		}

		inWord = TRUE;
	}

	Sequence_SentenceSound( data, sound, sizeof( sound ), &words );

	if ( !words )
	{
		*why = "it has no word";
		return FALSE;
	}

	if ( length > SEQUENCE_SPEAK_MAX )
	{
		*why = "it is too long";
		return FALSE;
	}

	*why = "";
	return Sequence_IsSpeakSafe( data );
}

// Sequence_SentenceFile: the file of one word, if it is the one asked for
static qboolean Sequence_IsWantedFile( const char *directory, const char *name, int *found, int index, char *sound, int soundSize )
{
	if ( (*found)++ != index )
		return FALSE;

	if ( snprintf( sound, soundSize, "%s/%s.wav", directory, name ) >= soundSize )
		sound[0] = '\0';

	return TRUE;
}

// The sound files a sentence is made of, as the engine's sentence player names them: "directory/word.wav" for
// every word, the directory being the first word's ("vox" without one), and "directory/_comma.wav" /
// "directory/_period.wav" for a ',' / '.'. index: 0, 1, ... until FALSE. For the download table.
qboolean Sequence_SentenceFile( const char *data, int index, char *sound, int soundSize )
{
	char	directory[SEQUENCE_SOUND_NAME_MAX] = "vox";
	char	word[SEQUENCE_SOUND_NAME_MAX];
	int		found = 0;

	if ( !data || index < 0 )
		return FALSE;

	while ( *data == ' ' || *data == '\t' )
		data++;

	// the directory: up to the last '/' of the first word
	const char *slash = NULL;

	for ( const char *end = data; *end && *end != ' ' && *end != '\t' && *end != '(' && *end != ','; end++ )
	{
		if ( *end == '/' )
			slash = end;
	}

	if ( slash )
	{
		if ( slash - data >= (int)sizeof( directory ) )
			return FALSE;

		memcpy( directory, data, slash - data );
		directory[ slash - data ] = '\0';
		data = slash + 1;
	}

	sound[0] = '\0';

	while ( *data )
	{
		if ( *data == ' ' || *data == '\t' )
		{
			data++;
			continue;
		}

		// a word's "(p120 t50)" settings
		if ( *data == '(' )
		{
			while ( *data && *data != ')' )
				data++;
			if ( *data )
				data++;
			continue;
		}

		if ( *data == ',' )
		{
			data++;

			if ( Sequence_IsWantedFile( directory, "_comma", &found, index, sound, soundSize ) )
				return sound[0] != '\0';
			continue;
		}

		int			length = 0;
		qboolean	period = FALSE;

		while ( *data && *data != ' ' && *data != '\t' && *data != '(' && *data != ',' )
		{
			if ( length < (int)sizeof( word ) - 1 )
				word[ length++ ] = *data;
			data++;
		}
		word[ length ] = '\0';

		// "name.wav" is the file itself; "name." is the word and a period
		if ( length > 4 && !stricmp( word + length - 4, ".wav" ) )
			word[ length - 4 ] = '\0';
		else
		{
			while ( length > 0 && word[ length - 1 ] == '.' )
			{
				word[ --length ] = '\0';
				period = TRUE;
			}
		}

		if ( word[0] && Sequence_IsWantedFile( directory, word, &found, index, sound, soundSize ) )
			return sound[0] != '\0';

		if ( period && Sequence_IsWantedFile( directory, "_period", &found, index, sound, soundSize ) )
			return sound[0] != '\0';
	}

	return FALSE;
}


//=========================================================
// After the parsing: macros, modifiers, loops
//=========================================================

// A copy of a command with strings of its own
static sequenceCommandLine_s *Sequence_DuplicateCommand( const sequenceCommandLine_s *original )
{
	sequenceCommandLine_s *copy = Sequence_NewCommand( original->commandType );

	copy->clientMessage = original->clientMessage;
	copy->clientMessage.pName = Sequence_CopyString( original->clientMessage.pName );
	copy->clientMessage.pMessage = Sequence_CopyString( original->clientMessage.pMessage );
	copy->speakerName = Sequence_CopyString( original->speakerName );
	copy->listenerName = Sequence_CopyString( original->listenerName );
	copy->soundFileName = Sequence_CopyString( original->soundFileName );
	copy->sentenceName = Sequence_CopyString( original->sentenceName );
	copy->fireTargetNames = Sequence_CopyString( original->fireTargetNames );
	copy->killTargetNames = Sequence_CopyString( original->killTargetNames );
	copy->delay = original->delay;
	copy->repeatCount = original->repeatCount;
	copy->textChannel = original->textChannel;
	copy->modifierBitField = original->modifierBitField;

	return copy;
}

// Puts the macros of an entry in place. Behind every command that names an entry ("@name", "gosub = name") goes a copy
// of that entry's commands, without its defaults command: a macro brings its modifiers and commands, and they
// work as if they were written there. The copy is scanned in turn (macros in macros).
static void Sequence_ExpandMacros( sequenceEntry_s *entry )
{
	int expansions = 0;

	for ( sequenceCommandLine_s *command = entry->firstCommand; command && !s_failed; command = command->nextCommandLine )
	{
		if ( !command->clientMessage.pName )
			continue;

		if ( !stricmp( command->clientMessage.pName, entry->entryName ) )
		{
			Sequence_FileError( "the entry \"%s\" uses itself as a macro", entry->entryName );
			return;
		}

		sequenceEntry_s *macro = Sequence_FindEntry( command->clientMessage.pName );
		if ( !macro )
		{
			Sequence_FileError( "the entry \"%s\" uses the macro \"%s\", which is in neither this file nor global.seq",
				entry->entryName, command->clientMessage.pName );
			return;
		}

		if ( ++expansions > SEQUENCE_MAX_EXPANSIONS )
		{
			Sequence_FileError( "the entry \"%s\" uses a macro that uses itself (\"%s\")", entry->entryName,
				command->clientMessage.pName );
			return;
		}

		sequenceCommandLine_s *rest = command->nextCommandLine;
		sequenceCommandLine_s **link = &command->nextCommandLine;

		for ( sequenceCommandLine_s *scan = macro->firstCommand; scan; scan = scan->nextCommandLine )
		{
			// the defaults command (a no-op once its entry is flattened)
			if ( scan->commandType == SEQUENCE_COMMAND_SETDEFAULTS || scan->commandType == SEQUENCE_COMMAND_NOOP )
				continue;

			*link = Sequence_DuplicateCommand( scan );
			link = &(*link)->nextCommandLine;
		}

		*link = rest;

		free( (char *)command->clientMessage.pName );
		command->clientMessage.pName = NULL;
	}
}

// Gives every line of an entry the modifiers that are in force where it stands, once the macros are in place.
// Walks the entry with a set of current modifiers: the entry's defaults command and every "$" line change the
// set; a "#" / "@" line receives the whole set; a modifier written on a line changes the line before it only.
static void Sequence_ResolveModifiers( sequenceEntry_s *entry )
{
	sequenceCommandLine_s	current;
	sequenceCommandLine_s	*lastLine = NULL;

	memset( &current, 0, sizeof( current ) );

	for ( sequenceCommandLine_s *command = entry->firstCommand; command; command = command->nextCommandLine )
	{
		switch ( command->commandType )
		{
		// spent once it is applied: a macro's copy of the entry leaves it out
		case SEQUENCE_COMMAND_SETDEFAULTS:
			Sequence_ApplyModifiers( &current, command, command->modifierBitField );
			command->commandType = SEQUENCE_COMMAND_NOOP;
			break;

		case SEQUENCE_COMMAND_MODIFIER:
			Sequence_ApplyModifiers( &current, command, command->modifierBitField );
			break;

		case SEQUENCE_COMMAND_POSTMODIFIER:
			if ( lastLine )
				Sequence_ApplyModifiers( lastLine, command, command->modifierBitField );
			break;

		case SEQUENCE_COMMAND_NOOP:
			break;

		default:
			Sequence_ApplyModifiers( command, &current, SEQUENCE_ALL_MODIFIER_BITS );
			lastLine = command;
			break;
		}
	}

	free( current.speakerName );
	free( current.listenerName );
}

// An endless "repeat" needs a pause before it, or the entry never lets go of the frame. Checked with the macros
// in place, so a pause that comes from a macro counts.
static void Sequence_CheckEntry( sequenceEntry_s *entry )
{
	float	duration = 0;
	int		repeats = 0;

	for ( sequenceCommandLine_s *command = entry->firstCommand; command; command = command->nextCommandLine )
	{
		duration += command->delay;

		if ( command->repeatCount < 0 && duration <= 0 )
		{
			Sequence_FileError( "the entry \"%s\" repeats forever without a pause", entry->entryName );
			return;
		}

		if ( command->repeatCount > 0 && ++repeats > SEQUENCE_MAX_REPEATS )
		{
			Sequence_FileError( "the entry \"%s\" has more than %d \"repeat\" commands with a count", entry->entryName,
				SEQUENCE_MAX_REPEATS );
			return;
		}
	}
}

// Which of its entry's counted "repeat" commands a command is (0 = the first); -1 if it is none
int Sequence_GetRepeatIndex( sequenceEntry_s *entry, sequenceCommandLine_s *command )
{
	int index = 0;

	for ( sequenceCommandLine_s *scan = entry->firstCommand; scan; scan = scan->nextCommandLine )
	{
		if ( scan->repeatCount <= 0 )
			continue;

		if ( scan == command )
			return index < SEQUENCE_MAX_REPEATS ? index : -1;

		index++;
	}

	return -1;
}


// The command that runs after a command. czeror-sdk: CTriggerSequence::AdvanceAndCheckForRepeat, where every
// "repeat" that is not 0 starts the entry over, forever. Here as Valve's Sequence.h describes the count ("N times,
// -1 = infinite"): -1 starts the entry over every time; N starts it over N times and then lets it go on.
// repeatsDone: SEQUENCE_MAX_REPEATS counters of the run, all 0 when it starts; each counted "repeat" of the entry
// has its own and sets it back to 0 when it lets the entry go on (a repeat inside a repeat counts anew each time).
sequenceCommandLine_s *Sequence_NextCommand( sequenceEntry_s *entry, sequenceCommandLine_s *command, int *repeatsDone )
{
	if ( command->repeatCount == 0 )
		return command->nextCommandLine;

	if ( command->repeatCount > 0 )
	{
		int index = Sequence_GetRepeatIndex( entry, command );

		if ( index < 0 )
			return command->nextCommandLine;

		if ( repeatsDone[ index ] >= command->repeatCount )
		{
			repeatsDone[ index ] = 0;
			return command->nextCommandLine;
		}

		repeatsDone[ index ]++;
	}

	return entry->firstCommand;
}


//=========================================================
// Files
//=========================================================

// Parses the text of one file (changed in place). FALSE: an error, reported; nothing of the
// file is kept.
qboolean Sequence_LoadText( char *buffer, const char *fileName, qboolean isGlobal )
{
	sequenceEntry_s	*entry;
	unsigned int	firstSentence = s_sentenceCount;

	strncpy( s_fileName, fileName, sizeof( s_fileName ) - 1 );
	s_fileName[ sizeof( s_fileName ) - 1 ] = '\0';
	s_fileIsGlobal = isGlobal;
	s_failed = FALSE;
	s_pendingEntries = NULL;
	s_line = 1;
	s_cursor = buffer;

	// the mark some editors put in front of a UTF-8 file
	if ( (byte)buffer[0] == 0xEF && (byte)buffer[1] == 0xBB && (byte)buffer[2] == 0xBF )
		s_cursor += 3;

	Sequence_RemoveComments( buffer );
	Sequence_InitFileDefaults( &s_fileDefaults );

	char symbol = Sequence_NextSymbol();

	while ( symbol && !s_failed )
	{
		switch ( symbol )
		{
		case '$':
			symbol = Sequence_ParseModifierList( NULL );
			break;

		case '%':
			symbol = Sequence_ParseEntryBlock();
			break;

		case '!':
			symbol = Sequence_ParseSentencesBlock();
			break;

		default:
			Sequence_ParseError( "outside the entries a line begins with '$' (modifier), '%%' (entry) or '!' (sentences); found %s",
				Sequence_CharName( symbol ) );
			break;
		}
	}

	for ( entry = s_pendingEntries; entry && !s_failed; entry = entry->nextEntry )
		Sequence_ExpandMacros( entry );

	for ( entry = s_pendingEntries; entry && !s_failed; entry = entry->nextEntry )
	{
		Sequence_ResolveModifiers( entry );
		Sequence_CheckEntry( entry );
	}

	Sequence_InitFileDefaults( &s_fileDefaults );

	if ( s_failed )
	{
		while ( s_pendingEntries )
		{
			entry = s_pendingEntries;
			s_pendingEntries = entry->nextEntry;
			Sequence_DeleteEntry( entry );
		}

		Sequence_RemoveSentences( FALSE, firstSentence );
		s_sentenceCount = firstSentence;
		return FALSE;
	}

	// in front of what is loaded, the file's last entry first (a later entry hides an earlier one of the same name)
	if ( s_pendingEntries )
	{
		for ( entry = s_pendingEntries; entry->nextEntry; entry = entry->nextEntry )
			;

		entry->nextEntry = s_entries;
		s_entries = s_pendingEntries;
		s_pendingEntries = NULL;
	}

	return TRUE;
}

// sequences/<fileName>.seq
int Sequence_LoadFile( const char *fileName, qboolean isGlobal )
{
	char	path[128];
	int		length = 0;

	if ( !fileName || !fileName[0] || snprintf( path, sizeof( path ), "sequences/%s.seq", fileName ) >= (int)sizeof( path ) )
		return SEQUENCE_FILE_MISSING;

	char *file = Sequence_HostLoadFile( path, &length );
	if ( !file )
		return SEQUENCE_FILE_MISSING;

	if ( length < 0 )
		length = 0;

	// a copy the parser may change, ended by a 0 whatever the host's buffer ends with
	char *buffer = (char *)malloc( length + 1 );
	memcpy( buffer, file, length );
	buffer[ length ] = '\0';

	Sequence_HostFreeFile( file );

	qboolean parsed = Sequence_LoadText( buffer, fileName, isGlobal );

	free( buffer );

	return parsed ? SEQUENCE_FILE_LOADED : SEQUENCE_FILE_BAD;
}

// Sequence.h: loads sequences/<fileName>.seq
void Sequence_ParseFile( const char *fileName, qboolean isGlobal )
{
	Sequence_LoadFile( fileName, isGlobal );
}

// Sequence.h: a new map. global.seq is read again with every
// map, so a changed file counts from the next map on.
void Sequence_OnLevelLoad( const char *mapName )
{
	Sequence_Clear( TRUE );

	Sequence_ParseFile( "global", TRUE );
	Sequence_ParseFile( mapName, FALSE );
}


//=========================================================
// What the game asks (Sequence.h; in Condition Zero through g_engfuncs.pfnSequenceGet, pfnSequencePickSentence)
//=========================================================

// Sequence.h: the entry of a name. The file name is not looked at: the entries of the map's file and of global.seq
// are one list, the map's first.
sequenceEntry_s *SequenceGet( const char *fileName, const char *entryName )
{
	if ( !entryName || !entryName[0] )
		return NULL;

	for ( sequenceEntry_s *scan = s_entries; scan; scan = scan->nextEntry )
	{
		if ( !stricmp( entryName, scan->entryName ) )
			return scan;
	}

	return NULL;
}

sequenceEntry_s *Sequence_GetEntryList( void )
{
	return s_entries;
}

sentenceGroupEntry_s *Sequence_GetSentenceGroupList( void )
{
	return s_sentenceGroups;
}

// Sequence.h: a random sentence of a group. There is one way to pick, so the pick method is not looked at.
sentenceEntry_s *SequencePickSentence( const char *groupName, int pickMethod, int *picked )
{
	sentenceGroupEntry_s	*group = Sequence_GetSentenceGroup( groupName );
	sentenceEntry_s			*sentence = NULL;
	int						pickedIndex = 0;

	if ( group && group->numSentences > 0 )
	{
		pickedIndex = Sequence_HostRandomLong( 0, group->numSentences - 1 );
		sentence = group->firstSentence;

		for ( int i = pickedIndex; i > 0 && sentence->nextEntry; i-- )
			sentence = sentence->nextEntry;
	}

	if ( picked )
		*picked = pickedIndex;

	return sentence;
}
