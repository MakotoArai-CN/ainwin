// Small UTF-8 and string helpers shared by the planner and the plugin.
#pragma once

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace finder {

inline std::string utf8Of(const std::wstring& w)
{
    if (w.empty())
        return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

inline std::wstring widenOf(const std::string& s)
{
    if (s.empty())
        return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// argv comes in the console's ANSI code page (GBK on a Chinese Windows);
// a manifest passes UTF-8. Accept both, so --plan reads either.
inline std::string argToUtf8(const char* arg)
{
    if (!arg || !*arg)
        return {};
    const int wide = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, arg, -1, nullptr, 0);
    if (wide > 0)
        return std::string(arg);
    const int ansi = ::MultiByteToWideChar(CP_ACP, 0, arg, -1, nullptr, 0);
    if (ansi <= 0)
        return std::string(arg);
    std::wstring w(static_cast<std::size_t>(ansi), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, arg, -1, w.data(), ansi);
    if (!w.empty() && w.back() == L'\0')
        w.pop_back();
    return utf8Of(w);
}

// %USERPROFILE%\Documents -> C:\Users\…\Documents
inline std::string expandEnv(const std::string& text)
{
    const std::wstring wide = [&] {
        if (text.empty())
            return std::wstring();
        const int n = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), w.data(), n);
        return w;
    }();
    if (wide.empty())
        return text;
    wchar_t buffer[32768];
    const DWORD n = ::ExpandEnvironmentStringsW(wide.c_str(), buffer, 32768);
    if (n == 0 || n > 32768)
        return text;
    return utf8Of(std::wstring(buffer, n - 1));
}

inline std::string lowerAscii(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline bool isAscii(const std::string& s)
{
    for (const unsigned char c : s) {
        if (c >= 0x80)
            return false;
    }
    return true;
}

inline std::string trim(const std::string& s)
{
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
        ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
        --b;
    return s.substr(a, b - a);
}

// Full-width punctuation that people type with a Chinese IME becomes the
// ASCII one, so the same sentence parses the same way either way.
inline std::string normalize(std::string s)
{
    struct Pair { const char* from; const char* to; };
    static const Pair pairs[] = {
        {"？", "?"}, {"，", ","}, {"：", ":"}, {"；", ";"}, {"（", "("}, {"）", ")"},
        {"！", "!"}, {"。", "."}, {"“", "\""}, {"”", "\""}, {"‘", "'"}, {"’", "'"},
        {"、", ","}, {"　", " "}, {"／", "/"}, {"＼", "\\"}, {"＝", "="}, {"＋", "+"},
    };
    for (const Pair& p : pairs) {
        for (std::size_t at = s.find(p.from); at != std::string::npos; at = s.find(p.from, at + 1))
            s.replace(at, std::string(p.from).size(), p.to);
    }
    return lowerAscii(trim(s));
}

inline bool startsWithNoCase(const std::string& s, const std::string& prefix)
{
    if (s.size() < prefix.size())
        return false;
    return lowerAscii(s.substr(0, prefix.size())) == lowerAscii(prefix);
}

inline bool containsNoCase(const std::string& hay, const std::string& needle)
{
    if (needle.empty())
        return true;
    return lowerAscii(hay).find(lowerAscii(needle)) != std::string::npos;
}

// An ASCII word has to stand alone ("art" is not in "start"); a Chinese term
// is a plain substring.
inline bool containsTerm(const std::string& hay, const std::string& term)
{
    if (term.empty())
        return false;
    if (!isAscii(term))
        return containsNoCase(hay, term);
    const std::string lower = lowerAscii(hay);
    const std::string word = lowerAscii(term);
    std::size_t at = lower.find(word);
    while (at != std::string::npos) {
        const bool before = at == 0 || !(std::isalnum(static_cast<unsigned char>(lower[at - 1])) || lower[at - 1] == '_');
        const std::size_t end = at + word.size();
        const bool after = end >= lower.size() || !(std::isalnum(static_cast<unsigned char>(lower[end])) || lower[end] == '_');
        if (before && after)
            return true;
        at = lower.find(word, at + 1);
    }
    return false;
}

// Where an ASCII term stands alone, or a Chinese term starts; npos when not.
inline std::size_t findTerm(const std::string& hay, const std::string& term)
{
    if (term.empty())
        return std::string::npos;
    if (!isAscii(term)) {
        const std::string lower = lowerAscii(hay);
        return lower.find(lowerAscii(term));
    }
    const std::string lower = lowerAscii(hay);
    const std::string word = lowerAscii(term);
    std::size_t at = lower.find(word);
    while (at != std::string::npos) {
        const bool before = at == 0 || !(std::isalnum(static_cast<unsigned char>(lower[at - 1])) || lower[at - 1] == '_');
        const std::size_t end = at + word.size();
        const bool after = end >= lower.size() || !(std::isalnum(static_cast<unsigned char>(lower[end])) || lower[end] == '_');
        if (before && after)
            return at;
        at = lower.find(word, at + 1);
    }
    return std::string::npos;
}

inline std::vector<std::string> splitAny(const std::string& s, const std::string& delimiters)
{
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at <= s.size()) {
        const std::size_t end = s.find_first_of(delimiters, at);
        const std::string part = trim(s.substr(at, end == std::string::npos ? std::string::npos : end - at));
        if (!part.empty())
            out.push_back(part);
        if (end == std::string::npos)
            break;
        at = end + 1;
    }
    return out;
}

inline std::string replaceAll(std::string s, const std::string& from, const std::string& to)
{
    if (from.empty())
        return s;
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
    return s;
}

// UTF-8 code points, for counting and cutting without splitting a character.
inline std::size_t utf8Length(const std::string& s)
{
    std::size_t n = 0;
    for (const unsigned char c : s) {
        if ((c & 0xC0) != 0x80)
            ++n;
    }
    return n;
}

// The byte after the code point that starts at `at`.
inline std::size_t utf8Next(const std::string& s, std::size_t at)
{
    if (at >= s.size())
        return s.size();
    ++at;
    while (at < s.size() && (static_cast<unsigned char>(s[at]) & 0xC0) == 0x80)
        ++at;
    return at;
}

inline std::string utf8Cut(const std::string& s, std::size_t codePoints, const std::string& ellipsis = "…")
{
    if (utf8Length(s) <= codePoints)
        return s;
    std::size_t n = 0;
    std::size_t at = 0;
    while (at < s.size() && n < codePoints) {
        ++at;
        while (at < s.size() && (static_cast<unsigned char>(s[at]) & 0xC0) == 0x80)
            ++at;
        ++n;
    }
    return s.substr(0, at) + ellipsis;
}

inline std::string utf8Prefix(const std::string& s, std::size_t codePoints)
{
    std::size_t n = 0;
    std::size_t at = 0;
    while (at < s.size() && n < codePoints) {
        ++at;
        while (at < s.size() && (static_cast<unsigned char>(s[at]) & 0xC0) == 0x80)
            ++at;
        ++n;
    }
    return s.substr(0, at);
}

// The text between two markers, or empty.
inline std::string between(const std::string& s, std::size_t from, std::size_t to)
{
    if (from == std::string::npos)
        return {};
    const std::size_t begin = from;
    const std::size_t end = to == std::string::npos ? s.size() : to;
    return end > begin ? s.substr(begin, end - begin) : std::string();
}

inline std::string basename(const std::string& path)
{
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

inline std::string dirname(const std::string& path)
{
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

inline std::string extensionOf(const std::string& path)
{
    const std::string name = basename(path);
    const std::size_t dot = name.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? std::string() : lowerAscii(name.substr(dot + 1));
}

} // namespace finder
