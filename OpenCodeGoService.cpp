#include "pch.h"
#include "OpenCodeGoService.h"

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

// ---------------------------------------------------------------
// Minimal recursive JSON parser (no numbers with exponents needed).
// ---------------------------------------------------------------
namespace
{

const char* g_cursor;
const char* g_end;

void JsonSkipWs()
{
    while (g_cursor < g_end && (*g_cursor == ' ' || *g_cursor == '\t' || *g_cursor == '\r' || *g_cursor == '\n'))
        ++g_cursor;
}

bool JsonConsume(char c)
{
    JsonSkipWs();
    if (g_cursor < g_end && *g_cursor == c)
    {
        ++g_cursor;
        return true;
    }
    return false;
}

bool JsonParseString(std::wstring& out)
{
    JsonSkipWs();
    if (g_cursor >= g_end || *g_cursor != '"')
        return false;
    ++g_cursor;
    out.clear();
    while (g_cursor < g_end)
    {
        unsigned char ch = static_cast<unsigned char>(*g_cursor);
        if (ch == '"')
        {
            ++g_cursor;
            return true;
        }
        if (ch == '\\')
        {
            ++g_cursor;
            if (g_cursor >= g_end)
                return false;
            wchar_t esc = static_cast<unsigned char>(*g_cursor++);
            switch (esc)
            {
            case '"': out += L'"'; break;
            case '\\': out += L'\\'; break;
            case '/': out += L'/'; break;
            case 'b': out += L'\b'; break;
            case 'f': out += L'\f'; break;
            case 'n': out += L'\n'; break;
            case 'r': out += L'\r'; break;
            case 't': out += L'\t'; break;
            case 'u':
            {
                if (g_end - g_cursor < 4)
                    return false;
                unsigned int cp = 0;
                for (int i = 0; i < 4; ++i)
                {
                    wchar_t h = static_cast<unsigned char>(*g_cursor++);
                    cp <<= 4;
                    if (h >= L'0' && h <= L'9') cp |= static_cast<unsigned int>(h - L'0');
                    else if (h >= L'a' && h <= L'f') cp |= static_cast<unsigned int>(h - L'a' + 10);
                    else if (h >= L'A' && h <= L'F') cp |= static_cast<unsigned int>(h - L'A' + 10);
                    else return false;
                }
                out += static_cast<wchar_t>(cp);
                break;
            }
            default:
                return false;
            }
        }
        else
        {
            out += static_cast<wchar_t>(ch);
            ++g_cursor;
        }
    }
    return false;
}

bool JsonParseValue(CJsonNode& node)
{
    JsonSkipWs();
    if (g_cursor >= g_end)
        return false;
    char c = *g_cursor;
    switch (c)
    {
    case '{':
    {
        ++g_cursor;
        node.type = CJsonNode::Type::Object;
        JsonSkipWs();
        if (JsonConsume('}'))
            return true;
        while (true)
        {
            std::wstring key;
            if (!JsonParseString(key))
                return false;
            if (!JsonConsume(':'))
                return false;
            CJsonNode child;
            if (!JsonParseValue(child))
                return false;
            node.children.emplace_back(key, std::move(child));
            JsonSkipWs();
            if (JsonConsume('}'))
                return true;
            if (!JsonConsume(','))
                return false;
        }
    }
    case '[':
    {
        ++g_cursor;
        node.type = CJsonNode::Type::Array;
        JsonSkipWs();
        if (JsonConsume(']'))
            return true;
        while (true)
        {
            CJsonNode child;
            if (!JsonParseValue(child))
                return false;
            // Array elements are stored with an empty key.
            node.children.emplace_back(std::wstring(), std::move(child));
            JsonSkipWs();
            if (JsonConsume(']'))
                return true;
            if (!JsonConsume(','))
                return false;
        }
    }
    case '"':
    {
        node.type = CJsonNode::Type::String;
        return JsonParseString(node.str);
    }
    case 't':
        if (g_end - g_cursor >= 4 && std::strncmp(g_cursor, "true", 4) == 0)
        {
            g_cursor += 4;
            node.type = CJsonNode::Type::Bool;
            node.boolean = true;
            return true;
        }
        return false;
    case 'f':
        if (g_end - g_cursor >= 5 && std::strncmp(g_cursor, "false", 5) == 0)
        {
            g_cursor += 5;
            node.type = CJsonNode::Type::Bool;
            node.boolean = false;
            return true;
        }
        return false;
    case 'n':
        if (g_end - g_cursor >= 4 && std::strncmp(g_cursor, "null", 4) == 0)
        {
            g_cursor += 4;
            node.type = CJsonNode::Type::Null;
            return true;
        }
        return false;
    default:
    {
        // Number: [-+]?digits(.digits)?
        node.type = CJsonNode::Type::Number;
        const char* start = g_cursor;
        if (g_cursor < g_end && (*g_cursor == '-' || *g_cursor == '+'))
            ++g_cursor;
        bool any_digit = false;
        while (g_cursor < g_end && *g_cursor >= '0' && *g_cursor <= '9')
        {
            ++g_cursor;
            any_digit = true;
        }
        if (g_cursor < g_end && *g_cursor == '.')
        {
            ++g_cursor;
            while (g_cursor < g_end && *g_cursor >= '0' && *g_cursor <= '9')
                ++g_cursor;
        }
        if (!any_digit)
            return false;
        std::string num(start, static_cast<size_t>(g_cursor - start));
        node.number = std::atof(num.c_str());
        return true;
    }
    }
}

} // namespace

bool JsonParse(const std::string& text, CJsonNode& root)
{
    root = CJsonNode();
    g_cursor = text.c_str();
    g_end = g_cursor + text.size();
    return JsonParseValue(root);
}

const CJsonNode* CJsonNode::Find(const wchar_t* key) const
{
    for (const auto& child : children)
    {
        if (child.first == key)
            return &child.second;
    }
    return nullptr;
}

// ---------------------------------------------------------------
// OpenCode Go usage fetch
// ---------------------------------------------------------------
namespace
{

int JsonNumberAsInt(const CJsonNode* n)
{
    if (n == nullptr || n->type != CJsonNode::Type::Number)
        return -1;
    return static_cast<int>(n->number);
}

void ParseWindow(const CJsonNode& parent, const wchar_t* key, OpenCodeWindow& out)
{
    const CJsonNode* win = parent.Find(key);
    if (win == nullptr || win->type != CJsonNode::Type::Object)
        return;

    const CJsonNode* status = win->Find(L"status");
    out.ok = status != nullptr && status->type == CJsonNode::Type::String && status->str == L"ok";

    const CJsonNode* percent = win->Find(L"percent");
    int pct = JsonNumberAsInt(percent);
    if (pct >= 0)
    {
        out.percent = pct;
        if (out.percent < 0) out.percent = 0;
        if (out.percent > 100) out.percent = 100;
    }

    const CJsonNode* resets_at = win->Find(L"resetsAt");
    if (resets_at != nullptr && resets_at->type == CJsonNode::Type::String)
        out.resets_at = resets_at->str;
}

} // namespace

OpenCodeGoUsage FetchOpenCodeGoUsage(const std::wstring& api_key)
{
    OpenCodeGoUsage usage;
    if (api_key.empty())
    {
        usage.error = "Enter an API key";
        return usage;
    }

    const wchar_t* host = L"opencode.ai";
    const wchar_t* path = L"/zen/go/v1/usage";
    const bool https = true;

    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool success = false;

    do
    {
        hSession = WinHttpOpen(L"TrafficMonitor-AIUsage/1.0",
                               WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession == nullptr)
        {
            usage.error = "WinHttpOpen failed";
            break;
        }
        hConnect = WinHttpConnect(hSession, host,
                                  https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
                                  0);
        if (hConnect == nullptr)
        {
            usage.error = "WinHttpConnect failed";
            break;
        }

        const wchar_t* accept_types[] = { L"application/json", nullptr };
        hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr,
                                      WINHTTP_NO_REFERER, accept_types,
                                      https ? WINHTTP_FLAG_SECURE : 0);
        if (hRequest == nullptr)
        {
            usage.error = "WinHttpOpenRequest failed";
            break;
        }

        DWORD timeout = 30000;
        WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);

        std::wstring header = L"Authorization: Bearer " + api_key + L"\r\n";
        if (!WinHttpSendRequest(hRequest, header.c_str(), static_cast<DWORD>(header.size()),
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            usage.error = "WinHttpSendRequest failed";
            break;
        }
        if (!WinHttpReceiveResponse(hRequest, nullptr))
        {
            usage.error = "WinHttpReceiveResponse failed";
            break;
        }

        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                            WINHTTP_NO_HEADER_INDEX);
        if (status_code == 401 || status_code == 403)
        {
            usage.error = "API key rejected (HTTP 401/403)";
            break;
        }
        if (status_code == 429)
        {
            usage.error = "Rate limited (HTTP 429)";
            break;
        }
        if (status_code < 200 || status_code >= 300)
        {
            char buff[64];
            sprintf_s(buff, "HTTP error %lu", status_code);
            usage.error = buff;
            break;
        }

        std::string body;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do
        {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
            {
                if (body.empty())
                    usage.error = "WinHttpQueryDataAvailable failed";
                break;
            }
            if (dwSize == 0)
                break;
            std::string chunk;
            chunk.resize(dwSize);
            if (!WinHttpReadData(hRequest, &chunk[0], dwSize, &dwDownloaded))
            {
                if (body.empty())
                    usage.error = "WinHttpReadData failed";
                break;
            }
            chunk.resize(dwDownloaded);
            body += chunk;
        } while (dwSize > 0);

        if (body.empty() && usage.error.empty())
        {
            usage.error = "Empty response";
            break;
        }
        if (!usage.error.empty())
            break;

        CJsonNode root;
        if (!JsonParse(body, root))
        {
            usage.error = "Malformed JSON";
            break;
        }
        const CJsonNode* usage_node = root.Find(L"usage");
        if (usage_node == nullptr || usage_node->type != CJsonNode::Type::Object)
        {
            usage.error = "Response missing usage";
            break;
        }

        ParseWindow(*usage_node, L"rolling", usage.rolling);
        ParseWindow(*usage_node, L"weekly", usage.weekly);
        ParseWindow(*usage_node, L"monthly", usage.monthly);
        usage.has_usage = usage.rolling.ok || usage.weekly.ok || usage.monthly.ok;
        success = true;
    } while (false);

    if (!success && usage.error.empty())
        usage.error = "Unknown error";

    if (hRequest != nullptr) WinHttpCloseHandle(hRequest);
    if (hConnect != nullptr) WinHttpCloseHandle(hConnect);
    if (hSession != nullptr) WinHttpCloseHandle(hSession);
    return usage;
}

std::wstring FormatPercentText(int percent)
{
    if (percent < 0)
        return L"--";
    wchar_t buff[32];
    swprintf_s(buff, L"%d%%", percent);
    return buff;
}

namespace
{
// Parse an ISO-8601 UTC timestamp like "2026-08-17T00:00:00.397Z" into seconds
// until "now". Returns false when it cannot be parsed or is in the past.
bool SecondsUntil(const std::wstring& iso, __int64& out_seconds)
{
    int y = 0, mo = 0, d = 0, hh = 0, mi = 0, ss = 0;
    if (swscanf_s(iso.c_str(), L"%d-%d-%dT%d:%d:%d", &y, &mo, &d, &hh, &mi, &ss) < 6)
        return false;
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31 || hh < 0 || hh > 23 || mi < 0 || mi > 59 || ss < 0 || ss > 60)
        return false;

    SYSTEMTIME st{};
    st.wYear = static_cast<WORD>(y);
    st.wMonth = static_cast<WORD>(mo);
    st.wDay = static_cast<WORD>(d);
    st.wHour = static_cast<WORD>(hh);
    st.wMinute = static_cast<WORD>(mi);
    st.wSecond = static_cast<WORD>(ss);
    // Input is UTC (trailing Z).
    SYSTEMTIME stUtc = st;

    FILETIME ftUtc{}, ftNowUtc{};
    if (!SystemTimeToFileTime(&stUtc, &ftUtc))
        return false;
    GetSystemTimeAsFileTime(&ftNowUtc);

    ULARGE_INTEGER a, b;
    a.LowPart = ftUtc.dwLowDateTime;
    a.HighPart = ftUtc.dwHighDateTime;
    b.LowPart = ftNowUtc.dwLowDateTime;
    b.HighPart = ftNowUtc.dwHighDateTime;

    __int64 delta100ns = static_cast<__int64>(a.QuadPart) - static_cast<__int64>(b.QuadPart);
    __int64 seconds = delta100ns / 10000000LL;
    if (seconds < 0)
        seconds = 0;
    out_seconds = seconds;
    return true;
}

// A single compact unit for space-constrained display: "2h", "3d", "18d", "45m".
// Only the most significant unit is shown (no "4h 59m").
std::wstring FormatCountdown(__int64 seconds)
{
    const __int64 days = seconds / 86400;
    const __int64 hours = (seconds % 86400) / 3600;
    const __int64 minutes = (seconds % 3600) / 60;

    wchar_t buff[32];
    if (days >= 1)
        swprintf_s(buff, L"%lldd", days);
    else if (hours >= 1)
        swprintf_s(buff, L"%lldh", hours);
    else
        swprintf_s(buff, L"%lldm", minutes > 0 ? minutes : 1);
    return buff;
}
} // namespace

std::wstring FormatWindowValue(int percent, const std::wstring& resets_at, bool include_reset)
{
    if (percent < 0)
        return L"--";
    if (!include_reset || resets_at.empty())
        return FormatPercentText(percent);

    __int64 seconds = 0;
    if (!SecondsUntil(resets_at, seconds))
        return FormatPercentText(percent);

    wchar_t buff[64];
    swprintf_s(buff, L"%d%% (%ls)", percent, FormatCountdown(seconds).c_str());
    return buff;
}

std::wstring FormatResetCountdown(const std::wstring& resets_at)
{
    if (resets_at.empty())
        return L"-";
    __int64 seconds = 0;
    if (!SecondsUntil(resets_at, seconds))
        return L"-";
    return FormatCountdown(seconds);
}
