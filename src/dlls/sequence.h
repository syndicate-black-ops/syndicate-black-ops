//=========================================================
// Sequence files (sequences/<map>.seq, sequences/global.seq): the game's own sequence manager, see sequence.cpp and
// docs/SEQUENCES.md. The structures and the four functions of the engine's manager are in common/Sequence.h.
//=========================================================

#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "Sequence.h"
#include "sequence_speak.h"		// game_shared: SEQUENCE_SPEAK_MAX, the characters of a spoken sentence

// Sequence_LoadFile's results
#define SEQUENCE_FILE_MISSING	0	// no such file
#define SEQUENCE_FILE_LOADED	1
#define SEQUENCE_FILE_BAD		2	// the file exists but has an error: nothing of it is kept

// The most "repeat" commands with a count in one entry (trigger_sequence counts each of them on its own)
#define SEQUENCE_MAX_REPEATS	8

// The longest sound name Sequence_SentenceSound makes
#define SEQUENCE_SOUND_NAME_MAX	64


int Sequence_LoadFile( const char *fileName, qboolean isGlobal );
qboolean Sequence_LoadText( char *buffer, const char *fileName, qboolean isGlobal );
void Sequence_Clear( qboolean includeGlobal );

sequenceEntry_s *Sequence_GetEntryList( void );
sentenceGroupEntry_s *Sequence_GetSentenceGroupList( void );
sentenceGroupEntry_s *Sequence_GetSentenceGroup( const char *groupName );
int Sequence_GetRepeatIndex( sequenceEntry_s *entry, sequenceCommandLine_s *command );
sequenceCommandLine_s *Sequence_NextCommand( sequenceEntry_s *entry, sequenceCommandLine_s *command, int *repeatsDone );
qboolean Sequence_SentenceSound( const char *data, char *sound, int soundSize, int *wordCount );
qboolean Sequence_IsSimpleSentence( const char *data );
qboolean Sequence_IsSpeakableSentence( const char *data, const char **why );
qboolean Sequence_SentenceFile( const char *data, int index, char *sound, int soundSize );

// What sequence.cpp needs from its host: the game (trigger_sequence.cpp) or the parser test (tests/sequences)
char *Sequence_HostLoadFile( const char *path, int *length );
void Sequence_HostFreeFile( char *buffer );
void Sequence_HostPrint( const char *text );
int Sequence_HostRandomLong( int low, int high );

#ifdef VALVE_DLL
// World precache (world.cpp): loads the map's sequence files, puts the map's file in the download table
void Sequence_LevelInit( void );
#endif

#endif // SEQUENCE_H
