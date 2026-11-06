//========= SBO: update checker (Version tab) =========//
//
// Purpose: what the Advanced options' Version tab shows and its "Check for updates" button does: a view of the
//          updater (updater_client.h, docs/UPDATER.md). Implemented in vgui2/sbo_update_window.cpp, which owns the
//          updater.
//=====================================================//

#ifndef SBO_UPDATE_CHECKER_H
#define SBO_UPDATE_CHECKER_H

#include <string>

#include "updater_client.h"

namespace sbo_update
{
//! False when this build has no release repository (CMake SBO_UPDATE_REPO empty)
bool IsEnabled();
//! The player asked: checks, and the update dialog opens if there is an update (also a skipped one)
void CheckForUpdates();
//! Nothing to do any more (the updater runs from HUD_Frame); kept for the Version tab's OnThink
inline void Frame() {}
State GetState();
//! Newest release ("2.0.1"), "" if unknown
const char *GetLatestVersion();
const char *GetError();
//! One line for the Version tab: "Latest version: 2.0.1 (update available)", "checking...", the error, ...
std::string StatusText();
//! The updater, nullptr before the client's Initialize
Updater *Instance();
} // namespace sbo_update

#endif // SBO_UPDATE_CHECKER_H
