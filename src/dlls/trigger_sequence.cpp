//=========================================================
// trigger_sequence: runs an entry of the map's sequence file (sequences/<map>.seq, sequences/global.seq) when it
// is triggered. CS: Condition Zero Deleted Scenes' entity; keyvalues and spawnflags: sdk/syndicate.fgd, the file format
// and every command: docs/SEQUENCES.md.
//
// Port of czeror-sdk dlls/CTriggerSequence.cpp (the twhl-community CZ: Deleted Scenes SDK), function by function.
// There the entries come from the engine (g_engfuncs.pfnSequenceGet); here from the game's own sequence manager
// (sequence.cpp), so the entity works the same on every engine.
//
// An entry is a list of commands, each with the delay to the next. Use starts the entry from its first command
// (a running one starts over); Think runs the commands that are due. A command may fire targets, remove targets,
// show a HUD text to every player (what game_text shows) and play a sound, by itself or from a speaker.
//
// Not czeror-sdk's: USE_OFF stops a running entry (there every use type starts it).
//
// Not CZ's: its Restart (CS round restart). Nothing in an SBO map is reset between rounds (docs/ROUNDS.md,
// docs/GAME_TIMER.md "Rounds"), so a running sequence keeps running and a "fire once" one stays used up until
// the map changes.
//=========================================================

#include	<stdarg.h>

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"basemonster.h"
#include	"sequence.h"

extern int gmsgSeqSpeak;

#define SF_SEQUENCE_FIRE_ONCE		0x0001

// The most commands one frame runs: an entry that never pauses is stopped (a counted "repeat" without a pause)
#define SEQUENCE_MAX_COMMANDS_PER_THINK	4096


//=========================================================
// The host of sequence.cpp: file access, console, random numbers
//=========================================================

char *Sequence_HostLoadFile( const char *path, int *length )
{
	return (char *)LOAD_FILE_FOR_ME( (char *)path, length );
}

void Sequence_HostFreeFile( char *buffer )
{
	FREE_FILE( buffer );
}

// The server console, whatever "developer" is: these are a map's errors, and a mapper must see them
void Sequence_HostPrint( const char *text )
{
	g_engfuncs.pfnServerPrint( text );
}

int Sequence_HostRandomLong( int low, int high )
{
	return RANDOM_LONG( low, high );
}

static void Sequence_Warning( const char *format, ... )
{
	va_list	argptr;
	char	text[512];

	va_start( argptr, format );
	vsnprintf( text, sizeof( text ), format, argptr );
	va_end( argptr );
	text[ sizeof( text ) - 1 ] = '\0';

	g_engfuncs.pfnServerPrint( text );
}


//=========================================================
// Level load
//=========================================================

// What the level load found, for the entities' warnings
static int s_mapFileState = SEQUENCE_FILE_MISSING;

// The sound files precached for the map's sequences. The engine has 512 sounds per map for everything and stops
// the map when they run out, so the sequences take a share and no more (CZ's biggest files name over 300).
#define SEQUENCE_MAX_SOUNDS		128

static int s_precachedSounds[SEQUENCE_MAX_SOUNDS];	// string_t
static int s_numPrecachedSounds;

static BOOL Sequence_IsSoundPrecached( const char *sound )
{
	for ( int i = 0; i < s_numPrecachedSounds; i++ )
	{
		if ( !stricmp( STRING( s_precachedSounds[i] ), sound ) )
			return TRUE;
	}

	return FALSE;
}

// The sound files of the map's sentences that the clients' own sentence player plays ("spk"): in the download
// table as plain files, which takes none of the 512 sounds. The 512 files of that table are shared too.
#define SEQUENCE_MAX_SPEAK_FILES	128

static int s_speakFiles[SEQUENCE_MAX_SPEAK_FILES];	// string_t
static int s_numSpeakFiles;

// A sentence the clients play themselves (SpeakSentence): its words' files go in the download table
static void Sequence_PrecacheSpeakFiles( sequenceEntry_s *entry, sentenceGroupEntry_s *group, const char *data )
{
	char sound[SEQUENCE_SOUND_NAME_MAX];
	char path[SEQUENCE_SOUND_NAME_MAX + 8];

	for ( int index = 0; Sequence_SentenceFile( data, index, sound, sizeof( sound ) ); index++ )
	{
		int	i, length = 0;

		snprintf( path, sizeof( path ), "sound/%s", sound );

		for ( i = 0; i < s_numSpeakFiles; i++ )
		{
			if ( !stricmp( STRING( s_speakFiles[i] ), path ) )
				break;
		}

		if ( i < s_numSpeakFiles )
			continue;

		// Only a file the server has: a missing one would be asked for by every client. The pause files
		// (_comma, _period) are the game's own in "vox"; a map's directory may do without them.
		byte *file = LOAD_FILE_FOR_ME( path, &length );
		if ( file )
			FREE_FILE( file );

		if ( !file )
		{
			if ( !strstr( sound, "/_comma.wav" ) && !strstr( sound, "/_period.wav" ) )
				Sequence_Warning( "sequences/%s.seq, sentence group \"%s\": %s of \"%s\" is not on the server\n", entry->fileName,
					group->groupName, path, data );
			continue;
		}

		if ( s_numSpeakFiles >= SEQUENCE_MAX_SPEAK_FILES )
		{
			Sequence_Warning( "sequences/%s.seq, sentence group \"%s\": more than %d sound files in the map's spoken sentences, %s is not sent to the clients\n",
				entry->fileName, group->groupName, SEQUENCE_MAX_SPEAK_FILES, path );
			continue;
		}

		// the engine keeps the pointer for the whole map
		s_speakFiles[ s_numSpeakFiles ] = ALLOC_STRING( path );
		PRECACHE_GENERIC( (char *)STRING( s_speakFiles[ s_numSpeakFiles ] ) );
		s_numSpeakFiles++;
	}
}

// The sounds of a "sound" command: the file's sentence group of that name, or else a group of sentences.txt
// (nothing to precache). Without an entry: a new map, nothing is done yet.
//
// A sentence that is one sound file is precached as a sound and played as one. Any other sentence (several
// words, pauses, "(p120)" settings) is the engine's sentence player's business: without a speaker every client
// plays it itself (SpeakSentence) and only needs the files; from a speaker it cannot be played that way (the
// client's sentence player has no place in the world), so the monster says its first word.
static void Sequence_PrecacheSound( sequenceEntry_s *entry, const char *soundName, BOOL hasSpeaker )
{
	// the groups done for this map, with and without a speaker (an entry's lines, and a map's entries, often
	// share one)
	static sentenceGroupEntry_s	*precached[2][256];
	static int					numPrecached[2];

	if ( !entry )
	{
		numPrecached[0] = numPrecached[1] = 0;
		return;
	}

	sentenceGroupEntry_s *group = Sequence_GetSentenceGroup( soundName );

	if ( !group )
	{
		if ( SENTENCEG_GetIndex( soundName ) < 0 )
			Sequence_Warning( "sequences/%s.seq, entry \"%s\": the sound \"%s\" is neither a sentence group of the file nor of sentences.txt\n",
				entry->fileName, entry->entryName, soundName );
		return;
	}

	int mode = hasSpeaker ? 1 : 0;

	for ( int i = 0; i < numPrecached[ mode ]; i++ )
	{
		if ( precached[ mode ][i] == group )
			return;
	}

	if ( numPrecached[ mode ] < (int)ARRAYSIZE( precached[ mode ] ) )
		precached[ mode ][ numPrecached[ mode ]++ ] = group;

	for ( sentenceEntry_s *sentence = group->firstSentence; sentence; sentence = sentence->nextEntry )
	{
		char		sound[SEQUENCE_SOUND_NAME_MAX];
		const char	*why = "";

		if ( !Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), NULL ) )
		{
			Sequence_Warning( "sequences/%s.seq, sentence group \"%s\": no sound file in \"%s\"\n", entry->fileName,
				group->groupName, sentence->data );
			continue;
		}

		if ( !Sequence_IsSimpleSentence( sentence->data ) )
		{
			if ( !hasSpeaker && Sequence_IsSpeakableSentence( sentence->data, &why ) )
			{
				Sequence_PrecacheSpeakFiles( entry, group, sentence->data );
				continue;
			}

			if ( hasSpeaker )
				Sequence_Warning( "sequences/%s.seq, sentence group \"%s\": \"%s\" is more than one sound file; said by a speaker, only the first (%s) is played\n",
					entry->fileName, group->groupName, sentence->data, sound );
			else
				Sequence_Warning( "sequences/%s.seq, sentence group \"%s\": \"%s\" cannot be played as a sentence (%s), only the first sound (%s) is played\n",
					entry->fileName, group->groupName, sentence->data, why, sound );
		}

		if ( Sequence_IsSoundPrecached( sound ) )
			continue;

		if ( s_numPrecachedSounds >= SEQUENCE_MAX_SOUNDS )
		{
			Sequence_Warning( "sequences/%s.seq, sentence group \"%s\": more than %d sound files in the map's sequences, %s will not be played\n",
				entry->fileName, group->groupName, SEQUENCE_MAX_SOUNDS, sound );
			continue;
		}

		// the engine keeps the pointer for the whole map
		s_precachedSounds[ s_numPrecachedSounds ] = ALLOC_STRING( sound );
		PRECACHE_SOUND( (char *)STRING( s_precachedSounds[ s_numPrecachedSounds ] ) );
		s_numPrecachedSounds++;
	}
}

// World precache (CWorld::Precache, after SENTENCEG_Init). Loads sequences/global.seq and sequences/<map>.seq,
// puts the map's file in the list of files the clients download, and precaches the sounds of the entries.
// A map without a sequence file: the game's small global.seq is read and one file is looked for, nothing else.
void Sequence_LevelInit( void )
{
	const char *mapName = STRING( gpGlobals->mapname );

	Sequence_Clear( TRUE );

	Sequence_LoadFile( "global", TRUE );
	s_mapFileState = Sequence_LoadFile( mapName, FALSE );

	// The download table. The clients do not need the file to play the map (the server runs the sequences); it
	// travels with the map so that whoever has the map can host it. global.seq is the game's own: everybody has it.
	if ( s_mapFileState != SEQUENCE_FILE_MISSING )
	{
		char path[128];

		// the engine's limit for a resource's name: 64 characters with the 0 (MAX_QPATH)
		if ( snprintf( path, sizeof( path ), "sequences/%s.seq", mapName ) < 64 )
		{
			// the engine keeps the pointer for the whole map
			PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( path ) ) );

			// the engine does not send a file whose path has more than one dot
			if ( strchr( mapName, '.' ) )
				Sequence_Warning( "sequences/%s.seq: the engine does not send a file with two dots in its name to the clients\n", mapName );
		}
		else
			Sequence_Warning( "sequences/%s.seq: the name is too long for the download table (63 characters at most), the clients will not get the file\n", mapName );
	}

	// What the entries need precached, and what they ask for that SBO does not have
	Sequence_PrecacheSound( NULL, NULL, FALSE );
	s_numPrecachedSounds = 0;
	s_numSpeakFiles = 0;

	for ( sequenceEntry_s *entry = Sequence_GetEntryList(); entry; entry = entry->nextEntry )
	{
		qboolean warnedSentence = FALSE;

		for ( sequenceCommandLine_s *command = entry->firstCommand; command; command = command->nextCommandLine )
		{
			if ( command->soundFileName )
			{
				BOOL hasSpeaker = command->speakerName && strcmp( "none", command->speakerName );

				// once per entry (what is told about a name that is no group is told per entry)
				sequenceCommandLine_s *scan;
				for ( scan = entry->firstCommand; scan != command; scan = scan->nextCommandLine )
				{
					if ( scan->soundFileName && !stricmp( scan->soundFileName, command->soundFileName )
						&& hasSpeaker == ( scan->speakerName && strcmp( "none", scan->speakerName ) ) )
						break;
				}

				if ( scan == command )
					Sequence_PrecacheSound( entry, command->soundFileName, hasSpeaker );
			}

			// CZ: never used by the game either
			if ( command->sentenceName && !warnedSentence )
			{
				Sequence_Warning( "sequences/%s.seq, entry \"%s\": the command \"sentence\" does nothing (use \"sound\")\n",
					entry->fileName, entry->entryName );
				warnedSentence = TRUE;
			}
		}
	}
}


//=========================================================
// trigger_sequence
//=========================================================

class CTriggerSequence : public CBaseDelay
{
public:
	void	KeyValue( KeyValueData *pkvd );
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );

	virtual int		ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	GoToFirstCommand( void );
	void	AdvanceAndCheckForRepeat( sequenceCommandLine_s *command );
	void	SpeakSentence( const char *sentence );
	void	FastForward( void );
	void	StopSequence( void );

	void	ExecuteFireTargets( const char *fireTargetNames, CBaseEntity *pActivator );
	void	ExecuteKillTargets( const char *killTargetNames );
	void	ExecuteTextMessage( client_textmessage_t *message, int textChannel );
	void	ExecuteSound( const char *soundName );
	void	PlayEverywhere( const char *sample );
	void	StopOwnSound( void );
	void	ExecuteSpeech( const char *soundName, const char *speakerName, const char *listenerName, float duration );
	void	ExecuteSoundOrSpeech( const char *soundName, const char *speakerName, const char *listenerName, float duration );
	void	ExecuteSequenceCommand( sequenceCommandLine_s *command );

	const char	*EntryName( void ) { return STRING( m_iszSequenceEntry ); }

	int			m_iszSequenceEntry;		// "sequence_id"
	float		m_flSequenceStartTime;

	// into the sequence manager's lists, which live as long as the map
	sequenceEntry_s			*m_pSequenceEntry;
	sequenceCommandLine_s	*m_pNextCommand;

	EHANDLE		m_hActivator;			// who started the entry: the activator of what it fires
	USE_TYPE	m_useType;
	float		m_flValue;

	BOOL		m_fBusy;
	BOOL		m_fDisabled;			// "fire once", used
	BOOL		m_fFastForward;			// after a restore: find the command the saved game was at

	int			m_iRunSerial;			// changes whenever the entry starts or stops (a target may trigger us)
	BOOL		m_fOwnSound;			// this run played a sound from this entity: what an OFF cuts
	char		m_szOwnSound[SEQUENCE_SOUND_NAME_MAX];	// the last sound it played, to stop it ("" none)
	int			m_rgRepeatsDone[SEQUENCE_MAX_REPEATS];	// per counted "repeat" command of the entry
};

LINK_ENTITY_TO_CLASS( trigger_sequence, CTriggerSequence );

TYPEDESCRIPTION	CTriggerSequence::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerSequence, m_iszSequenceEntry, FIELD_STRING ),
	DEFINE_FIELD( CTriggerSequence, m_flSequenceStartTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerSequence, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerSequence, m_useType, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSequence, m_flValue, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerSequence, m_fBusy, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerSequence, m_fDisabled, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CTriggerSequence, CBaseDelay );

void CTriggerSequence::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "sequence_id" ) )
	{
		m_iszSequenceEntry = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerSequence::Spawn( void )
{
	Precache();

	// Told once, when the map loads: a map with a trigger_sequence and no entry for it
	if ( !m_pSequenceEntry )
	{
		const char *mapName = STRING( gpGlobals->mapname );

		if ( FStringNull( m_iszSequenceEntry ) || !EntryName()[0] )
			Sequence_Warning( "Map %s: trigger_sequence \"%s\" has no sequence name (key \"sequence_id\")\n", mapName,
				STRING( pev->targetname ) );
		else if ( s_mapFileState == SEQUENCE_FILE_MISSING )
			Sequence_Warning( "Map %s: trigger_sequence \"%s\" needs the sequence \"%s\", but the map has no file sequences/%s.seq (and sequences/global.seq has no such entry)\n",
				mapName, STRING( pev->targetname ), EntryName(), mapName );
		else if ( s_mapFileState == SEQUENCE_FILE_BAD )
			Sequence_Warning( "Map %s: trigger_sequence \"%s\" needs the sequence \"%s\", but sequences/%s.seq has an error (see above)\n",
				mapName, STRING( pev->targetname ), EntryName(), mapName );
		else
			Sequence_Warning( "Map %s: trigger_sequence \"%s\" needs the sequence \"%s\", which is in neither sequences/%s.seq nor sequences/global.seq\n",
				mapName, STRING( pev->targetname ), EntryName(), mapName );
	}
}

// czeror-sdk: CTriggerSequence::Spawn (and Restart) ask for the entry. Here in Precache, which a restored game
// runs too: a restored entity has its name, not its pointers.
void CTriggerSequence::Precache( void )
{
	m_pSequenceEntry = SequenceGet( NULL, EntryName() );
	m_pNextCommand = NULL;

	// czeror-sdk: CTriggerSequence::Restart
	m_fFastForward = m_fBusy;
}

// czeror-sdk: CTriggerSequence::Use, where every use type starts the entry. Here OFF stops a running one: no line
// of it runs any more. What it has done stays done (targets fired, entities removed; a text on the screen stays
// for its hold time; a line a speaker or the clients' sentence player has begun is said to its end); the sound
// this entity itself is playing is cut. OFF does not start anything and does not use up "fire once"; a "fire
// once" entry that was started stays used, stopped or not. ON, TOGGLE and SET start the entry, from the beginning
// if it is running.
void CTriggerSequence::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_OFF )
	{
		if ( m_fBusy )
		{
			BOOL ownSound = m_fOwnSound;

			StopSequence();

			if ( ownSound )
				StopOwnSound();
		}
		return;
	}

	if ( !m_pSequenceEntry )
	{
		StopSequence();

		Sequence_Warning( "Map %s: trigger_sequence \"%s\": unknown sequence \"%s\", not firing\n", STRING( gpGlobals->mapname ),
			STRING( pev->targetname ), EntryName() );
		return;
	}

	if ( m_fDisabled )
	{
		ALERT( at_console, "trigger_sequence \"%s\": sequence \"%s\" has fired once, not firing\n", STRING( pev->targetname ),
			EntryName() );
		return;
	}

	if ( pev->spawnflags & SF_SEQUENCE_FIRE_ONCE )
		m_fDisabled = TRUE;

	StopSequence();

	if ( m_pSequenceEntry->firstCommand )
	{
		m_flValue = value;
		m_useType = useType;
		m_hActivator = pActivator;

		m_fBusy = TRUE;

		GoToFirstCommand();

		Think();
	}
	else
	{
		ALERT( at_console, "trigger_sequence \"%s\": sequence \"%s\" (sequences/%s.seq) is empty\n", STRING( pev->targetname ),
			EntryName(), m_pSequenceEntry->fileName );
	}
}

// czeror-sdk: CTriggerSequence::Think. Runs the commands that are due: each one, then its delay.
void CTriggerSequence::Think( void )
{
	if ( m_fFastForward )
	{
		m_fFastForward = FALSE;

		if ( m_fBusy && m_pSequenceEntry )
			FastForward();
	}

	float	timeNext = gpGlobals->time;
	int		serial = m_iRunSerial;
	int		commands = 0;

	while ( m_pNextCommand )
	{
		timeNext += m_pNextCommand->delay;

		ExecuteSequenceCommand( m_pNextCommand );

		// a target triggered this entity (it started over and has thought), or removed it
		if ( serial != m_iRunSerial )
			return;

		if ( pev->flags & FL_KILLME )
		{
			StopSequence();
			return;
		}

		if ( gpGlobals->time < timeNext )
			break;

		if ( ++commands >= SEQUENCE_MAX_COMMANDS_PER_THINK )
		{
			Sequence_Warning( "Map %s: trigger_sequence \"%s\": sequence \"%s\" ran %d commands without a pause, stopped\n",
				STRING( gpGlobals->mapname ), STRING( pev->targetname ), EntryName(), commands );
			StopSequence();
			return;
		}
	}

	if ( m_pNextCommand )
		pev->nextthink = timeNext;
	else
		StopSequence();
}

void CTriggerSequence::GoToFirstCommand( void )
{
	m_flSequenceStartTime = gpGlobals->time;
	m_pNextCommand = m_pSequenceEntry->firstCommand;
}

// czeror-sdk: CTriggerSequence::AdvanceAndCheckForRepeat; the rule is Sequence_NextCommand's (sequence.cpp):
// "repeat = -1" starts the entry over every time, "repeat = N" N times.
void CTriggerSequence::AdvanceAndCheckForRepeat( sequenceCommandLine_s *command )
{
	m_pNextCommand = Sequence_NextCommand( m_pSequenceEntry, command, m_rgRepeatsDone );
}

// czeror-sdk: CTriggerSequence::FastForward. The command that is due at this time, without running the ones before
// it (a restored game: they ran before it was saved). An entry that repeats is taken as in its first pass.
void CTriggerSequence::FastForward( void )
{
	m_pNextCommand = m_pSequenceEntry->firstCommand;

	for ( float timeNext = m_flSequenceStartTime; ( timeNext + 0.0001 ) < gpGlobals->time && m_pNextCommand; )
	{
		timeNext += m_pNextCommand->delay;

		m_pNextCommand = m_pNextCommand->nextCommandLine;
	}
}

void CTriggerSequence::StopSequence( void )
{
	m_pNextCommand = NULL;
	m_hActivator = NULL;
	m_flSequenceStartTime = 0;
	m_useType = USE_OFF;
	m_flValue = 0;
	m_fBusy = FALSE;

	m_iRunSerial++;
	m_fOwnSound = FALSE;
	memset( m_rgRepeatsDone, 0, sizeof( m_rgRepeatsDone ) );

	pev->nextthink = 0;
}

// "firetargets": the targets get the entry's activator, use type and value, with this entity as the caller
void CTriggerSequence::ExecuteFireTargets( const char *fireTargetNames, CBaseEntity *pActivator )
{
	if ( fireTargetNames )
		FireTargets( fireTargetNames, pActivator, this, m_useType, m_flValue );
}

// "killtargets" (czeror-sdk also calls CZ's PreKillTarget, which SBO's entities do not have)
void CTriggerSequence::ExecuteKillTargets( const char *killTargetNames )
{
	if ( !killTargetNames )
		return;

	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, killTargetNames ) ) != NULL )
		UTIL_Remove( pEntity );
}

// "text": a HUD message for every player, what game_text shows with "All Players"
void CTriggerSequence::ExecuteTextMessage( client_textmessage_t *message, int textChannel )
{
	if ( !message || !message->pMessage )
		return;

	hudtextparms_t hudTextParms;

	hudTextParms.holdTime = message->holdtime;
	hudTextParms.x = message->x;
	hudTextParms.y = message->y;
	hudTextParms.effect = message->effect;
	hudTextParms.r1 = message->r1;
	hudTextParms.g1 = message->g1;
	hudTextParms.b1 = message->b1;
	hudTextParms.a1 = message->a1;
	hudTextParms.r2 = message->r2;
	hudTextParms.g2 = message->g2;
	hudTextParms.b2 = message->b2;
	hudTextParms.a2 = message->a2;
	hudTextParms.fadeinTime = message->fadein;
	hudTextParms.fadeoutTime = message->fadeout;
	hudTextParms.fxTime = message->fxtime;
	hudTextParms.channel = textChannel;

	// "holdtime = -1": by the length of the text
	if ( message->holdtime == -1 )
		hudTextParms.holdTime = ( strlen( message->pMessage ) * 0.05 ) + 1.0;

	UTIL_HudMessageAll( hudTextParms, message->pMessage );
}

// A sentence of the sequence file that is more than one sound file, without a speaker: every client plays it with
// the engine's own sentence player. The engine can only be told to play a sentence by its name or number in
// sentences.txt; what takes a sentence's words is the client's console command "spk" (Valve's Ricochet,
// CBasePlayer::ClientHearVox: "Force a client to hear a specific VOX sentence. This should eventually be moved into
// the engine"). Ricochet has the server push that command into the client; SBO sends the sentence in a message of
// its own and its client library runs "spk" locally (cl_dll/hud.cpp, MsgFunc_SeqSpeak), so nothing depends on the
// client accepting commands from a server.
//
// Reliable, to everyone connected (the engine leaves bots out): a voice line is sent once and must not be lost,
// and it is at most SEQUENCE_SPEAK_MAX + 1 bytes. Heard at full volume wherever the listener is; nothing can stop
// it once it is sent.
void CTriggerSequence::SpeakSentence( const char *sentence )
{
	// the client checks it again
	if ( !gmsgSeqSpeak || !Sequence_IsSpeakableSentence( sentence, NULL ) )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgSeqSpeak );
		WRITE_STRING( sentence );
	MESSAGE_END();
}

// A sound without a speaker, played the way ambient_generic plays with "Play Everywhere" (sound.cpp): an ambient
// sound at ATTN_NONE, which the engine sends to every client. A sound of the entity's own (EMIT_SOUND) only reaches
// the clients in the PAS of the entity's position. The line before it, if it still plays, stops first: one line
// at a time, as on one sound channel.
void CTriggerSequence::PlayEverywhere( const char *sample )
{
	StopOwnSound();

	UTIL_EmitAmbientSound( edict(), pev->origin, sample, VOL_NORM, ATTN_NONE, 0, PITCH_NORM );

	strncpy( m_szOwnSound, sample, sizeof( m_szOwnSound ) - 1 );
	m_szOwnSound[ sizeof( m_szOwnSound ) - 1 ] = 0;
	m_fOwnSound = TRUE;
}

// ambient_generic's stop (sound.cpp): the same sample with SND_STOP
void CTriggerSequence::StopOwnSound( void )
{
	if ( !m_szOwnSound[0] )
		return;

	UTIL_EmitAmbientSound( edict(), pev->origin, m_szOwnSound, 0, 0, SND_STOP, 0 );
	m_szOwnSound[0] = 0;
}

// "sound" without a speaker: heard by everyone, everywhere, at full volume
void CTriggerSequence::ExecuteSound( const char *soundName )
{
	char sound[SEQUENCE_SOUND_NAME_MAX];

	if ( !soundName )
		return;

	// a sentence group of the sequence file: a random sentence of it
	sentenceEntry_s *sentence = SequencePickSentence( soundName, 0, NULL );

	if ( sentence )
	{
		if ( !Sequence_IsSimpleSentence( sentence->data ) && Sequence_IsSpeakableSentence( sentence->data, NULL ) )
		{
			SpeakSentence( sentence->data );
		}
		// one sound file (or the first of a sentence that cannot be spoken); not precached: over the map's share
		// of sounds, told when the map loaded
		else if ( Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), NULL ) && Sequence_IsSoundPrecached( sound ) )
		{
			PlayEverywhere( sound );
		}
	}
	else if ( SENTENCEG_GetIndex( soundName ) >= 0 )
	{
		// a random sentence of the group, as SENTENCEG_PlayRndSz picks it ("!HG_GREN3")
		if ( USENTENCEG_Pick( SENTENCEG_GetIndex( soundName ), sound ) >= 0 && sound[0] )
			PlayEverywhere( sound );
	}
}

// "sound" with a speaker: the monster of that name says it, to the listener
void CTriggerSequence::ExecuteSpeech( const char *soundName, const char *speakerName, const char *listenerName, float duration )
{
	char sound[SEQUENCE_SOUND_NAME_MAX];

	if ( !speakerName || !soundName )
		return;

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, speakerName );
	if ( !pTarget )
		return;

	CBaseMonster *pMonsterTarget = pTarget->MyMonsterPointer();
	if ( !pMonsterTarget )
		return;

	// a sentence group of the sequence file: a random sentence of it, its (first) sound file from the monster's
	// mouth. It is not a sentence the monster's PlayScriptedSentence could play.
	sentenceEntry_s *sentence = SequencePickSentence( soundName, 0, NULL );

	if ( sentence )
	{
		if ( Sequence_SentenceSound( sentence->data, sound, sizeof( sound ), NULL ) && Sequence_IsSoundPrecached( sound ) )
			EMIT_SOUND_DYN( pMonsterTarget->edict(), CHAN_VOICE, sound, VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	CBaseEntity *pListener = listenerName ? UTIL_FindEntityGeneric( listenerName, pev->origin, 4096 ) : NULL;

	pMonsterTarget->PlayScriptedSentence( soundName, duration, VOL_NORM, ATTN_NORM, TRUE, pListener );
}

void CTriggerSequence::ExecuteSoundOrSpeech( const char *soundName, const char *speakerName, const char *listenerName, float duration )
{
	if ( !soundName )
		return;

	if ( speakerName && strcmp( "none", speakerName ) )
		ExecuteSpeech( soundName, speakerName, listenerName, duration );
	else
		ExecuteSound( soundName );
}

// czeror-sdk: CTriggerSequence::ExecuteSequenceCommand. A command does everything it has: targets, text, sound.
// The next command is chosen first (czeror-sdk: last): a target may trigger this entity, which starts it over.
void CTriggerSequence::ExecuteSequenceCommand( sequenceCommandLine_s *command )
{
	if ( !command )
		return;

	int			serial = m_iRunSerial;
	CBaseEntity	*pActivator = m_hActivator;

	AdvanceAndCheckForRepeat( command );

	ExecuteFireTargets( command->fireTargetNames, pActivator );

	if ( serial != m_iRunSerial )
		return;

	ExecuteKillTargets( command->killTargetNames );
	ExecuteTextMessage( &command->clientMessage, command->textChannel );

	ExecuteSoundOrSpeech( command->soundFileName, command->speakerName, command->listenerName, command->delay );
}
