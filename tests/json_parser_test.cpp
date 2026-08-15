// Standalone test for the JSON parser + window parsing used in OpenCodeGoService.cpp.
// Compile: g++ -std=c++17 -o json_test json_test.cpp && ./json_test
// Mirrors the logic of OpenCodeGoService.cpp so parsing can be verified without MFC/WinHTTP.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

// ---- Copied from OpenCodeGoService.h ----
class CJsonNode
{
public:
    enum class Type { Null, Object, Array, Number, String, Bool };
    Type type = Type::Null;
    double number = 0;
    bool boolean = false;
    std::wstring str;
    std::vector<std::pair<std::wstring, CJsonNode>> children;
    const CJsonNode* Find(const wchar_t* key) const;
};
bool JsonParse(const std::string& text, CJsonNode& root);

// ---- Copied from OpenCodeGoService.cpp parse logic ----
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
    if (g_cursor < g_end && *g_cursor == c) { ++g_cursor; return true; }
    return false;
}
bool JsonParseString(std::wstring& out)
{
    JsonSkipWs();
    if (g_cursor >= g_end || *g_cursor != '"') return false;
    ++g_cursor; out.clear();
    while (g_cursor < g_end)
    {
        unsigned char ch = static_cast<unsigned char>(*g_cursor);
        if (ch == '"') { ++g_cursor; return true; }
        if (ch == '\\')
        {
            ++g_cursor;
            if (g_cursor >= g_end) return false;
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
                if (g_end - g_cursor < 4) return false;
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
            default: return false;
            }
        }
        else { out += static_cast<wchar_t>(ch); ++g_cursor; }
    }
    return false;
}
bool JsonParseValue(CJsonNode& node)
{
    JsonSkipWs();
    if (g_cursor >= g_end) return false;
    char c = *g_cursor;
    switch (c)
    {
    case '{':
    {
        ++g_cursor;
        node.type = CJsonNode::Type::Object;
        JsonSkipWs();
        if (JsonConsume('}')) return true;
        while (true)
        {
            std::wstring key;
            if (!JsonParseString(key)) return false;
            if (!JsonConsume(':')) return false;
            CJsonNode child;
            if (!JsonParseValue(child)) return false;
            node.children.emplace_back(key, std::move(child));
            JsonSkipWs();
            if (JsonConsume('}')) return true;
            if (!JsonConsume(',')) return false;
        }
    }
    case '[':
    {
        ++g_cursor;
        node.type = CJsonNode::Type::Array;
        JsonSkipWs();
        if (JsonConsume(']')) return true;
        while (true)
        {
            CJsonNode child;
            if (!JsonParseValue(child)) return false;
            node.children.emplace_back(std::wstring(), std::move(child));
            JsonSkipWs();
            if (JsonConsume(']')) return true;
            if (!JsonConsume(',')) return false;
        }
    }
    case '"':
        node.type = CJsonNode::Type::String;
        return JsonParseString(node.str);
    case 't':
        if (g_end - g_cursor >= 4 && std::strncmp(g_cursor, "true", 4) == 0) { g_cursor += 4; node.type = CJsonNode::Type::Bool; node.boolean = true; return true; }
        return false;
    case 'f':
        if (g_end - g_cursor >= 5 && std::strncmp(g_cursor, "false", 5) == 0) { g_cursor += 5; node.type = CJsonNode::Type::Bool; node.boolean = false; return true; }
        return false;
    case 'n':
        if (g_end - g_cursor >= 4 && std::strncmp(g_cursor, "null", 4) == 0) { g_cursor += 4; node.type = CJsonNode::Type::Null; return true; }
        return false;
    default:
    {
        node.type = CJsonNode::Type::Number;
        const char* start = g_cursor;
        if (g_cursor < g_end && (*g_cursor == '-' || *g_cursor == '+')) ++g_cursor;
        bool any_digit = false;
        while (g_cursor < g_end && *g_cursor >= '0' && *g_cursor <= '9') { ++g_cursor; any_digit = true; }
        if (g_cursor < g_end && *g_cursor == '.') { ++g_cursor; while (g_cursor < g_end && *g_cursor >= '0' && *g_cursor <= '9') ++g_cursor; }
        if (!any_digit) return false;
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
        if (child.first == key) return &child.second;
    return nullptr;
}

struct W { int percent = -1; std::wstring resets_at; bool ok = false; };
void ParseWindow(const CJsonNode& parent, const wchar_t* key, W& out)
{
    const CJsonNode* win = parent.Find(key);
    if (!win || win->type != CJsonNode::Type::Object) return;
    const CJsonNode* status = win->Find(L"status");
    out.ok = status && status->type == CJsonNode::Type::String && status->str == L"ok";
    const CJsonNode* percent = win->Find(L"percent");
    if (percent && percent->type == CJsonNode::Type::Number)
    {
        out.percent = static_cast<int>(percent->number);
        if (out.percent < 0) out.percent = 0;
        if (out.percent > 100) out.percent = 100;
    }
    const CJsonNode* resets_at = win->Find(L"resetsAt");
    if (resets_at && resets_at->type == CJsonNode::Type::String)
        out.resets_at = resets_at->str;
}

int main()
{
    // Exact real response captured from the API.
    const char* response =
        "{\"usage\":{\"rolling\":{\"status\":\"ok\",\"percent\":1,\"resetsAt\":\"2026-08-15T12:34:47.397Z\"},"
        "\"weekly\":{\"status\":\"ok\",\"percent\":18,\"resetsAt\":\"2026-08-17T00:00:00.397Z\"},"
        "\"monthly\":{\"status\":\"ok\",\"percent\":11,\"resetsAt\":\"2026-09-01T01:15:47.397Z\"}}}";

    CJsonNode root;
    if (!JsonParse(response, root))
    {
        printf("FAIL: json parse\n");
        return 1;
    }
    const CJsonNode* usage_node = root.Find(L"usage");
    if (!usage_node)
    {
        printf("FAIL: missing usage\n");
        return 1;
    }
    W rolling, weekly, monthly;
    ParseWindow(*usage_node, L"rolling", rolling);
    ParseWindow(*usage_node, L"weekly", weekly);
    ParseWindow(*usage_node, L"monthly", monthly);

    int failures = 0;
    if (rolling.percent != 1) { printf("FAIL rolling percent=%d\n", rolling.percent); failures++; }
    if (weekly.percent != 18) { printf("FAIL weekly percent=%d\n", weekly.percent); failures++; }
    if (monthly.percent != 11) { printf("FAIL monthly percent=%d\n", monthly.percent); failures++; }
    if (!rolling.ok || !weekly.ok || !monthly.ok) { printf("FAIL status\n"); failures++; }
    if (rolling.resets_at != L"2026-08-15T12:34:47.397Z") { printf("FAIL rolling reset\n"); failures++; }

    // Edge cases
    CJsonNode r2;
    if (!JsonParse("{\"usage\":{\"rolling\":{\"percent\":150}}}", r2)) { printf("FAIL clamp parse\n"); failures++; }
    else
    {
        const CJsonNode* u = r2.Find(L"usage");
        W w; ParseWindow(*u, L"rolling", w);
        if (w.percent != 100) { printf("FAIL clamp upper %d\n", w.percent); failures++; }
    }
    // Malformed
    if (JsonParse("not json", r2)) { printf("FAIL malformed accepted\n"); failures++; }
    // Missing window -> stays -1
    if (JsonParse("{\"usage\":{\"weekly\":{\"percent\":5}}}", r2))
    {
        const CJsonNode* u = r2.Find(L"usage");
        W w; ParseWindow(*u, L"rolling", w);
        if (w.percent != -1) { printf("FAIL missing window %d\n", w.percent); failures++; }
    }

    if (failures == 0)
        printf("ALL TESTS PASSED (rolling=%d, weekly=%d, monthly=%d)\n", rolling.percent, weekly.percent, monthly.percent);
    return failures == 0 ? 0 : 1;
}
