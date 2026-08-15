#pragma once
#include "DataManager.h"
#include <windows.h>

// Shows the AI Usage options dialog (a Win32 modal dialog — no MFC dependency).
// On OK the dialog's choices are written into @param data and returns true.
// On Cancel it returns false and @param data is left unchanged.
bool ShowOptionsDialog(HWND hParent, SettingData& data);
