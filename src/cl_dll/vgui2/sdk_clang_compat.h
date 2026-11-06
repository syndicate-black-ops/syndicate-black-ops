//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: Force-included into every hl1_source_sdk translation unit (tier1,
//          tier2, vgui_controls, sbo_vgui2_sdk) when building with clang-cl.
//
// tier0/platform.h defines RESTRICT as __restrict for _MSC_VER. MSVC accepts a
// member function declared without and defined with __restrict (bitbuf.h /
// bitbuf.cpp: bf_write::WriteUBitLong, bf_read::ReadUBitLongNoInline); clang-cl
// rejects it as "conflicting types". __restrict is only an optimization hint, so
// it is dropped: platform.h is included first (include guard), then RESTRICT is
// redefined empty.
//=================================================================//

#ifndef SBO_VGUI2_SDK_CLANG_COMPAT_H
#define SBO_VGUI2_SDK_CLANG_COMPAT_H

#if defined(__clang__)
#include <tier0/platform.h>
#undef RESTRICT
#define RESTRICT
#endif

#endif
