// OptionsDlg.cpp — AI Usage settings dialog (pure Win32, no MFC).
//

#include "pch.h"
#include "OptionsDlg.h"
#include "PluginModule.h"
#include <cstdlib>

namespace
{

INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // lParam carries a pointer to the caller's SettingData.
        auto* data = reinterpret_cast<SettingData*>(lParam);
        if (data != nullptr)
        {
            SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(data));

            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, data->api_key.c_str());
            CheckDlgButton(hDlg, IDC_SHOW_RESET_CHECK, data->show_reset_time ? BST_CHECKED : BST_UNCHECKED);

            wchar_t buff[16];
            swprintf_s(buff, L"%d", data->refresh_interval_seconds);
            SetDlgItemTextW(hDlg, IDC_REFRESH_INTERVAL_EDIT, buff);
        }
        return TRUE;   // set focus to the first control
    }

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        if (id == IDOK)
        {
            auto* data = reinterpret_cast<SettingData*>(GetWindowLongPtrW(hDlg, DWLP_USER));
            if (data != nullptr)
            {
                wchar_t key[4096];
                GetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, key, 4096);
                data->api_key = key;

                data->show_reset_time = (IsDlgButtonChecked(hDlg, IDC_SHOW_RESET_CHECK) == BST_CHECKED);

                int seconds = 60;
                wchar_t interval[64];
                GetDlgItemTextW(hDlg, IDC_REFRESH_INTERVAL_EDIT, interval, 64);
                seconds = _wtoi(interval);
                if (seconds < 5)
                    seconds = 5;
                data->refresh_interval_seconds = seconds;
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (id == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

} // namespace

bool ShowOptionsDialog(HWND hParent, SettingData& data)
{
    INT_PTR result = DialogBoxParamW(
        GetPluginModuleHandle(),
        MAKEINTRESOURCE(IDD_OPTIONS_DIALOG),
        hParent,
        OptionsDlgProc,
        reinterpret_cast<LPARAM>(&data));
    return result == IDOK;
}
