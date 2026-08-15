#pragma once
#include "PluginInterface.h"
#include "DataManager.h"

// One display row of the AI Usage plugin: Rolling / Weekly / Monthly, plus a
// combined Reset row. The rows are host-drawn text items (not custom drawn) so
// they align like TrafficMonitor's own items.
class COpenCodeGoItem : public IPluginItem
{
public:
    explicit COpenCodeGoItem(DisplayWindow window);

    // 通过 IPluginItem 继承
    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual int IsDrawResourceUsageGraph() const override;
    virtual float GetResourceUsageGraphValue() const override;

private:
    DisplayWindow m_window;
    mutable std::wstring m_name_cache;
};
