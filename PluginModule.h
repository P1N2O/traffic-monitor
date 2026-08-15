#pragma once
#include <windows.h>

// Returns this plugin DLL's module handle, captured in DllMain(PROCESS_ATTACH).
// Used to load this DLL's own resources (icons, strings, dialogs) without an
// MFC resource chain.
HINSTANCE GetPluginModuleHandle();

// Returns the plugin's version string read from this DLL's embedded version
// resource (VS_VERSION_INFO "FileVersion"). Never a hard-coded value — it always
// matches the version set in OpenCodeGoUsage.rc, e.g. "0.7.0.0".
const wchar_t* GetPluginVersionString();
