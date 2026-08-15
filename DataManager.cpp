#include "pch.h"
#include "DataManager.h"
#include "OpenCodeGoService.h"
#include "PluginModule.h"
#include <cstdio>
#include <utility>

CDataManager CDataManager::m_instance;

namespace
{

void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
    wchar_t buff[32];
    swprintf_s(buff, L"%d", value);
    WritePrivateProfileStringW(app_name, key_name, buff, file_path);
}

std::wstring LoadPrivateProfileString(const wchar_t* app_name, const wchar_t* key_name, const wchar_t* default_value, const wchar_t* file_path)
{
    wchar_t buff[4096];
    DWORD len = GetPrivateProfileStringW(app_name, key_name, default_value, buff, 4096, file_path);
    return std::wstring(buff, len);
}

} // namespace

CDataManager::CDataManager()
{
    InitializeCriticalSection(&m_lock);
}

CDataManager::~CDataManager()
{
    SaveConfig();
    DeleteCriticalSection(&m_lock);
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    // Config file is placed in the main program's plugin config directory.
    if (!config_dir.empty())
        m_config_path = config_dir + L"OpenCodeGoUsage.ini";
    else
        m_config_path = L"OpenCodeGoUsage.ini";

    m_setting_data.api_key = LoadPrivateProfileString(L"config", L"api_key", L"", m_config_path.c_str());
    m_setting_data.refresh_interval_seconds = GetPrivateProfileIntW(L"config", L"refresh_seconds", 60, m_config_path.c_str());
    m_setting_data.show_reset_time = GetPrivateProfileIntW(L"config", L"show_reset_time", 1, m_config_path.c_str()) != 0;
}

void CDataManager::SaveConfig() const
{
    WritePrivateProfileStringW(L"config", L"api_key", m_setting_data.api_key.c_str(), m_config_path.c_str());
    WritePrivateProfileInt(L"config", L"refresh_seconds", m_setting_data.refresh_interval_seconds, m_config_path.c_str());
    WritePrivateProfileInt(L"config", L"show_reset_time", m_setting_data.show_reset_time ? 1 : 0, m_config_path.c_str());
}

void CDataManager::SetUsage(const OpenCodeGoUsage& usage)
{
    EnterCriticalSection(&m_lock);
    m_rolling_percent = usage.rolling.percent;
    m_weekly_percent = usage.weekly.percent;
    m_monthly_percent = usage.monthly.percent;
    m_rolling_reset = usage.rolling.resets_at;
    m_weekly_reset = usage.weekly.resets_at;
    m_monthly_reset = usage.monthly.resets_at;
    m_has_usage = usage.has_usage;
    m_error.clear();
    LeaveCriticalSection(&m_lock);
}

void CDataManager::SetErrorMessage(const std::wstring& message)
{
    EnterCriticalSection(&m_lock);
    m_error = message;
    m_has_usage = false;
    LeaveCriticalSection(&m_lock);
}

void CDataManager::ClearError()
{
    EnterCriticalSection(&m_lock);
    m_error.clear();
    LeaveCriticalSection(&m_lock);
}

const wchar_t* CDataManager::GetDisplayLabel(DisplayWindow w) const
{
    const size_t slot = static_cast<size_t>(w);
    EnterCriticalSection(&m_lock);
    if (w == DisplayWindow::Reset)
    {
        m_label_buffer[slot] = StringRes(IDS_LABEL_AI_RESET);
    }
    else
    {
        m_label_buffer[slot] = StringRes(IDS_LABEL_AI_USAGE);
        m_label_buffer[slot] += L" (";
        m_label_buffer[slot] += DisplayWindowShortLabel(w);
        m_label_buffer[slot] += L"):";
    }
    LeaveCriticalSection(&m_lock);
    return m_label_buffer[slot].c_str();
}

const wchar_t* CDataManager::GetDisplayValue(DisplayWindow w) const
{
    const size_t slot = static_cast<size_t>(w);
    EnterCriticalSection(&m_lock);

    if (w == DisplayWindow::Reset)
    {
        // Combined next-reset row: "1h • 3d • 12d" (Rolling • Weekly • Monthly).
        std::wstring combined;
        combined += FormatResetCountdown(m_rolling_reset);
        combined += L" \x2022 ";
        combined += FormatResetCountdown(m_weekly_reset);
        combined += L" \x2022 ";
        combined += FormatResetCountdown(m_monthly_reset);
        m_value_buffer[slot] = std::move(combined);
        LeaveCriticalSection(&m_lock);
        return m_value_buffer[slot].c_str();
    }

    int percent = -1;
    std::wstring reset;
    switch (w)
    {
    case DisplayWindow::Weekly:  percent = m_weekly_percent;  reset = m_weekly_reset;  break;
    case DisplayWindow::Monthly: percent = m_monthly_percent; reset = m_monthly_reset; break;
    default:                     percent = m_rolling_percent; reset = m_rolling_reset; break;
    }

    if (percent >= 0)
    {
        // We have a cached value for this window — show it. On a later failed
        // refresh this is the last "known good" value (graceful degradation):
        // e.g. "2% (4h)" instead of nothing or a broken "% (4h)".
        std::wstring value = FormatWindowValue(percent, reset, m_setting_data.show_reset_time);
        m_value_buffer[slot] = std::move(value);
    }
    else
    {
        // This window has never produced a value (e.g. no API key, or the API
        // has never returned data): show a plain "-".
        m_value_buffer[slot] = L"-";
    }

    LeaveCriticalSection(&m_lock);
    return m_value_buffer[slot].c_str();
}

std::wstring CDataManager::GetError() const
{
    EnterCriticalSection(&m_lock);
    std::wstring err = m_error;
    LeaveCriticalSection(&m_lock);
    return err;
}

bool CDataManager::HasApiKey() const
{
    return !m_setting_data.api_key.empty();
}

/// Returns the graph value (0..1) for one window. Returns -1 when no usage is
/// available (the main program should skip the graph).
float CDataManager::GetGraphValue(DisplayWindow w) const
{
    EnterCriticalSection(&m_lock);
    int percent = -1;
    switch (w)
    {
    case DisplayWindow::Weekly:  percent = m_weekly_percent;  break;
    case DisplayWindow::Monthly: percent = m_monthly_percent; break;
    case DisplayWindow::Reset:   percent = -1;                break;
    default:                     percent = m_rolling_percent; break;
    }
    LeaveCriticalSection(&m_lock);
    if (w == DisplayWindow::Reset || !m_has_usage || percent < 0)
        return -1.0f;
    return static_cast<float>(percent) / 100.0f;
}

const wchar_t* CDataManager::StringRes(UINT id) const
{
    {
        auto iter = m_string_table.find(id);
        if (iter != m_string_table.end())
            return iter->second.c_str();
    }

    // Not cached yet — load from this DLL's resources.
    wchar_t buff[512];
    int len = LoadStringW(GetPluginModuleHandle(), id, buff, 512);
    std::wstring& slot = m_string_table[id];
    if (len > 0)
        slot.assign(buff, static_cast<size_t>(len));
    return slot.c_str();
}
