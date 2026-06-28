//-----------------------------------------------------------------------------
// COMPILE-TIME OPTIONS FOR DEAR IMGUI
// Runtime options (clipboard callbacks, enabling various features, etc.) can generally be set via the ImGuiIO structure.
// You can use ImGui::SetAllocatorFunctions() before calling ImGui::CreateContext() to rewire memory allocation functions.
//-----------------------------------------------------------------------------
// A) You may edit imconfig.h (and not overwrite it when updating Dear ImGui, or maintain a patch/rebased branch with your modifications to it)
// B) or '#define IMGUI_USER_CONFIG "my_imgui_config.h"' in your project and then add directives in your own file without touching this template.
//-----------------------------------------------------------------------------
// You need to make sure that configuration settings are defined consistently _everywhere_ Dear ImGui is used, which include the imgui*.cpp
// files but also _any_ of your code that uses Dear ImGui. This is because some compile-time options have an affect on data structures.
// Defining those options in imconfig.h will ensure every compilation unit gets to see the same data structure layouts.
// Call IMGUI_CHECKVERSION() from your .cpp files to verify that the data structures your files are using are matching the ones imgui.cpp is using.
//-----------------------------------------------------------------------------

#pragma once

// PS3 FIX: WHBLogPrintf als printf definiert (ohne pch.h, um zirkuläre Includes zu vermeiden)
#ifndef WHBLogPrintf
#include <cstdio>
#define WHBLogPrintf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#define IM_COL32_R_SHIFT    24
#define IM_COL32_G_SHIFT    16
#define IM_COL32_B_SHIFT    8
#define IM_COL32_A_SHIFT    0
#define IM_COL32_A_MASK     0x000000FF

//---- Define assertion handler. Defaults to calling assert().
// PS3 FIX: Nutzt WHBLogPrintf (definiert als printf)
#define IM_ASSERT(_EXPR)  do { if (!(_EXPR)) { WHBLogPrintf("IMGUI ASSERTION FAILED: %s\n", #_EXPR); } } while (0)
//#define IM_ASSERT(_EXPR)  ((void)(_EXPR))     // Disable asserts

//---- Define attributes of all API symbols declarations, e.g. for DLL under Windows
//#define IMGUI_API __declspec( dllexport )
//#define IMGUI_API __declspec( dllimport )

//---- Don't define obsolete functions/enums/behaviors.
//#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
//#define IMGUI_DISABLE_OBSOLETE_KEYIO

//---- Disable all of Dear ImGui or don't implement standard windows/tools.
//#define IMGUI_DISABLE
//#define IMGUI_DISABLE_DEMO_WINDOWS
//#define IMGUI_DISABLE_DEBUG_TOOLS

//---- Don't implement some functions to reduce linkage requirements.
//#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
//#define IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
//#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
//#define IMGUI_DISABLE_WIN32_FUNCTIONS
//#define IMGUI_ENABLE_OSX_DEFAULT_CLIPBOARD_FUNCTIONS
//#define IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
//#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
//#define IMGUI_DISABLE_FILE_FUNCTIONS
//#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
//#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
//#define IMGUI_DISABLE_SSE

//---- Include imgui_user.h at the end of imgui.h as a convenience
//#define IMGUI_INCLUDE_IMGUI_USER_H

//---- Pack colors to BGRA8 instead of RGBA8 (to avoid converting from one to another)
//#define IMGUI_USE_BGRA_PACKED_COLOR

//---- Use 32-bit for ImWchar (default is 16-bit) to support unicode planes 1-16.
//#define IMGUI_USE_WCHAR32

//---- Avoid multiple STB libraries implementations, or redefine path/filenames to prioritize another version
//#define IMGUI_STB_TRUETYPE_FILENAME   "my_folder/stb_truetype.h"
//#define IMGUI_STB_RECT_PACK_FILENAME  "my_folder/stb_rect_pack.h"
//#define IMGUI_STB_SPRINTF_FILENAME    "my_folder/stb_sprintf.h"
//#define IMGUI_DISABLE_STB_TRUETYPE_IMPLEMENTATION
//#define IMGUI_DISABLE_STB_RECT_PACK_IMPLEMENTATION

//---- Use stb_sprintf.h for a faster implementation of vsnprintf
//#define IMGUI_USE_STB_SPRINTF

//---- Use FreeType to build and rasterize the font atlas (empfohlen für PS3 falls psl1ght portlibs genutzt werden)
//#define IMGUI_ENABLE_FREETYPE

//---- Use stb_truetype to build and rasterize the font atlas (default)
//#define IMGUI_ENABLE_STB_TRUETYPE

//---- Define constructor and implicit cast operators to convert back<>forth between your math types and ImVec2/ImVec4.
/*
#define IM_VEC2_CLASS_EXTRA                                                     \
        constexpr ImVec2(const MyVec2& f) : x(f.x), y(f.y) {}                   \
        operator MyVec2() const { return MyVec2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                                     \
        constexpr ImVec4(const MyVec4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {}   \
        operator MyVec4() const { return MyVec4(x,y,z,w); }
*/

//---- Use 32-bit vertex indices (default is 16-bit)
//#define ImDrawIdx unsigned int

//---- Override ImDrawCallback signature
//struct ImDrawList;
//struct ImDrawCmd;
//typedef void (*MyImDrawCallback)(const ImDrawList* draw_list, const ImDrawCmd* cmd, void* my_renderer_user_data);
//#define ImDrawCallback MyImDrawCallback

//---- Debug Tools: Macro to break in Debugger
//#define IM_DEBUG_BREAK  IM_ASSERT(0)
//#define IM_DEBUG_BREAK  __debugbreak()

//---- Debug Tools: Have the Item Picker break in the ItemAdd() function instead of ItemHoverable()
//#define IMGUI_DEBUG_TOOL_ITEM_PICKER_EX

//---- Debug Tools: Enable slower asserts
//#define IMGUI_DEBUG_PARANOID

//---- Tip: You can add extra functions within the ImGui:: namespace, here or in your own headers files.
/*
namespace ImGui
{
    void MyFunction(const char* name, const MyMatrix44& v);
}
*/