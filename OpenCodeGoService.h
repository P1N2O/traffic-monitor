#pragma once
#include <string>
#include <vector>

// A small JSON value parser used only for the fixed response shape returned by
// the OpenCode Go usage endpoint. The endpoint always returns an object tree of
// {"usage":{"rolling":{...},"weekly":{...},"monthly":{...}}} so a tiny recursive
// parser is sufficient and avoids pulling in a heavyweight JSON dependency.
class CJsonNode
{
public:
    enum class Type { Null, Object, Array, Number, String, Bool };

    Type type = Type::Null;
    double number = 0;
    bool boolean = false;
    std::wstring str;                       // string value (or key for an object)

    // Children for object/array nodes.
    std::vector<std::pair<std::wstring, CJsonNode>> children;

    const CJsonNode* Find(const wchar_t* key) const;
};

// Parses a JSON document (UTF-8 bytes) into a node tree. Returns false on malformed input.
bool JsonParse(const std::string& text, CJsonNode& root);

// ---------------------------------------------
// OpenCode Go usage
// ---------------------------------------------
struct OpenCodeWindow
{
    int percent = -1;            // -1 => window not reported by the API
    std::wstring resets_at;      // ISO-8601 text ("" when absent)
    bool ok = false;             // "status":"ok"
};

struct OpenCodeGoUsage
{
    OpenCodeWindow rolling;
    OpenCodeWindow weekly;
    OpenCodeWindow monthly;
    bool has_usage = false;      // true when at least one window was parsed
    std::string error;           // non-empty when the whole request/parse failed
};

// Fetches OpenCode Go usage from https://opencode.ai/zen/go/v1/usage using the
// supplied bearer token. This is a blocking call intended for a background thread.
// On success OpenCodeGoUsage::error is empty and has_usage reflects parsed windows;
// on transport/HTTP/parse failure error carries a short ASCII description.
OpenCodeGoUsage FetchOpenCodeGoUsage(const std::wstring& api_key);

// Formats a percent (0..100) as a wide string, e.g. L"18%" or L"--".
std::wstring FormatPercentText(int percent);

// Formats a window's value string, e.g. L"18% (reset 2026-08-17)" or L"--".
std::wstring FormatWindowValue(int percent, const std::wstring& resets_at, bool include_reset);

// Formats just the reset countdown of one window, e.g. L"1h", L"3d", or L"-"
// when there is no reset time / data.
std::wstring FormatResetCountdown(const std::wstring& resets_at);
