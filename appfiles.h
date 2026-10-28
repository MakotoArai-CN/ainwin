#pragma once

#include "common.h"
#include "planner.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <windows.h>

namespace finder {

struct AppFile {
    std::string path;
    std::string name;
    std::string source;
    long long modified = 0;
    long long size = 0;
};

inline bool isAppFileQuery(const std::string& text)
{
    const std::string lower = lowerAscii(text);
    static const char* const words[] = {"微信", "wechat", "weixin", "企业微信", "wxwork", "qq", "tencent files",
        "钉钉", "dingtalk", "飞书", "feishu", "lark", "wps", "金山"};
    for (const char* word : words) {
        if (lower.find(word) != std::string::npos)
            return true;
    }
    return false;
}

inline std::set<std::string> wantedSources(const std::string& text)
{
    const std::string lower = lowerAscii(text);
    std::set<std::string> out;
    if (lower.find("企业微信") != std::string::npos || lower.find("wxwork") != std::string::npos)
        out.insert("wxwork");
    else if (lower.find("微信") != std::string::npos || lower.find("wechat") != std::string::npos
        || lower.find("weixin") != std::string::npos)
        out.insert("wechat");
    if (lower.find("qq") != std::string::npos || lower.find("tencent files") != std::string::npos)
        out.insert("qq");
    if (lower.find("钉钉") != std::string::npos || lower.find("dingtalk") != std::string::npos)
        out.insert("dingtalk");
    if (lower.find("飞书") != std::string::npos || lower.find("feishu") != std::string::npos
        || lower.find("lark") != std::string::npos)
        out.insert("feishu");
    if (lower.find("wps") != std::string::npos || lower.find("金山") != std::string::npos)
        out.insert("wps");
    return out;
}

inline std::string sourceForPath(const std::string& path)
{
    const std::string lower = lowerAscii(path);
    if (lower.find("\\wxwork") != std::string::npos)
        return "wxwork";
    if (lower.find("\\tencent files") != std::string::npos)
        return "qq";
    if (lower.find("\\dingtalk") != std::string::npos)
        return "dingtalk";
    if (lower.find("\\feishu") != std::string::npos || lower.find("\\lark") != std::string::npos)
        return "feishu";
    if (lower.find("wps cloud files") != std::string::npos)
        return "wps";
    if (lower.find("\\wechat files") != std::string::npos || lower.find("\\xwechat_files") != std::string::npos)
        return "wechat";
    return {};
}

inline std::string sourceLabel(const std::string& source)
{
    if (source == "qq")
        return "QQ";
    if (source == "wxwork")
        return "企业微信";
    if (source == "dingtalk")
        return "钉钉";
    if (source == "feishu")
        return "飞书";
    if (source == "wps")
        return "WPS 云文档";
    return "微信";
}

inline std::wstring expandPath(const std::string& path)
{
    const std::wstring wide = widenOf(path);
    wchar_t buffer[32768];
    const DWORD n = ::ExpandEnvironmentStringsW(wide.c_str(), buffer, 32768);
    if (n > 0 && n < 32768)
        return std::wstring(buffer);
    return wide;
}

inline bool skippedAppDir(const std::string& name, const std::string& source)
{
    const std::string lower = lowerAscii(name);
    static const char* const blocked[] = {"cache", "customemotion", "emoji", "thumb", "temp", "backup",
        "config", "db", "sns", "favorite", "general", "avatar", "head", "sticker", "log", "logs",
        "crash", "nt_db", "nt_temp", "mmkv", "msf", "online status", "unitedconfig", "hyperionlocalcache"};
    for (const char* part : blocked) {
        if (lower == part) {
            if (lower == "cache" && (source == "wxwork" || source == "dingtalk" || source == "feishu"))
                return false;
            return true;
        }
    }
    return false;
}

inline bool skippedAppExt(const std::string& ext)
{
    static const char* const blocked[] = {"dat", "db", "db-wal", "db-shm", "ini", "log", "tmp", "json",
        "xml", "db3", "sqlite", "sqlite3", "pdb", "lock", "data"};
    for (const char* part : blocked) {
        if (ext == part)
            return true;
    }
    return false;
}

inline bool isTransferredPath(const std::string& lower, const std::string& source)
{
    if (source == "wechat") {
        return lower.find("\\filestorage\\file\\") != std::string::npos
            || lower.find("\\filestorage\\video\\") != std::string::npos
            || lower.find("\\filestorage\\image\\") != std::string::npos
            || lower.find("\\msgattach\\") != std::string::npos
            || lower.find("\\msg\\file\\") != std::string::npos
            || lower.find("\\msg\\video\\") != std::string::npos
            || lower.find("\\msg\\attach\\") != std::string::npos;
    }
    if (source == "qq") {
        return lower.find("\\filerecv\\") != std::string::npos
            || lower.find("\\nt_data\\file\\") != std::string::npos
            || lower.find("\\nt_data\\pic\\") != std::string::npos
            || lower.find("\\nt_data\\video\\") != std::string::npos
            || lower.find("\\image\\") != std::string::npos
            || lower.find("\\video\\") != std::string::npos;
    }
    if (source == "wxwork" || source == "dingtalk" || source == "feishu") {
        return lower.find("\\file\\") != std::string::npos
            || lower.find("\\image\\") != std::string::npos
            || lower.find("\\video\\") != std::string::npos
            || lower.find("\\recv\\") != std::string::npos;
    }
    if (source == "wps")
        return true;
    return false;
}

inline void scanAppRoot(const std::wstring& dir, const std::string& source, std::vector<AppFile>& out, int depth = 0)
{
    if (depth > 12)
        return;
    WIN32_FIND_DATAW data;
    const std::wstring pattern = dir + L"\\*";
    HANDLE handle = ::FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE)
        return;
    do {
        const std::wstring name = data.cFileName;
        if (name == L"." || name == L"..")
            continue;
        const std::wstring full = dir + L"\\" + name;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!skippedAppDir(utf8Of(name), source))
                scanAppRoot(full, source, out, depth + 1);
            continue;
        }
        const std::string path = utf8Of(full);
        const std::string lower = lowerAscii(path);
        if (!isTransferredPath(lower, source))
            continue;
        const std::string ext = extensionOf(path);
        if (skippedAppExt(ext) || lower.find(".dat") != std::string::npos)
            continue;
        ULARGE_INTEGER size;
        size.LowPart = data.nFileSizeLow;
        size.HighPart = data.nFileSizeHigh;
        ULARGE_INTEGER modified;
        modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
        modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
        const long long epoch = static_cast<long long>((modified.QuadPart - 116444736000000000ULL) / 10000000ULL);
        out.push_back(AppFile {path, utf8Of(name), source, epoch, static_cast<long long>(size.QuadPart)});
    } while (::FindNextFileW(handle, &data));
    ::FindClose(handle);
}

inline std::vector<std::string> defaultAppRoots()
{
    return {
        "%USERPROFILE%\\Documents\\WeChat Files",
        "%USERPROFILE%\\Documents\\xwechat_files",
        "%USERPROFILE%\\Documents\\Tencent Files",
        "%USERPROFILE%\\Documents\\WXWork",
        "%USERPROFILE%\\Documents\\DingTalk",
        "%USERPROFILE%\\Documents\\Feishu",
        "%USERPROFILE%\\Documents\\Lark",
        "%USERPROFILE%\\WPS Cloud Files",
        "%USERPROFILE%\\Documents\\WPS Cloud Files",
    };
}

inline std::vector<AppFile> allAppFiles(const std::string& rootsSpec)
{
    static std::mutex mutex;
    static std::string cachedRoots;
    static std::vector<AppFile> cache;
    static std::uint64_t cachedAt = 0;
    static bool cachedValid = false;
    const std::uint64_t now = ::GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex);
    if (cachedValid && cachedRoots == rootsSpec && now - cachedAt < 60000)
        return cache;

    std::vector<std::string> roots;
    if (!trim(rootsSpec).empty()) {
        for (const std::string& raw : splitAny(rootsSpec, ";")) {
            const std::string root = trim(raw);
            if (!root.empty())
                roots.push_back(root);
        }
    } else {
        roots = defaultAppRoots();
    }

    std::vector<AppFile> files;
    for (const std::string& root : roots) {
        const std::wstring wide = expandPath(root);
        if (::GetFileAttributesW(wide.c_str()) == INVALID_FILE_ATTRIBUTES)
            continue;
        const std::string source = sourceForPath(root);
        if (source.empty())
            continue;
        scanAppRoot(wide, source, files);
    }
    cache = files;
    cachedRoots = rootsSpec;
    cachedAt = now;
    cachedValid = true;
    return cache;
}

inline std::vector<AppFile> findAppFiles(const Plan& plan, const std::string& rootsSpec, int limit)
{
    const std::set<std::string> wanted = wantedSources(plan.text);
    std::vector<AppFile> found;
    for (const AppFile& file : allAppFiles(rootsSpec)) {
        if (!wanted.empty() && wanted.count(file.source) == 0)
            continue;
        if (plan.time.set && (file.modified < plan.time.from || file.modified >= plan.time.to))
            continue;
        if (plan.type.set && !plan.type.folders && !plan.type.extensions.empty()) {
            const std::string ext = extensionOf(file.path);
            if (std::find(plan.type.extensions.begin(), plan.type.extensions.end(), ext) == plan.type.extensions.end())
                continue;
        }
        const std::string name = lowerAscii(file.name);
        bool excluded = false;
        for (const std::string& term : plan.exclude) {
            if (name.find(lowerAscii(term)) != std::string::npos) {
                excluded = true;
                break;
            }
        }
        if (excluded)
            continue;
        std::vector<std::string> words;
        if (!plan.type.set) {
            for (const std::string& term : plan.terms) {
                const std::string lower = lowerAscii(term);
                if (lower == "微信" || lower == "wechat" || lower == "weixin" || lower == "qq"
                    || lower == "企业微信" || lower == "wxwork" || lower == "钉钉" || lower == "dingtalk"
                    || lower == "飞书" || lower == "feishu" || lower == "lark" || lower == "wps"
                    || lower == "文件" || lower == "文件夹")
                    continue;
                if (utf8Length(term) >= 2)
                    words.push_back(lower);
            }
        }
        if (!words.empty()) {
            bool any = false;
            for (const std::string& word : words) {
                if (name.find(word) != std::string::npos) {
                    any = true;
                    break;
                }
            }
            if (!any)
                continue;
        }
        found.push_back(file);
    }
    std::sort(found.begin(), found.end(), [](const AppFile& a, const AppFile& b) { return a.modified > b.modified; });
    if (static_cast<int>(found.size()) > limit)
        found.resize(limit);
    return found;
}

} // namespace finder
