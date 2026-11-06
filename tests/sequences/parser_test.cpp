// Parser test for the sequence files: the host of src/dlls/sequence.cpp outside the game (see run_test.sh).
//
//   parser_test <tests/sequences> [<gamedir>:<name> ...]
//
// Checks what tests/sequences/sequences/global.seq and test.seq become, that faulty files are refused whole, and
// that every further <gamedir>/sequences/<name>.seq loads (its entries are printed; with the game's own global.seq
// if <gamedir> has none).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>

typedef int qboolean;
typedef unsigned char byte;
#define _DEF_BYTE_
#include "sequence.h"

static std::string s_root;			// the directory that holds "sequences/"
static std::string s_messages;		// what the parser printed since the last check
static int s_failures = 0;
static int s_nextRandom = 0;

// The host's four functions (sequence.h)
char *Sequence_HostLoadFile( const char *path, int *length )
{
	std::string full = s_root + "/" + path;

	FILE *file = fopen( full.c_str(), "rb" );
	if ( !file )
		return NULL;

	fseek( file, 0, SEEK_END );
	long size = ftell( file );
	fseek( file, 0, SEEK_SET );

	// no 0 at the end: the parser must not need one
	char *buffer = (char *)malloc( size > 0 ? size : 1 );
	*length = (int)fread( buffer, 1, size, file );
	fclose( file );

	return buffer;
}

void Sequence_HostFreeFile( char *buffer )
{
	free( buffer );
}

void Sequence_HostPrint( const char *text )
{
	s_messages += text;
}

int Sequence_HostRandomLong( int low, int high )
{
	int value = low + s_nextRandom;
	return value > high ? high : value;
}

#define CHECK( condition ) \
	do { if ( !( condition ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition ); s_failures++; } } while ( 0 )

static bool Near( float a, float b )
{
	return fabs( a - b ) < 0.0001f;
}

static bool Same( const char *a, const char *b )
{
	if ( !a || !b )
		return a == b;
	return !strcmp( a, b );
}

// An entry's commands that came from "#" and "@" lines (the rest are what is left of the modifiers)
static std::vector<sequenceCommandLine_s *> Lines( sequenceEntry_s *entry )
{
	std::vector<sequenceCommandLine_s *> lines;

	for ( sequenceCommandLine_s *command = entry ? entry->firstCommand : NULL; command; command = command->nextCommandLine )
	{
		if ( command->commandType == SEQUENCE_COMMAND_PAUSE )
			lines.push_back( command );
	}

	return lines;
}

static int CountEntries( void )
{
	int count = 0;
	for ( sequenceEntry_s *entry = Sequence_GetEntryList(); entry; entry = entry->nextEntry )
		count++;
	return count;
}

static void PrintEntries( void )
{
	for ( sequenceEntry_s *entry = Sequence_GetEntryList(); entry; entry = entry->nextEntry )
	{
		printf( "  %%%s (%s.seq%s)\n", entry->entryName, entry->fileName, entry->isGlobal ? ", global" : "" );

		for ( sequenceCommandLine_s *c : Lines( entry ) )
		{
			printf( "    " );
			if ( c->fireTargetNames )
				printf( "fire \"%s\" ", c->fireTargetNames );
			if ( c->killTargetNames )
				printf( "kill \"%s\" ", c->killTargetNames );
			if ( c->clientMessage.pMessage )
				printf( "text \"%s\" [ch %d pos %g %g color %d %d %d / %d %d %d fx %d in %g hold %g out %g fxtime %g] ",
					c->clientMessage.pMessage, c->textChannel, c->clientMessage.x, c->clientMessage.y,
					c->clientMessage.r1, c->clientMessage.g1, c->clientMessage.b1,
					c->clientMessage.r2, c->clientMessage.g2, c->clientMessage.b2, c->clientMessage.effect,
					c->clientMessage.fadein, c->clientMessage.holdtime, c->clientMessage.fadeout, c->clientMessage.fxtime );
			if ( c->soundFileName )
			{
				printf( "sound %s [speaker %s listener %s", c->soundFileName, c->speakerName ? c->speakerName : "-",
					c->listenerName ? c->listenerName : "-" );

				sentenceGroupEntry_s *group = Sequence_GetSentenceGroup( c->soundFileName );
				if ( group )
					printf( ", %u sentence(s) of the file", group->numSentences );
				printf( "] " );
			}
			if ( c->sentenceName )
				printf( "sentence %s ", c->sentenceName );
			if ( c->repeatCount )
				printf( "repeat %d ", c->repeatCount );
			if ( c->delay )
				printf( "pause %g", c->delay );
			printf( "\n" );
		}
	}
}

// Everything the parser holds, every command and every field: what "--dump" prints
static void DumpAll( void )
{
	for ( sequenceEntry_s *entry = Sequence_GetEntryList(); entry; entry = entry->nextEntry )
	{
		printf( "entry \"%s\" file \"%s\" global %d\n", entry->entryName, entry->fileName, entry->isGlobal );

		for ( sequenceCommandLine_s *c = entry->firstCommand; c; c = c->nextCommandLine )
		{
			printf( " cmd type %d bits %#x delay %.9g repeat %d channel %d", c->commandType, c->modifierBitField, c->delay,
				c->repeatCount, c->textChannel );
			printf( " msg fx %d c1 %d,%d,%d,%d c2 %d,%d,%d,%d pos %.9g %.9g in %.9g out %.9g hold %.9g fxtime %.9g",
				c->clientMessage.effect, c->clientMessage.r1, c->clientMessage.g1, c->clientMessage.b1, c->clientMessage.a1,
				c->clientMessage.r2, c->clientMessage.g2, c->clientMessage.b2, c->clientMessage.a2,
				c->clientMessage.x, c->clientMessage.y, c->clientMessage.fadein, c->clientMessage.fadeout,
				c->clientMessage.holdtime, c->clientMessage.fxtime );
			printf( " name [%s] text [%s] speaker [%s] listener [%s] sound [%s] sentence [%s] fire [%s] kill [%s]\n",
				c->clientMessage.pName ? c->clientMessage.pName : "<null>", c->clientMessage.pMessage ? c->clientMessage.pMessage : "<null>",
				c->speakerName ? c->speakerName : "<null>", c->listenerName ? c->listenerName : "<null>",
				c->soundFileName ? c->soundFileName : "<null>", c->sentenceName ? c->sentenceName : "<null>",
				c->fireTargetNames ? c->fireTargetNames : "<null>", c->killTargetNames ? c->killTargetNames : "<null>" );
		}
	}

	for ( sentenceGroupEntry_s *group = Sequence_GetSentenceGroupList(); group; group = group->nextEntry )
	{
		printf( "group \"%s\" sentences %u\n", group->groupName, group->numSentences );

		for ( sentenceEntry_s *sentence = group->firstSentence; sentence; sentence = sentence->nextEntry )
			printf( " sentence %u global %d [%s]\n", sentence->index, sentence->isGlobal, sentence->data );
	}
}

// A faulty file: refused, with a message, and nothing of it kept
static void CheckBad( const char *text, const char *expectInMessage )
{
	int before = CountEntries();
	char *buffer = strdup( text );

	s_messages.clear();
	qboolean parsed = Sequence_LoadText( buffer, "bad", 0 );
	free( buffer );

	bool ok = !parsed && CountEntries() == before && s_messages.find( "sequences/bad.seq" ) != std::string::npos
		&& s_messages.find( expectInMessage ) != std::string::npos && !SequenceGet( NULL, "a" )
		&& !Sequence_GetSentenceGroup( "BAD" );

	if ( !ok )
	{
		printf( "FAIL: faulty file not refused as expected (\"%s\")\n  file: %s\n  message: %s\n", expectInMessage, text,
			s_messages.c_str() );
		s_failures++;
	}
}

static void CheckGood( const char *text, int entries )
{
	char *buffer = strdup( text );

	Sequence_Clear( 1 );
	s_messages.clear();
	qboolean parsed = Sequence_LoadText( buffer, "good", 0 );
	free( buffer );

	if ( !parsed || CountEntries() != entries || !s_messages.empty() )
	{
		printf( "FAIL: file not loaded as expected (%d entries)\n  file: %s\n  message: %s\n", entries, text, s_messages.c_str() );
		s_failures++;
	}
}

static void TestFixture( void )
{
	s_messages.clear();

	Sequence_Clear( 1 );
	CHECK( Sequence_LoadFile( "missing", 0 ) == SEQUENCE_FILE_MISSING );
	CHECK( Sequence_LoadFile( "global", 1 ) == SEQUENCE_FILE_LOADED );
	CHECK( Sequence_LoadFile( "test", 0 ) == SEQUENCE_FILE_LOADED );
	CHECK( s_messages.empty() );
	if ( !s_messages.empty() )
		printf( "%s", s_messages.c_str() );

	CHECK( CountEntries() == 4 );

	// entries: names without case, the file, global or not
	sequenceEntry_s *intro = SequenceGet( NULL, "INTRO" );
	sequenceEntry_s *loop = SequenceGet( "ignored", "loop" );
	sequenceEntry_s *title = SequenceGet( NULL, "TitleStyle" );

	CHECK( intro && loop && title );
	CHECK( !SequenceGet( NULL, "nothing" ) && !SequenceGet( NULL, "" ) && !SequenceGet( NULL, NULL ) );
	if ( !intro || !loop || !title )
		return;

	CHECK( Same( intro->fileName, "test" ) && !intro->isGlobal );
	CHECK( Same( title->fileName, "global" ) && title->isGlobal );

	// %intro, line by line
	std::vector<sequenceCommandLine_s *> lines = Lines( intro );
	CHECK( lines.size() == 9 );
	if ( lines.size() != 9 )
		return;

	// #pause = 1.5
	CHECK( Near( lines[0]->delay, 1.5f ) && !lines[0]->fireTargetNames && !lines[0]->clientMessage.pMessage );
	// the file's defaults: "$channel = 2, holdtime = 3", "$color = 10 20 30", the rest built in
	CHECK( lines[0]->textChannel == 2 && Near( lines[0]->clientMessage.holdtime, 3 ) );
	CHECK( lines[0]->clientMessage.r1 == 10 && lines[0]->clientMessage.g1 == 20 && lines[0]->clientMessage.b1 == 30 );
	CHECK( lines[0]->clientMessage.r2 == 255 && Near( lines[0]->clientMessage.x, 0.5f ) && Near( lines[0]->clientMessage.fadein, 0.2f ) );
	CHECK( Near( lines[0]->clientMessage.fxtime, 1.0f ) );	// "$fxtime = 4" comes after the entry

	// #firetargets = "door1", pause = .25 (over two lines)
	CHECK( Same( lines[1]->fireTargetNames, "door1" ) && Near( lines[1]->delay, 0.25f ) );

	// @TitleStyle: the macro's command itself does nothing
	CHECK( !lines[2]->clientMessage.pName && !lines[2]->delay );

	// #text = "Hello<line end>world", holdtime = 7: global.seq's style, the hold time for this line only
	CHECK( Same( lines[3]->clientMessage.pMessage, "Hello\nworld" ) );
	CHECK( Near( lines[3]->clientMessage.x, -1 ) && Near( lines[3]->clientMessage.y, 0.3f ) );
	CHECK( Near( lines[3]->clientMessage.fadein, 0.1f ) && Near( lines[3]->clientMessage.fadeout, 0.5f ) );
	CHECK( Near( lines[3]->clientMessage.holdtime, 7 ) );
	CHECK( lines[3]->clientMessage.r1 == 255 && lines[3]->clientMessage.r2 == 128 && lines[3]->clientMessage.effect == 2 );
	CHECK( lines[3]->textChannel == 1 );

	// #text = "second": the style stays, the hold time is the style's again
	CHECK( Same( lines[4]->clientMessage.pMessage, "second" ) && Near( lines[4]->clientMessage.holdtime, -1 ) );
	CHECK( lines[4]->textChannel == 1 && !lines[4]->speakerName );

	// @Style #sound = TEST_LINE, speaker = guard1, listener = player, channel = 3, pause = 2
	CHECK( Same( lines[6]->soundFileName, "TEST_LINE" ) && Near( lines[6]->delay, 2 ) );
	CHECK( Same( lines[6]->speakerName, "guard1" ) && Same( lines[6]->listenerName, "player" ) && lines[6]->textChannel == 3 );
	CHECK( Near( lines[6]->clientMessage.x, 0.1f ) && Near( lines[6]->clientMessage.y, 0.2f ) );

	// #killtargets = "door1": the line's speaker, listener and channel are gone, the style's "$speaker = none" stays
	CHECK( Same( lines[7]->killTargetNames, "door1" ) && Same( lines[7]->speakerName, "none" ) );
	CHECK( !lines[7]->listenerName && lines[7]->textChannel == 1 );

	// #sentence = TEST_LINE (behind a comment over two lines)
	CHECK( Same( lines[8]->sentenceName, "TEST_LINE" ) );

	// %loop: #firetargets, #repeat = 3, #gosub = intro (and intro's 9 lines), #repeat = -1, pause = 0.5
	lines = Lines( loop );
	CHECK( lines.size() == 13 );
	if ( lines.size() != 13 )
		return;

	CHECK( Same( lines[0]->fireTargetNames, "tick" ) && Near( lines[0]->delay, 1 ) );
	CHECK( lines[1]->repeatCount == 3 && Sequence_GetRepeatIndex( loop, lines[1] ) == 0 );
	CHECK( !lines[2]->clientMessage.pName );
	CHECK( Same( lines[6]->clientMessage.pMessage, "Hello\nworld" ) && Near( lines[6]->clientMessage.holdtime, 7 ) );
	CHECK( Near( lines[6]->clientMessage.fxtime, 4 ) );		// %loop is after "$fxtime = 4"
	CHECK( Same( lines[9]->soundFileName, "TEST_LINE" ) && Same( lines[9]->speakerName, "guard1" ) );
	CHECK( lines[12]->repeatCount == -1 && Near( lines[12]->delay, 0.5f ) && Sequence_GetRepeatIndex( loop, lines[12] ) == -1 );

	// sentences
	sentenceGroupEntry_s *group = Sequence_GetSentenceGroup( "test_line" );
	CHECK( group && group->numSentences == 2 );
	CHECK( Sequence_GetSentenceGroup( "TEST_MULTI" ) && Sequence_GetSentenceGroup( "plain" ) && Sequence_GetSentenceGroup( "GLOBAL_HELLO" ) );
	CHECK( !Sequence_GetSentenceGroup( "TEST_LINE0" ) && !Sequence_GetSentenceGroup( NULL ) );

	char sound[SEQUENCE_SOUND_NAME_MAX];
	int words = 0, picked = -1;

	s_nextRandom = 0;
	sentenceEntry_s *sentence = SequencePickSentence( "TEST_LINE", 0, &picked );
	CHECK( sentence && picked == 0 && Same( sentence->data, "test/line_a" ) && !sentence->isGlobal );
	CHECK( sentence && Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), &words ) && Same( sound, "test/line_a.wav" ) && words == 1 );

	s_nextRandom = 1;
	sentence = SequencePickSentence( "TEST_LINE", 0, &picked );
	CHECK( sentence && picked == 1 && Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), &words ) && Same( sound, "test/line_b.wav" ) );
	s_nextRandom = 0;

	sentence = SequencePickSentence( "TEST_MULTI", 0, NULL );
	CHECK( sentence && Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), &words ) && Same( sound, "test/one.wav" ) && words == 3 );

	sentence = SequencePickSentence( "plain", 0, NULL );
	CHECK( sentence && Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), &words ) && Same( sound, "vox/hello.wav" ) );

	sentence = SequencePickSentence( "GLOBAL_HELLO", 0, NULL );
	CHECK( sentence && sentence->isGlobal );

	CHECK( !SequencePickSentence( "nothing", 0, &picked ) && picked == 0 );
	CHECK( !Sequence_SentenceSound( "(p120)", sound, sizeof( sound ), &words ) && words == 0 );
	CHECK( Sequence_SentenceSound( "hgrunt/grenade!", sound, sizeof( sound ), NULL ) && Same( sound, "hgrunt/grenade!.wav" ) );
	CHECK( Sequence_SentenceSound( "hgrunt/go.", sound, sizeof( sound ), NULL ) && Same( sound, "hgrunt/go.wav" ) );

	// a map change: the map's entries and sentences go, global.seq's stay
	Sequence_Clear( 0 );
	CHECK( CountEntries() == 1 && SequenceGet( NULL, "TitleStyle" ) && !SequenceGet( NULL, "intro" ) );
	CHECK( Sequence_GetSentenceGroup( "GLOBAL_HELLO" ) && !Sequence_GetSentenceGroup( "TEST_LINE" ) );

	// the whole level load, as the game does it
	Sequence_OnLevelLoad( "test" );
	CHECK( CountEntries() == 4 && s_messages.empty() );
	Sequence_OnLevelLoad( "missing" );
	CHECK( CountEntries() == 1 && s_messages.empty() );
}

// What a sentence is to the game: one sound file, or a sentence for the clients' sentence player
static void TestSentences( void )
{
	char		sound[SEQUENCE_SOUND_NAME_MAX];
	const char	*why = NULL;

	CHECK( Sequence_IsSimpleSentence( "test/line_a" ) && Sequence_IsSimpleSentence( "test/line_b.wav" ) && Sequence_IsSimpleSentence( "hello" ) );
	CHECK( Sequence_IsSimpleSentence( "test/stop." ) );
	CHECK( !Sequence_IsSimpleSentence( "test/one two" ) && !Sequence_IsSimpleSentence( "test/one(p120)" ) );
	CHECK( !Sequence_IsSimpleSentence( "test/one, two" ) && !Sequence_IsSimpleSentence( "" ) && !Sequence_IsSimpleSentence( NULL ) );

	CHECK( Sequence_IsSpeakableSentence( "hgrunt/clik(p120) grenade! clik", &why ) && Same( why, "" ) );
	CHECK( Sequence_IsSpeakableSentence( "test/one two(p120 t20), three.", NULL ) );
	CHECK( !Sequence_IsSpeakableSentence( "test/one\" ; quit", &why ) && Same( why, "it has a character a sentence cannot have" ) );
	CHECK( !Sequence_IsSpeakableSentence( "test/one; quit", NULL ) && !Sequence_IsSpeakableSentence( "test/one\ntwo", NULL ) );
	CHECK( !Sequence_IsSpeakableSentence( "test/one $two", NULL ) && !Sequence_IsSpeakableSentence( "test/one %two", NULL ) );
	CHECK( !Sequence_IsSpeakableSentence( "test/one other/two", &why ) && Same( why, "only its first word may name a directory" ) );
	CHECK( !Sequence_IsSpeakableSentence( "(p120) ,", &why ) && Same( why, "it has no word" ) );
	CHECK( !Sequence_IsSpeakableSentence( "", NULL ) && !Sequence_IsSpeakableSentence( NULL, NULL ) );

	std::string longSentence = "test/a";
	while ( longSentence.size() <= SEQUENCE_SPEAK_MAX )
		longSentence += " word";
	CHECK( !Sequence_IsSpeakableSentence( longSentence.c_str(), &why ) && Same( why, "it is too long" ) );

	// The client's check (game_shared/sequence_speak.h; cl_dll/hud.cpp runs  spk "<sentence>"  only if it passes):
	// whatever the server lets through passes, and nothing that could end the quoted string or the command does
	static const char *speakable[] = { "hgrunt/clik(p120) grenade! clik", "test/one two(p120 t20), three.", "hello there",
		"test/one\ttwo" };
	for ( const char *text : speakable )
		CHECK( Sequence_IsSpeakableSentence( text, NULL ) && Sequence_IsSpeakSafe( text ) );

	static const char *unsafe[] = { "", "a\"b", "a;b", "a\nb", "a\rb", "a\\b", "a$b", "a%b", "a'b", "a`b", "a{b", "a:b", "a\x01" "b",
		"a\xE9" "b", "a=b", "a#b", "a<b", "a|b", "a&b", "a~b", "a+b", "a*b", "a@b", "a?b", "a[b" };
	for ( const char *text : unsafe )
		CHECK( !Sequence_IsSpeakSafe( text ) && !Sequence_IsSpeakableSentence( text, NULL ) );
	CHECK( !Sequence_IsSpeakSafe( NULL ) && !Sequence_IsSpeakSafe( longSentence.c_str() ) );
	CHECK( Sequence_IsSpeakSafe( std::string( SEQUENCE_SPEAK_MAX, 'a' ).c_str() ) && !Sequence_IsSpeakSafe( std::string( SEQUENCE_SPEAK_MAX + 1, 'a' ).c_str() ) );

	// the files of a sentence, for the download table
	const char *sentence = "test/one two(p120 t20), three. four.wav";
	const char *files[] = { "test/one.wav", "test/two.wav", "test/_comma.wav", "test/three.wav", "test/_period.wav", "test/four.wav" };

	for ( int i = 0; i < 6; i++ )
		CHECK( Sequence_SentenceFile( sentence, i, sound, sizeof( sound ) ) && Same( sound, files[i] ) );
	CHECK( !Sequence_SentenceFile( sentence, 6, sound, sizeof( sound ) ) && !Sequence_SentenceFile( sentence, -1, sound, sizeof( sound ) ) );

	CHECK( Sequence_SentenceFile( "hello there", 0, sound, sizeof( sound ) ) && Same( sound, "vox/hello.wav" ) );
	CHECK( Sequence_SentenceFile( "hello there", 1, sound, sizeof( sound ) ) && Same( sound, "vox/there.wav" ) );
	CHECK( Sequence_SentenceFile( "a/b/clik(p120) grenade!", 1, sound, sizeof( sound ) ) && Same( sound, "a/b/grenade!.wav" ) );
	CHECK( !Sequence_SentenceFile( "", 0, sound, sizeof( sound ) ) && !Sequence_SentenceFile( NULL, 0, sound, sizeof( sound ) ) );
}

// Runs an entry the way trigger_sequence does (Sequence_NextCommand after every command) and counts how often
// each target is fired; stops after maxCommands. Returns the commands run, or -1 if the entry was still running.
static int RunEntry( const char *entryName, int maxCommands, int *firedX, int *firedY )
{
	sequenceEntry_s	*entry = SequenceGet( NULL, entryName );
	int				repeatsDone[SEQUENCE_MAX_REPEATS] = { 0 };
	int				commands = 0;

	*firedX = *firedY = 0;

	for ( sequenceCommandLine_s *command = entry ? entry->firstCommand : NULL; command; )
	{
		if ( commands++ >= maxCommands )
			return -1;

		if ( Same( command->fireTargetNames, "x" ) )
			(*firedX)++;
		if ( Same( command->fireTargetNames, "y" ) )
			(*firedY)++;

		command = Sequence_NextCommand( entry, command, repeatsDone );
	}

	return commands;
}

// "repeat": -1 forever, N that many times
static void TestRepeat( void )
{
	int x, y;

	// no repeat: once through
	CheckGood( "%a { #firetargets = \"x\", pause = 1\n #firetargets = \"y\" }", 1 );
	CHECK( RunEntry( "a", 1000, &x, &y ) > 0 && x == 1 && y == 1 );

	// repeat = 2: the lines before it run 3 times, then the entry goes on and ends
	CheckGood( "%a { #firetargets = \"x\", pause = 1\n #repeat = 2\n #firetargets = \"y\" }", 1 );
	CHECK( RunEntry( "a", 1000, &x, &y ) > 0 && x == 3 && y == 1 );

	// repeat = -1: never ends, never reaches what follows
	CheckGood( "%a { #firetargets = \"x\", pause = 1\n #repeat = -1\n #firetargets = \"y\" }", 1 );
	CHECK( RunEntry( "a", 1000, &x, &y ) == -1 && x > 300 && y == 0 );

	// CZ's cz_hankagai2 "%impatient": fire, pause 7, repeat = 2. Here 3 times; in CZ's game forever.
	CheckGood( "%impatient\n{\n\t#firetargets=\"x\"\n\t#pause = 7\n\t#repeat = 2\n}", 1 );
	CHECK( RunEntry( "impatient", 1000, &x, &y ) > 0 && x == 3 );

	// a repeat inside a repeat counts anew each time: x (1 + 1) * (2 + 1) times, y 2 + 1 times
	CheckGood( "%a { #firetargets = \"x\"\n #repeat = 1\n #firetargets = \"y\"\n #repeat = 2 }", 1 );
	CHECK( RunEntry( "a", 1000, &x, &y ) > 0 && x == 6 && y == 3 );

	// a counted repeat, then an endless one around it
	CheckGood( "%a { #firetargets = \"x\"\n #repeat = 1\n #firetargets = \"y\", pause = 1\n #repeat = -1 }", 1 );
	CHECK( RunEntry( "a", 600, &x, &y ) == -1 && x > 2 * y - 3 && x < 2 * y + 3 && y > 50 );

	// a repeat that comes from a macro
	CheckGood( "%twice { #repeat = 1 }\n%a { #firetargets = \"x\"\n @twice\n #firetargets = \"y\" }", 2 );
	CHECK( RunEntry( "a", 1000, &x, &y ) > 0 && x == 2 && y == 1 );

	Sequence_Clear( 1 );
}

// SBO's own files: the global.seq the game ships and the SDK's example, which uses its styles
static void TestShipped( const char *repository )
{
	std::string root = repository;

	Sequence_Clear( 1 );
	s_messages.clear();

	s_root = root + "/gamedir/syndicate";
	CHECK( Sequence_LoadFile( "global", 1 ) == SEQUENCE_FILE_LOADED );

	static const char *styles[] = { "TitleStyle", "SubtitleStyle", "ObjectiveStyle", "RadioStyle", "SignStyle" };
	for ( const char *style : styles )
	{
		sequenceEntry_s *entry = SequenceGet( NULL, style );
		CHECK( entry && entry->isGlobal );
	}
	CHECK( CountEntries() == 5 && !Sequence_GetSentenceGroup( "anything" ) );

	s_root = root + "/sdk/examples";
	CHECK( Sequence_LoadFile( "co_example", 0 ) == SEQUENCE_FILE_LOADED );
	CHECK( s_messages.empty() );
	if ( !s_messages.empty() )
		printf( "%s", s_messages.c_str() );

	// what the example's comments promise
	int x, y;
	CHECK( SequenceGet( NULL, "example_intro" ) && SequenceGet( NULL, "example_alarm" ) && SequenceGet( NULL, "example_countdown" ) );
	CHECK( RunEntry( "example_alarm", 1000, &x, &y ) == -1 );		// repeat = -1
	CHECK( RunEntry( "example_countdown", 1000, &x, &y ) > 0 );		// repeat = N ends

	Sequence_Clear( 1 );
}

static void TestSmallFiles( void )
{
	CheckGood( "", 0 );
	CheckGood( "// nothing\n/* at\nall */\n", 0 );
	CheckGood( "%a\n{\n}\n", 1 );
	CheckGood( "\xEF\xBB\xBF%a { #text = \"caf\xC3\xA9\" }", 1 );					// a UTF-8 file with its mark
	CheckGood( "%a { #pause = 1 #pause = 2 }", 1 );						// a line end is not needed between lines
	CheckGood( "$effect = 1\n%a\n{\n\t$channel = 1\n}\n%b { @a }\n", 2 );
	CheckGood( "%a { #firetargets = \"x\", pause = 1, repeat = -1 }", 1 );
	CheckGood( "%a { #repeat = 2.9 }", 1 );								// counted: ends by itself
	CheckGood( "%b { @a }\n%a { #pause = 1 }\n%c { @b #repeat = -1 }", 3 );	// a macro defined later; its pause counts
	CheckGood( "!SENTENCES\n{\n\tA0 x/y\r\n\tA1 x/z\r\n}\r\n%a { #SOUND = A, Speaker = none }\r\n", 1 );

	Sequence_Clear( 1 );

	CheckBad( "junk", "outside the entries" );
	CheckBad( "%a { #bogus = 1 }", "unknown command \"bogus\"" );
	CheckBad( "%a { $bogus = 1 }", "unknown modifier \"bogus\"" );
	CheckBad( "%a { $pause = 1\n}", "is a command: it goes on a '#' line" );
	CheckBad( "%a { #pause = 1", "no closing '}'" );
	CheckBad( "%a { #pause 1 }", "expected '='" );
	CheckBad( "%a { #pause = abc }", "not a number" );
	CheckBad( "%a { #pause = -1 }", "cannot be negative" );
	CheckBad( "%a { #repeat = many, pause = 1 }", "not a number" );
	CheckBad( "%a\n{\n\t$color = 1 2 300\n}", "color value 300" );
	CheckBad( "%a { #firetargets = door }", "expected a quoted string" );
	CheckBad( "%a { #text = \"abc }", "closing quote" );
	CheckBad( "%a { #noop = 1 }", "cannot be used in a file" );
	CheckBad( "%a { #setdefaults = 1 }", "cannot be used in a file" );
	CheckBad( "%a { #pause = 1, #pause = 2 }", "only one '#' per line" );
	CheckBad( "%a\n{\n\t$channel = 1 }", "expected ',' or the end of the line" );
	CheckBad( "%a { #repeat = -1 }", "repeats forever without a pause" );
	CheckBad( "%a { @a }", "uses itself" );
	CheckBad( "%a { @b }\n%b { @a }", "uses itself" );
	CheckBad( "%a { @missing }", "the macro \"missing\"" );
	CheckBad( "%a { }\n%A { }", "in the file twice" );
	CheckBad( "!sounds { }", "unknown block" );
	CheckBad( "!sentences {\n\tBAD0 test/a\n", "no closing '}'" );
	CheckBad( "!sentences {\n\tBAD0\n}", "is empty" );	// name and sentence go on one line
	CheckBad( "!sentences {\n\tBAD0 test/a\n}\n%a { #pause = x }", "not a number" );	// the sentences go with the file

	// the line of an error
	s_messages.clear();
	char *buffer = strdup( "// one\n%a\n{\n\t#pause = 1\n\t/* five\n\tsix */\n\t#bogus = 1\n}\n" );
	Sequence_LoadText( buffer, "bad", 0 );
	free( buffer );
	CHECK( s_messages == "sequences/bad.seq, line 7: unknown command \"bogus\"\n" );

	Sequence_Clear( 1 );
}

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		printf( "usage: parser_test <tests/sequences> [<gamedir>:<name> ...]\n" );
		return 2;
	}

	// parser_test <tests/sequences> --dump <gamedir>:<name> ...: the whole parsed content of each, and what the
	// parser said, nothing else (to compare two versions of the parser)
	if ( argc > 2 && !strcmp( argv[2], "--dump" ) )
	{
		for ( int i = 3; i < argc; i++ )
		{
			const char *colon = strrchr( argv[i], ':' );
			if ( !colon )
				return 2;

			s_root.assign( argv[i], colon - argv[i] );
			s_messages.clear();

			Sequence_Clear( 1 );
			int global = Sequence_LoadFile( "global", 1 );
			int map = Sequence_LoadFile( colon + 1, 0 );

			printf( "==== %s global %d map %d\n%s", colon + 1, global, map, s_messages.c_str() );
			DumpAll();
		}

		Sequence_Clear( 1 );
		return 0;
	}

	s_root = argv[1];
	TestFixture();
	TestSmallFiles();
	TestSentences();
	TestRepeat();

	// the repository: tests/sequences/../..
	TestShipped( ( std::string( argv[1] ) + "/../.." ).c_str() );

	for ( int i = 2; i < argc; i++ )
	{
		const char *colon = strrchr( argv[i], ':' );
		if ( !colon )
		{
			printf( "FAIL: \"%s\" is not <gamedir>:<name>\n", argv[i] );
			s_failures++;
			continue;
		}

		s_root.assign( argv[i], colon - argv[i] );
		s_messages.clear();

		Sequence_Clear( 1 );
		int global = Sequence_LoadFile( "global", 1 );

		// no global.seq there: the game's own, as a player's game directory would have it
		if ( global == SEQUENCE_FILE_MISSING )
		{
			std::string given = s_root;

			s_root = std::string( argv[1] ) + "/../../gamedir/syndicate";
			global = Sequence_LoadFile( "global", 1 );
			s_root = given;
		}

		int map = Sequence_LoadFile( colon + 1, 0 );

		printf( "%s/sequences/%s.seq: %s%s\n", s_root.c_str(), colon + 1,
			map == SEQUENCE_FILE_LOADED ? "loaded" : ( map == SEQUENCE_FILE_BAD ? "ERROR" : "MISSING" ),
			global == SEQUENCE_FILE_LOADED ? " (with global.seq)" : ( global == SEQUENCE_FILE_BAD ? " (global.seq: ERROR)" : "" ) );
		printf( "%s", s_messages.c_str() );

		if ( map != SEQUENCE_FILE_LOADED || global == SEQUENCE_FILE_BAD )
			s_failures++;
		else
			PrintEntries();
	}

	Sequence_Clear( 1 );

	if ( s_failures )
	{
		printf( "FAIL (%d)\n", s_failures );
		return 1;
	}

	printf( "PASS\n" );
	return 0;
}
