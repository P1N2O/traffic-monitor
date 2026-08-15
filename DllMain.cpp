#include "pch.h"
#include "PluginModule.h"
#include <string>
#include <cstdio>

namespace
{
HINSTANCE g_plugin_module = nullptr;
}

HINSTANCE GetPluginModuleHandle()
{
    return g_plugin_module;
}

const wchar_t* GetPluginVersionString()
{
    // Read the version from this DLL's embedded VS_VERSION_INFO "FileVersion"
    // resource so it is never a hard-coded value and always matches the version
    // declared in OpenCodeGoUsage.rc.
    static std::wstring version_cache;      // thread-unsafe init, but read once at startup
    if (!version_cache.empty())
        return version_cache.c_str();

    wchar_t module_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(g_plugin_module, module_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return L"";

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(module_path, &handle);
    if (size == 0)
        return L"";

    std::string buffer(size, '\0');
    if (!GetFileVersionInfoW(module_path, 0, size, &buffer[0]))
        return L"";

    struct LANGANDCODEPAGE
    {
        WORD wLanguage;
        WORD wCodePage;
    };

    LANGANDCODEPAGE* lang = nullptr;
    UINT lang_len = 0;
    if (!VerQueryValueW(&buffer[0], L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&lang), &lang_len) || lang_len < sizeof(LANGANDCODEPAGE))
    {
        return L"";
    }

    wchar_t subblock[64];
    swprintf_s(subblock, L"\\StringFileInfo\\%04x%04x\\FileVersion", lang[0].wLanguage, lang[0].wCodePage);

    void* value = nullptr;
    UINT value_len = 0;
    if (!VerQueryValueW(&buffer[0], subblock, &value, &value_len) || value == nullptr || value_len == 0)
        return L"";

    version_cache = static_cast<const wchar_t*>(value);
    return version_cache.c_str();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_plugin_module = static_cast<HINSTANCE>(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#pragma comment(lib, "version.lib")
