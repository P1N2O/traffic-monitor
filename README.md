# OpenCode Go Usage — TrafficMonitor plugin

A standalone [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) plugin
that shows your **OpenCode Go** subscription usage (rolling / weekly / monthly) right
in the taskbar / main window, driven by the **public OpenCode Go usage API**
(`https://opencode.ai/zen/go/v1/usage`).

It is built against the official
[`PluginInterface.h`](https://github.com/zhongyang219/TrafficMonitor/tree/master/include)
(supplied here in the repo) and the documented plugin contract
([Plugin Development Guide](https://github.com/zhongyang219/TrafficMonitor/wiki/Plugin-Development-Guide)).

### Plugin identity
- **Name:** OpenCode Go Usage
- **Description:** Displays Open Code Go subscription usage via Official API.
- **Author:** P1N2O
- **Copyright:** © 2026 P1N2O
- **Home:** https://github.com/P1N2O/ocg-usage-plugin
- **Item IDs:** `ocg-rolling`, `ocg-weekly`, `ocg-monthly`, `ocg-reset`
- **DLL:** `OpenCodeGoUsage.dll`

## Features
- **Your own API key** — each user enters their own OpenCode Go bearer token in the
  item settings; nothing is hard-coded in the plugin.
- Shows **four rows** (like the CPU/GPU monitors):
  ```
  AI (R): 2%    (2h)
  AI (W): 7%    (3d)
  AI (M): 12%   (18d)
  AI RESET: 1h • 3d • 12d
  ```
  - `AI (R)` / `AI (W)` / `AI (M)` each show one window's percent plus a compact
    single-unit reset countdown (`2h`, `3d`, `18d`) when **Inline reset time** is on.
  - `AI RESET` always shows the next reset for all three windows together
    (`1h • 3d • 12d`), independent of the Inline-reset-time toggle.
  Rows are host-drawn text items aligned like TrafficMonitor's own items.
- **All rows are always available** — no per-row visibility toggles in the plugin.
  (You can still hide a row from TrafficMonitor's own display-item settings.)
- **Configurable refresh interval** — set how often it queries the API (default 60 s, min 5 s).
- Optional **Inline reset time** toggle (`12%` → `12% (18d)`).
- Background fetch thread (network never blocks the UI); each usage row also exposes a
  resource-usage graph value so the taskbar item can draw a small usage bar.

## Install (for users)
1. Download the DLL for your architecture:
   - `OpenCodeGoUsage-x64.dll` — 64-bit Windows (most machines)
   - `OpenCodeGoUsage-x86.dll` — 32-bit Windows
2. Copy it into your TrafficMonitor **`plugins`** folder (next to `TrafficMonitor.exe`)
   and rename it to **`OpenCodeGoUsage.dll`**.
3. Start/restart TrafficMonitor.
4. Right-click an **OpenCode Go Usage** row (AI (R) / AI (W) / AI (M) / AI RESET)
   → **Item settings**:
   - paste your **OpenCode Go API key**,
   - set the refresh interval (seconds),
   - optionally toggle **Inline reset time**.

The plugin appears as four rows (`AI (R)`, `AI (W)`, `AI (M)`, `AI RESET`).
The settings are stored in `<TrafficMonitor>\plugins\OpenCodeGoUsage.ini`.

## For developers — build the plugin

You only need **one** plugin DLL; you do **not** need to build TrafficMonitor itself.

Requirements:
- Visual Studio 2022 (Community is fine) with the **Desktop development with C++** workload
  (MSVC `v143` + Windows 10/11 SDK). The plugin is **pure Win32 and has no MFC dependency**,
  so no extra MFC component is required.

Steps:
1. Open `OpenCodeGoUsage.sln` in Visual Studio.
2. Set **Release | x64** (or x86) and **Build**.
3. The output is written to `bin\Release\x64\OpenCodeGoUsage.dll`.

Or from the command line:
```
msbuild OpenCodeGoUsage.sln -p:configuration=Release -p:platform=x64 -p:platformToolset=v143
```
Output lands in `bin\Release\x64\OpenCodeGoUsage.dll` (x86 → `bin\Release\Win32\`).

Drop that DLL into TrafficMonitor's `plugins\` folder to test.

### CI / Releases
The included GitHub Action (`.github/workflows/build.yml`) builds the DLL for **x64** and
**x86** on `windows-latest`. On a `v*` tag it publishes them as a public **GitHub Release**.

## How the usage is fetched
`GET https://opencode.ai/zen/go/v1/usage` with header `Authorization: Bearer <your key>`:
```json
{
  "usage": {
    "rolling": { "status": "ok", "percent": 1,  "resetsAt": "2026-08-15T12:34:47.397Z" },
    "weekly":  { "status": "ok", "percent": 18, "resetsAt": "2026-08-17T00:00:00.397Z" },
    "monthly": { "status": "ok", "percent": 11, "resetsAt": "2026-09-01T01:15:47.397Z" }
  }
}
```
Each window's `percent` (clamped 0–100) and `resetsAt` are parsed, cached, and rendered
through the standard `IPluginItem::GetItemLableText` / `GetItemValueText` contract — four
items (Rolling / Weekly / Monthly / Reset). The `resetsAt` timestamp is shown as a compact
countdown (`2h`, `3d`, `18d`).

## Config keys (`plugins\OpenCodeGoUsage.ini`)
| Key | Meaning |
| --- | --- |
| `api_key` | Your OpenCode Go bearer token |
| `show_reset_time` | `1` to append the reset countdown to each usage row (e.g. `12% (18d)`) |
| `refresh_seconds` | Seconds between API fetches (default 60, min 5) |

## Security note
The API key is stored **in plain text** in `plugins\OpenCodeGoUsage.ini` on the user's own machine and
sent only to `opencode.ai`. Do not use a privileged key for public testing.

## Files
| File | Purpose |
| --- | --- |
| `OpenCodeGoUsage.vcxproj` / `OpenCodeGoUsage.sln` | Standalone build project |
| `PluginInterface.h` | Vendored official TrafficMonitor plugin interface (v8) |
| `OpenCodeGoService.*` | WinHTTP client + JSON parser for the usage API |
| `DataManager.*` | Settings persistence + thread-safe usage cache |
| `OpenCodeGoItem.*` | `IPluginItem` rows — one per window (AI R / AI W / AI M / AI RESET) |
| `OpenCodeGoUsage.*` | `ITMPlugin` entry point + background fetch thread |
| `OptionsDlg.*`, `OpenCodeGoUsage.rc` | Item settings dialog + resources |
