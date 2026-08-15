#include "pch.h"
#include "OpenCodeGoItem.h"
#include "DataManager.h"
#include <cstdio>

COpenCodeGoItem::COpenCodeGoItem(DisplayWindow window)
    : m_window(window)
{
}

const wchar_t* COpenCodeGoItem::GetItemName() const
{
    if (m_window == DisplayWindow::Reset)
    {
        m_name_cache = L"OpenCode Go Reset";
    }
    else
    {
        wchar_t name[64];
        swprintf_s(name, L"OpenCode Go %ls", DisplayWindowShortLabel(m_window));
        m_name_cache = name;
    }
    return m_name_cache.c_str();
}

const wchar_t* COpenCodeGoItem::GetItemId() const
{
    switch (m_window)
    {
    case DisplayWindow::Weekly:  return L"ocg-weekly";
    case DisplayWindow::Monthly: return L"ocg-monthly";
    case DisplayWindow::Reset:   return L"ocg-reset";
    default:                     return L"ocg-rolling";
    }
}

const wchar_t* COpenCodeGoItem::GetItemLableText() const
{
    return CDataManager::Instance().GetDisplayLabel(m_window);
}

const wchar_t* COpenCodeGoItem::GetItemValueText() const
{
    return CDataManager::Instance().GetDisplayValue(m_window);
}

const wchar_t* COpenCodeGoItem::GetItemValueSampleText() const
{
    // Reset row is independent of the Show-reset-time toggle.
    if (m_window == DisplayWindow::Reset)
        return L"99d \x2022 99d \x2022 99d";

    // Size the column to the widest value for the current setting, so there is
    // no excess right-aligned gap when "Show reset time" is off.
    if (CDataManager::Instance().m_setting_data.show_reset_time)
        return L"100% (18d)";
    return L"100%";
}

int COpenCodeGoItem::IsDrawResourceUsageGraph() const
{
    return CDataManager::Instance().GetGraphValue(m_window) >= 0.0f ? 1 : 0;
}

float COpenCodeGoItem::GetResourceUsageGraphValue() const
{
    float value = CDataManager::Instance().GetGraphValue(m_window);
    return value >= 0.0f ? value : 0.0f;
}
