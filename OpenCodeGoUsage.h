#pragma once
#include "PluginInterface.h"
#include "OpenCodeGoItem.h"
#include <windows.h>
#include <thread>

class COpenCodeGoUsage : public ITMPlugin
{
private:
    COpenCodeGoUsage();
    ~COpenCodeGoUsage();

public:
    static COpenCodeGoUsage& Instance();

    HICON GetIcon(UINT id);
    void StartWorker();
    void StopWorker();

    // 通过 ITMPlugin 继承
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;
    virtual void* GetPluginIcon() override;

private:
    void LoadConfigFromApp();
    void WorkerLoop();

    // One item per display row: Rolling / Weekly / Monthly / Reset.
    COpenCodeGoItem m_items[4] = {
        COpenCodeGoItem(DisplayWindow::Rolling),
        COpenCodeGoItem(DisplayWindow::Weekly),
        COpenCodeGoItem(DisplayWindow::Monthly),
        COpenCodeGoItem(DisplayWindow::Reset),
    };

    ITrafficMonitor* m_app{};

    std::thread m_worker;
    HANDLE m_exit_event{};
    bool m_worker_started{};
    bool m_config_loaded{};
    CRITICAL_SECTION m_options_lock;

    static COpenCodeGoUsage m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
