//========= SBO world screens: movies (docs/WORLD_SCREENS.md, "Movie screens") =========//
//
// A vgui_screen with "moviefilename" shows an Ogg Theora video from video/ on its rectangle or bound faces instead
// of a VGUI2 panel, for everyone who sees it, with its sound from the screen. The server sends the movie with the
// screen's DEFINE (name, flags, seconds since it started, volume, radius) and the start / stop as STATE; the clock
// is the client's time from that start, so every player (and one who joins later) sees the same frame.
//
// Per slot: the fullscreen player's decoder (video/video_decoder.cpp) on the file (loaded once per name, shared by
// the screens showing it). The picture is decoded only while a 3D view sees the screen (worldscreen.cpp calls
// UpdatePicture from WorldScreen_DrawWorld); looked at again later, it seeks to the clock's position (keyframe
// index). The sound is decoded while the player is within the screen's radius and mixed by this file into one SDL2
// device of its own (video/video_audio.cpp): volume, linear falloff to the radius and left/right from the view.
//
// Plain C++ only (SBO world).
//==========================================================================================//

#ifndef SBO_WORLDSCREEN_VIDEO_H
#define SBO_WORLDSCREEN_VIDEO_H

namespace wsvideo
{
//! The slot shows movie (a name in video/). elapsed: seconds since it started (DEFINE). flags: WSCREEN_FLAG_*.
//! volume 0..10, radius in units. wide / tall: the picture size. False when the file is missing or no video (the
//! screen is then not shown; the console says why).
bool Define(int slot, const char *movie, float elapsed, int flags, int volume, int radius, int &wide, int &tall);
//! STATE: the screen's flags changed; becoming active starts the movie again from the beginning.
void SetFlags(int slot, int flags);
//! The slot is no movie screen any more (removed, new map).
void Remove(int slot);
void RemoveAll();
//! The slot shows a movie.
bool IsMovie(int slot);

//! WorldScreen_DrawWorld, a 3D view sees the screen (main view): decodes up to the clock (once a frame) and uploads
//! the picture to the slot's texture. Also uploads the current picture when the texture lost it (video mode change).
void UpdatePicture(int slot);
//! WorldScreen_Frame, once a frame: the sound of every movie screen, from where it is (center).
void Frame(const float centers[][3], const bool active[], int count);

//! worldscreen_status: one line per movie screen.
void PrintStatus();
} // namespace wsvideo

#endif // SBO_WORLDSCREEN_VIDEO_H
