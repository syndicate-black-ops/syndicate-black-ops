//========= SBO world screens: movies (worldscreen_video.h, docs/WORLD_SCREENS.md "Movie screens") =========//
//
// Clock: the client's time (GetClientTime, which follows the server's) minus the seconds the server says the movie
// has played (DEFINE), or the time the screen became active (STATE). Looping movies wrap at their length (the last
// Theora page's frame number); a movie that does not loop keeps its last frame and goes silent at its end.
//
// Picture: decoded only for screens a 3D view sees, at most once a frame per screen. When the clock is more than
// SEEK_GAP ahead of the decoded frame (the screen was not looked at, a late joiner, a loop wrap), the decoder seeks
// to the keyframe before the clock and decodes from there (CVideoDecoder::SeekTo).
//
// Sound: one SDL2 device for every movie screen (video/video_audio.cpp: VideoAudio_OpenDevice), 44100 Hz stereo,
// kept MIX_AHEAD seconds ahead. Each audible screen adds its Vorbis sound (the video's track or its sound file),
// resampled to 44100 (nearest), with a gain: volume (0..10, "health" as ambient_generic's) x linear falloff to
// zero at the radius x the engine's "volume"; left / right from the direction to the screen against the view's right
// vector. Flag WSCREEN_FLAG_SOUND2D: the same gain everywhere, no falloff or panning; WSCREEN_FLAG_NOSOUND: none.
// A screen's sound follows the clock: out of reach it is not decoded, and when it is more than AUDIO_SLACK away
// from where the clock says (coming into reach, a seek), stb_vorbis seeks to it.
//
// SBO world: compiled with SBO's client include paths + sbo_compat.h.
//=====================================================================================================//

#include <vector> // before cl_util.h (min / max macros)
#include <string>
#include "video/video_decoder.h"

#include "hud.h"
#include "cl_util.h"

#include <math.h>
#include <string.h>

#include "video/video_audio.h"
#include "video_shared.h"	// game_shared
#include "vgui_screen_shared.h"
#include "worldscreen.h"
#include "worldscreen_gl.h"
#include "worldscreen_video.h"

extern vec3_t v_origin, v_angles; // view.cpp: last view origin / aim angles

namespace
{
const double SEEK_GAP = 0.5;      // seconds the clock may run ahead of the picture before a seek
const double AUDIO_SLACK = 0.15;  // seconds the sound may be off the clock before it seeks
const int MIX_RATE = 44100;
const double MIX_AHEAD = 0.1;     // seconds of mixed sound kept queued
const int MIX_MAX = 4096;         // frames mixed in one go
const int DECODE_PER_FRAME = 6;   // video frames one screen decodes per client frame (catching up after a seek)

// a loaded movie file, shared by the screens showing it
struct File
{
	std::string name;
	byte *data = nullptr;
	int length = 0;
	int users = 0;
};
std::vector<File> s_Files;

struct Movie
{
	bool used = false;
	char name[VIDEO_MAX_PATH] = "";
	int file = -1;                 // s_Files index
	CVideoDecoder *dec = nullptr;
	double start = 0;              // client time the movie started
	int flags = 0;
	int volume = 10;
	int radius = 1250;
	double duration = 0;
	double pictureTime = -1;       // clock position of the decoded picture (-1: none yet)
	bool changed = false;          // a new picture not uploaded yet
	int lastFrame = -1;            // the frame counter it was decoded in
	bool audioSynced = false;      // the sound follows the clock (false: seek before mixing it again)
	int decodes = 0, seeks = 0, audioSeeks = 0; // status
};
Movie s_Movies[WORLDSCREEN_MAX];

int s_iFrame = 0;                  // WorldScreen_Frame counter (one picture update per screen per frame)
unsigned int s_iDevice = 0;        // the mixer's SDL2 device
bool s_bWarnedNoDevice = false;

int LoadFile(const char *path)
{
	for (size_t i = 0; i < s_Files.size(); i++)
	{
		if (s_Files[i].users > 0 && s_Files[i].name == path)
		{
			s_Files[i].users++;
			return (int)i;
		}
	}
	int length = 0;
	byte *data = gEngfuncs.COM_LoadFile((char *)path, 5, &length);
	if (!data)
		return -1;
	if (length <= 0 || length > CVideoDecoder::MAX_FILE_BYTES)
	{
		gEngfuncs.COM_FreeFile(data);
		return -1;
	}
	size_t i = 0;
	while (i < s_Files.size() && s_Files[i].users > 0)
		i++;
	if (i == s_Files.size())
		s_Files.push_back(File());
	s_Files[i].name = path;
	s_Files[i].data = data;
	s_Files[i].length = length;
	s_Files[i].users = 1;
	return (int)i;
}

void ReleaseFile(int i)
{
	if (i < 0 || i >= (int)s_Files.size() || s_Files[i].users <= 0)
		return;
	if (--s_Files[i].users == 0)
	{
		gEngfuncs.COM_FreeFile(s_Files[i].data);
		s_Files[i] = File();
	}
}

bool Active(const Movie &m)
{
	return m.used && m.dec && (m.flags & WSCREEN_FLAG_ACTIVE);
}

// The clock's position in the movie; ended: a movie that does not loop has played to its end
double Position(const Movie &m, bool &ended)
{
	ended = false;
	double t = gEngfuncs.GetClientTime() - m.start;
	if (t < 0)
		t = 0;
	if (m.duration > 0)
	{
		if (m.flags & WSCREEN_FLAG_LOOP)
			t = fmod(t, m.duration);
		else if (t >= m.duration)
		{
			ended = true;
			t = m.duration;
		}
	}
	return t;
}

float VolumeCvar()
{
	float v = gEngfuncs.pfnGetCvarFloat("volume");
	return v < 0 ? 0 : (v > 1 ? 1 : v);
}
} // namespace

namespace wsvideo
{
bool Define(int slot, const char *movie, float elapsed, int flags, int volume, int radius, int &wide, int &tall)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX)
		return false;
	Movie &m = s_Movies[slot];

	char path[VIDEO_MAX_PATH];
	if (!VideoShared_MakePath(movie, true, path, sizeof(path)))
	{
		worldscreen::Printf("worldscreen: screen %d: movie name \"%s\" refused: %s\n", slot, movie ? movie : "",
			VideoShared_NameError(movie, true));
		Remove(slot);
		return false;
	}

	// a DEFINE again for the same movie (fullupdate): keep the decoder, take the new clock and settings
	if (!(m.used && m.dec && !strcmp(m.name, path)))
	{
		Remove(slot);
		const int file = LoadFile(path);
		if (file < 0)
		{
			worldscreen::Printf("worldscreen: screen %d: %s not found\n", slot, path);
			return false;
		}
		CVideoDecoder *dec = new CVideoDecoder;
		dec->SetBottomUp(true); // OpenGL's row order: one upload (wsgl::UploadPicture)
		if (!dec->Open(s_Files[file].data, (size_t)s_Files[file].length))
		{
			worldscreen::Printf("worldscreen: screen %d: %s: %s\n", slot, path, dec->Error() ? dec->Error() : "cannot be read");
			delete dec;
			ReleaseFile(file);
			return false;
		}
		m.used = true;
		strcpy(m.name, path);
		m.file = file;
		m.dec = dec;
		m.duration = dec->Duration();
		m.pictureTime = -1;
		m.changed = false;
		m.decodes = m.seeks = m.audioSeeks = 0;
	}
	m.flags = flags;
	m.volume = volume < 0 ? 0 : (volume > 10 ? 10 : volume);
	m.radius = radius < 1 ? 1 : radius;
	m.start = gEngfuncs.GetClientTime() - (elapsed > 0 ? elapsed : 0);
	m.audioSynced = false;
	wide = m.dec->Width();
	tall = m.dec->Height();
	worldscreen::Printf("worldscreen: screen %d plays %s (%dx%d, %.2f fps, %.1f s, %s%s), at %.1f s\n", slot, path, wide,
	    tall, m.dec->FrameRate(), m.duration, (flags & WSCREEN_FLAG_LOOP) ? "loop, " : "",
	    (flags & WSCREEN_FLAG_NOSOUND) || !m.dec->AudioRate() ? "no sound" : ((flags & WSCREEN_FLAG_SOUND2D) ? "2D sound" : "3D sound"),
	    elapsed);
	return true;
}

void SetFlags(int slot, int flags)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX || !s_Movies[slot].used)
		return;
	Movie &m = s_Movies[slot];
	const bool wasActive = (m.flags & WSCREEN_FLAG_ACTIVE) != 0;
	m.flags = flags;
	if (!wasActive && (flags & WSCREEN_FLAG_ACTIVE))
	{
		m.start = gEngfuncs.GetClientTime(); // started (again): from the beginning
		m.pictureTime = -1;
		m.audioSynced = false;
		worldscreen::Printf("worldscreen: screen %d starts %s\n", slot, m.name);
	}
}

void Remove(int slot)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX)
		return;
	Movie &m = s_Movies[slot];
	delete m.dec;
	ReleaseFile(m.file);
	m = Movie();
}

void RemoveAll()
{
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
		Remove(i);
	if (s_iDevice)
	{
		VideoAudio_CloseDevice(s_iDevice);
		s_iDevice = 0;
	}
}

bool IsMovie(int slot)
{
	return slot >= 0 && slot < WORLDSCREEN_MAX && s_Movies[slot].used;
}

void UpdatePicture(int slot)
{
	if (slot < 0 || slot >= WORLDSCREEN_MAX)
		return;
	Movie &m = s_Movies[slot];
	if (!Active(m))
		return;

	if (m.lastFrame != s_iFrame)
	{
		m.lastFrame = s_iFrame;
		bool ended;
		const double t = Position(m, ended);
		const double target = ended ? t - 0.5 / m.dec->FrameRate() : t;
		// behind by more than SEEK_GAP, or the clock went back (loop wrap, restart): from the keyframe before it
		if (m.pictureTime < 0 || target < m.pictureTime || target - m.pictureTime > SEEK_GAP || m.dec->IsFinished())
		{
			m.dec->SeekTo(target);
			m.seeks++;
		}
		if (m.dec->DecodeToTime(target, DECODE_PER_FRAME))
		{
			m.changed = true;
			m.decodes++;
		}
		m.pictureTime = target;
	}

	if ((m.changed || !wsgl::HasContent(slot)) && m.dec->RGBA() && m.decodes > 0)
	{
		if (wsgl::UploadPicture(slot, m.dec->RGBA(), m.dec->Width(), m.dec->Height()))
			m.changed = false;
	}
}

void Frame(const float centers[][3], const bool active[], int count)
{
	s_iFrame++;

	// the screens that can be heard now
	float forward[3], right[3], up[3];
	gEngfuncs.pfnAngleVectors(v_angles, forward, right, up);
	const float master = VolumeCvar();
	float gainL[WORLDSCREEN_MAX] = {}, gainR[WORLDSCREEN_MAX] = {};
	bool any = false;
	for (int i = 0; i < count && i < WORLDSCREEN_MAX; i++)
	{
		Movie &m = s_Movies[i];
		if (!active[i] || !Active(m) || (m.flags & WSCREEN_FLAG_NOSOUND) || !m.dec->AudioRate())
		{
			m.audioSynced = false;
			continue;
		}
		bool ended;
		Position(m, ended);
		if (ended)
			continue;
		float g = m.volume / 10.0f * master;
		float l = g, r = g;
		if (!(m.flags & WSCREEN_FLAG_SOUND2D))
		{
			float d[3] = { centers[i][0] - v_origin[0], centers[i][1] - v_origin[1], centers[i][2] - v_origin[2] };
			const float dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
			const float fall = 1.0f - dist / (float)m.radius;
			if (fall <= 0)
			{
				m.audioSynced = false; // out of reach: not decoded, seeks when it comes back
				continue;
			}
			g *= fall;
			// left / right: the side of the view it is on (at most 60 % quieter on the far ear)
			const float pan = dist > 1.0f ? (d[0] * right[0] + d[1] * right[1] + d[2] * right[2]) / dist : 0.0f;
			l = g * (pan > 0 ? 1.0f - 0.6f * pan : 1.0f);
			r = g * (pan < 0 ? 1.0f + 0.6f * pan : 1.0f);
		}
		gainL[i] = l;
		gainR[i] = r;
		any = true;
	}

	if (!any)
		return;
	if (!s_iDevice)
	{
		s_iDevice = VideoAudio_OpenDevice(MIX_RATE);
		if (!s_iDevice)
		{
			if (!s_bWarnedNoDevice)
				worldscreen::Printf("worldscreen: no sound device for the movie screens: %s\n", VideoAudio_Error());
			s_bWarnedNoDevice = true;
			return;
		}
	}

	const int queued = VideoAudio_DeviceQueued(s_iDevice);
	int n = (int)(MIX_RATE * MIX_AHEAD) - queued;
	if (n < 256)
		return;
	if (n > MIX_MAX)
		n = MIX_MAX;
	const double latency = (double)queued / MIX_RATE; // what is queued plays before this block

	static int mix[MIX_MAX * 2];
	memset(mix, 0, sizeof(int) * n * 2);
	static short in[(MIX_MAX * 4 + 16) * 2];
	for (int i = 0; i < count && i < WORLDSCREEN_MAX; i++)
	{
		if (gainL[i] <= 0 && gainR[i] <= 0)
			continue;
		Movie &m = s_Movies[i];
		const int rate = m.dec->AudioRate();
		bool ended;
		double t = Position(m, ended) + latency;
		if (m.duration > 0 && (m.flags & WSCREEN_FLAG_LOOP))
			t = fmod(t, m.duration);
		if (!m.audioSynced || fabs(m.dec->AudioPosition() - t) > AUDIO_SLACK)
		{
			m.dec->SeekAudio(t);
			m.audioSynced = true;
			m.audioSeeks++;
		}
		// the source frames for n output frames (nearest resampling)
		int need = (int)((double)n * rate / MIX_RATE + 0.5);
		if (need < 1)
			need = 1;
		if (need > MIX_MAX * 4)
			need = MIX_MAX * 4;
		int got = 0;
		while (got < need)
		{
			const int r = m.dec->ReadAudio(in + got * 2, need - got);
			if (r > 0)
			{
				got += r;
				continue;
			}
			if (!(m.flags & WSCREEN_FLAG_LOOP) || !m.dec->SeekAudio(0))
				break; // the end: the rest is silence
		}
		const int gl = (int)(gainL[i] * 256), gr = (int)(gainR[i] * 256);
		for (int k = 0; k < n; k++)
		{
			const int src = (int)((long long)k * need / n);
			if (src >= got)
				break;
			mix[k * 2] += (in[src * 2] * gl) >> 8;
			mix[k * 2 + 1] += (in[src * 2 + 1] * gr) >> 8;
		}
	}

	static short out[MIX_MAX * 2];
	for (int k = 0; k < n * 2; k++)
		out[k] = (short)(mix[k] > 32767 ? 32767 : (mix[k] < -32768 ? -32768 : mix[k]));
	VideoAudio_DeviceQueue(s_iDevice, out, n);
}

void PrintStatus()
{
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		const Movie &m = s_Movies[i];
		if (!m.used)
			continue;
		bool ended;
		const double t = Position(m, ended);
		worldscreen::Printf("  movie %d: %s %s at %.2f / %.2f s%s, picture %.2f s, %d pictures, %d seeks, sound %d seeks%s\n",
		    i, m.name, Active(m) ? "playing" : "stopped", t, m.duration, ended ? " (ended)" : "", m.pictureTime,
		    m.decodes, m.seeks, m.audioSeeks, s_iDevice ? "" : " (no device open)");
	}
}
} // namespace wsvideo
