//=========================================================
// Sequence files (docs/SEQUENCES.md, "Sounds"): a sentence of several words is played by each client's own
// sentence player. The server sends the sentence in the message "SeqSpeak" (one string) and the client library
// hands it to the engine's console command "spk", the only thing in the engine that takes a sentence's words.
//
// The sentence becomes part of a console command on the client, so both sides check it with the same rule, and
// the client never trusts the server's: nothing but what a sentence is made of, and not too long.
//=========================================================

#ifndef SEQUENCE_SPEAK_H
#define SEQUENCE_SPEAK_H

// The longest sentence (the message's string; a user message holds 192 bytes)
#define SEQUENCE_SPEAK_MAX		120

// Letters, digits, blanks (space, tab) and / _ - . , ( ) !  No quote, no semicolon, no line end, nothing a console
// would read as anything but text.
inline int Sequence_IsSpeakChar( char ch )
{
	if ( ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ( ch >= '0' && ch <= '9' ) )
		return 1;

	switch ( ch )
	{
	case ' ':
	case '\t':
	case '/':
	case '_':
	case '-':
	case '.':
	case ',':
	case '(':
	case ')':
	case '!':
		return 1;
	}

	return 0;
}

// Whether a string may go into  spk "<sentence>"
inline int Sequence_IsSpeakSafe( const char *sentence )
{
	int length = 0;

	if ( !sentence || !sentence[0] )
		return 0;

	for ( ; sentence[ length ]; length++ )
	{
		if ( length >= SEQUENCE_SPEAK_MAX || !Sequence_IsSpeakChar( sentence[ length ] ) )
			return 0;
	}

	return 1;
}

#endif // SEQUENCE_SPEAK_H
