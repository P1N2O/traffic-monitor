#pragma once
#include <string>
#include <map>

#define g_data CDataManager::Instance()

// Which display row an item represents. The first three are the OpenCode Go usage
// windows (Rolling / Weekly / Monthly); Reset is an extra combined row showing the
// next reset countdown for all three windows.
enum class DisplayWindow
{
    Rolling = 0,
    Weekly = 1,
    Monthly = 2,
    Reset = 3,
    Count = 4,
};

static const wchar_t* DisplayWindowShortLabel(DisplayWindow w)
{
    switch (w)
    {
    case DisplayWindow::Weekly:  return L"W";
    case DisplayWindow::Monthly: return L"M";
    case DisplayWindow::Reset:   return L"RS";
    default:                     return L"R";
    }
}

struct SettingData
{
    std::wstring api_key;
    // Seconds between API fetches (background thread). Default 60, min 5.
    int refresh_interval_seconds = 60;
    // Whether each usage row (R/W/M) value also carries a compact reset countdown,
    // e.g. "2% (4h)" vs "2%". (The dedicated Reset row always shows its countdowns.)
    bool show_reset_time = true;
};

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;

    // Cached usage values, written by the background fetch thread and read by
    // the display thread. All access goes through the lock so GetItemValueText
    // (called frequently on the UI thread) never sees torn data.
    void SetUsage(const struct OpenCodeGoUsage& usage);
    void SetErrorMessage(const std::wstring& message);
    void ClearError();

    // Display label / value for one window (Rolling/Weekly/Monthly/Reset).
    const wchar_t* GetDisplayLabel(DisplayWindow w) const;
    const wchar_t* GetDisplayValue(DisplayWindow w) const;
    // Graph value (0..1) for one window; -1 when unavailable.
    float GetGraphValue(DisplayWindow w) const;
    std::wstring GetError() const;
    bool HasApiKey() const;

    // Get a string resource by id (Win32 LoadStringW). Pointer valid until the
    // next StringRes/GetDisplay* call on this thread.
    const wchar_t* StringRes(UINT id) const;

public:
    SettingData m_setting_data;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    mutable CRITICAL_SECTION m_lock;
    mutable std::map<UINT, std::wstring> m_string_table;

    // Buffers for GetDisplayLabel/GetDisplayValue — the returned pointers are only
    // valid until the next call, which is the plugin text contract. One buffer per
    // window so the three rows don't clobber each other.
    mutable std::wstring m_label_buffer[static_cast<size_t>(DisplayWindow::Count)];
    mutable std::wstring m_value_buffer[static_cast<size_t>(DisplayWindow::Count)];

    int m_rolling_percent{ -1 };
    int m_weekly_percent{ -1 };
    int m_monthly_percent{ -1 };
    bool m_has_usage{ false };
    std::wstring m_error;
    std::wstring m_rolling_reset;
    std::wstring m_weekly_reset;
    std::wstring m_monthly_reset;
};
