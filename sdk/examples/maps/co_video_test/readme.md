# Example map: co_video_test

`co_video_test.map` is the source of `co_video_test`, a one-room test map for videos: `game_playvideo` (a video over
the whole screen; SBO docs: Video player) and movie screens (`vgui_screen` with `moviefilename`; SBO docs: World
screens, "Movie screens"). Every station plays the SDK's sample video.

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_video_test.map` | your editor (Valve 220 format) | the map source |
| `../../video/sbo_sample.ogv` | `syndicate/video/` | the sample video: 6 seconds of test picture with beeps (480 x 270, 24 fps, 450 KB). The game ships no videos: copy it before you play the map |

## The room

North wall, one button per case (a label over each button):

| Button | What happens |
|---|---|
| 1 | `game_playvideo`, the activator only: the sample with its own sound |
| 2 | `game_playvideo`, All Players: everybody sees it |
| 3 | a missing file (`missing.ogv`): nothing is drawn, the console says the file is not found, the player can move |
| 4 | the sample, then a `trigger_hurt` kills the player after 2 seconds: the video stops at once |
| 5 | a message box with the checks |
| 6 | the big movie screen off and on again: it starts over |
| 7 | the big movie screen on / off |
| 8 | four more movie screens (silent) on / off: the cost of five screens |

South wall: the big movie screen plays the sample on loop from the map start, with its sound coming from the screen
(full near it, silent at 1250 units). A player who joins later sees the same moment as everybody else.

While a full-screen video plays the player cannot move, fire or switch weapons; Esc stops it. Every start and stop
prints a console line beginning with `Video:`.
