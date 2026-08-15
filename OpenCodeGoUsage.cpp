#include "pch.h"
#include "OpenCodeGoUsage.h"
#include "DataManager.h"
#include "OpenCodeGoService.h"
#include "OptionsDlg.h"
#include "PluginModule.h"

COpenCodeGoUsage COpenCodeGoUsage::m_instance;

namespace
{

// Convert a short ASCII error string to a wide string for display.
std::wstring AsciiToWide(const std::string& ascii)
{
    if (ascii.empty())
        return std::wstring();
    std::wstring out;
    out.reserve(ascii.size());
    for (unsigned char ch : ascii)
        out += static_cast<wchar_t>(ch);
    return out;
}

} // namespace

COpenCodeGoUsage::COpenCodeGoUsage()
{
    InitializeCriticalSection(&m_options_lock);
}

COpenCodeGoUsage::~COpenCodeGoUsage()
{
    StopWorker();
    DeleteCriticalSection(&m_options_lock);
}

COpenCodeGoUsage& COpenCodeGoUsage::Instance()
{
    return m_instance;
}

HICON COpenCodeGoUsage::GetIcon(UINT id)
{
    return (HICON)LoadImageW(GetPluginModuleHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, 0);
}

void COpenCodeGoUsage::StartWorker()
{
    if (m_worker_started)
        return;
    m_exit_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_worker_started = true;
    m_worker = std::thread([this]() { WorkerLoop(); });
}

void COpenCodeGoUsage::StopWorker()
{
    if (!m_worker_started)
        return;
    m_worker_started = false;
    if (m_exit_event != nullptr)
        SetEvent(m_exit_event);
    if (m_worker.joinable())
        m_worker.join();
    if (m_exit_event != nullptr)
    {
        CloseHandle(m_exit_event);
        m_exit_event = nullptr;
    }
}

// Called on the model's monitor thread at the UI update cadence. All real work
// happens on the worker thread; this just makes sure the worker is alive.
void COpenCodeGoUsage::DataRequired()
{
    if (m_worker_started)
        return;
    // The worker could not be started lazily; nothing more to do here.
}

// Worker loop: fetch OpenCode Go usage periodically on a background thread, never
// on the monitor/UI thread. It respects the user's refresh interval and exits
// promptly when the plugin is shut down.
void COpenCodeGoUsage::WorkerLoop()
{
    DWORD last_fetch_ms = 0;

    while (true)
    {
        if (WaitForSingleObject(m_exit_event, 1000) == WAIT_OBJECT_0)
            return; // shutdown requested

        int interval = g_data.m_setting_data.refresh_interval_seconds;
        if (interval < 5)
            interval = 5;

        DWORD now = GetTickCount();
        DWORD due = last_fetch_ms + static_cast<DWORD>(interval) * 1000;
        if (now < due && last_fetch_ms != 0)
            continue; // not due yet

        // Always attempt a fetch on the first loop (last_fetch_ms == 0).
        // Copy the key under the lock so a settings change mid-fetch can't tear it,
        // but perform the (potentially long) network call outside the lock so the
        // options dialog never blocks on a slow request.
        std::wstring key;
        {
            EnterCriticalSection(&m_options_lock);
            key = g_data.m_setting_data.api_key;
            LeaveCriticalSection(&m_options_lock);
        }

        OpenCodeGoUsage usage = FetchOpenCodeGoUsage(key);

        if (usage.error.empty())
            g_data.SetUsage(usage);
        else
            g_data.SetErrorMessage(AsciiToWide(usage.error));

        last_fetch_ms = GetTickCount();
    }
}

void COpenCodeGoUsage::LoadConfigFromApp()
{
    if (m_config_loaded)
        return;
    std::wstring dir;
    if (m_app != nullptr)
    {
        const wchar_t* cfg_dir = m_app->GetPluginConfigDir();
        if (cfg_dir != nullptr)
            dir = cfg_dir;
    }
    g_data.LoadConfig(dir);
    m_config_loaded = true;
}

IPluginItem* COpenCodeGoUsage::GetItem(int index)
{
    if (index >= 0 && index < 4)
        return &m_items[index];
    return nullptr;
}

const wchar_t* COpenCodeGoUsage::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return g_data.StringRes(IDS_PLUGIN_NAME);
    case TMI_DESCRIPTION:
        return g_data.StringRes(IDS_PLUGIN_DESCRIPTION);
    case TMI_AUTHOR:
        return L"P1N2O";
    case TMI_COPYRIGHT:
        return L"\x00A9 2026 P1N2O";
    case TMI_VERSION:
        return GetPluginVersionString();
    case TMI_URL:
        return L"https://github.com/P1N2O/ocg-usage-plugin";
    default:
        break;
    }
    return L"";
}

ITMPlugin::OptionReturn COpenCodeGoUsage::ShowOptionsDialog(void* hParent)
{
    SettingData edited = g_data.m_setting_data;
    if (::ShowOptionsDialog((HWND)hParent, edited))
    {
        EnterCriticalSection(&m_options_lock);
        g_data.m_setting_data = edited;
        g_data.SaveConfig();
        LeaveCriticalSection(&m_options_lock);
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

void COpenCodeGoUsage::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        if (data != nullptr && !m_config_loaded)
        {
            g_data.LoadConfig(data);
            m_config_loaded = true;
        }
        break;
    default:
        break;
    }
}

void COpenCodeGoUsage::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
    LoadConfigFromApp();
    StartWorker();
}

void* COpenCodeGoUsage::GetPluginIcon()
{
    return GetIcon(IDI_ICON);
}

ITMPlugin* TMPluginGetInstance()
{
    return &COpenCodeGoUsage::Instance();
}
