#pragma once

#include "common.h"
#include "planner.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>

namespace finder {

struct WeChatFile {
    std::string path;
    std::string name;
    long long modified = 0;
    long long size = 0;
};

inline bool isWeChatQuery(const std::string& text)
{
    const std::string lower = lowerAscii(text);
    return lower.find("微信") != std::string::npos || lower.find("wechat") != std::string::npos
        || lower.find("weixin") != std::string::npos;
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

inline bool skippedWeChatDir(const std::string& name)
{
    const std::string lower = lowerAscii(name);
    static const char* const blocked[] = {"cache", "customemotion", "emoji", "thumb", "temp", "backup",
        "config", "db", "sns", "favorite", "general", "avatar", "head", "sticker", "log", "crash"};
    for (const char* part : blocked) {
        if (lower == part)
            return true;
    }
    return false;
}

inline bool skippedWeChatExt(const std::string& ext)
{
    static const char* const blocked[] = {"dat", "db", "db-wal", "db-shm", "ini", "log", "tmp", "json",
        "xml", "db3", "sqlite", "sqlite3", "pdb", "lock"};
    for (const char* part : blocked) {
        if (ext == part)
            return true;
    }
    return false;
}

inline bool isTransferredPath(const std::string& lower)
{
    return lower.find("\\filestorage\\file\\") != std::string::npos
        || lower.find("\\filestorage\\video\\") != std::string::npos
        || lower.find("\\filestorage\\image\\") != std::string::npos
        || lower.find("\\msgattach\\") != std::string::npos
        || lower.find("\\msg\\file\\") != std::string::npos
        || lower.find("\\msg\\video\\") != std::string::npos
        || lower.find("\\msg\\attach\\") != std::string::npos;
}

inline void scanWeChat(const std::wstring& dir, std::vector<WeChatFile>& out, int depth = 0)
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
            if (!skippedWeChatDir(utf8Of(name)))
                scanWeChat(full, out, depth + 1);
            continue;
        }
        const std::string path = utf8Of(full);
        const std::string lower = lowerAscii(path);
        if (!isTransferredPath(lower))
            continue;
        const std::string ext = extensionOf(path);
        if (skippedWeChatExt(ext) || lower.find(".dat") != std::string::npos)
            continue;
        ULARGE_INTEGER size;
        size.LowPart = data.nFileSizeLow;
        size.HighPart = data.nFileSizeHigh;
        ULARGE_INTEGER modified;
        modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
        modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
        const long long epoch = static_cast<long long>((modified.QuadPart - 116444736000000000ULL) / 10000000ULL);
        out.push_back(WeChatFile {path, utf8Of(name), epoch, static_cast<long long>(size.QuadPart)});
    } while (::FindNextFileW(handle, &data));
    ::FindClose(handle);
}

inline std::vector<WeChatFile> allWeChatFiles(const std::string& rootsSpec)
{
    static std::mutex mutex;
    static std::string cachedRoots;
    static std::vector<WeChatFile> cache;
    static std::uint64_t cachedAt = 0;
    static bool cachedValid = false;
    const std::uint64_t now = ::GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex);
    if (cachedValid && cachedRoots == rootsSpec && now - cachedAt < 60000)
        return cache;

    std::vector<WeChatFile> files;
    std::string spec = rootsSpec;
    if (trim(spec).empty())
        spec = "%USERPROFILE%\\Documents\\WeChat Files;%USERPROFILE%\\Documents\\xwechat_files";
    for (const std::string& raw : splitAny(spec, ";")) {
        const std::string root = trim(raw);
        if (root.empty())
            continue;
        const std::wstring wide = expandPath(root);
        if (::GetFileAttributesW(wide.c_str()) == INVALID_FILE_ATTRIBUTES)
            continue;
        scanWeChat(wide, files);
    }
    cache = files;
    cachedRoots = rootsSpec;
    cachedAt = now;
    return cache;
}

inline std::vector<WeChatFile> findWeChatFiles(const Plan& plan, const std::string& rootsSpec, int limit)
{
    std::vector<WeChatFile> found;
    for (const WeChatFile& file : allWeChatFiles(rootsSpec)) {
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
                if (lower == "微信" || lower == "wechat" || lower == "weixin" || lower == "文件" || lower == "文件夹")
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
    std::sort(found.begin(), found.end(), [](const WeChatFile& a, const WeChatFile& b) { return a.modified > b.modified; });
    if (static_cast<int>(found.size()) > limit)
        found.resize(limit);
    return found;
}

} // namespace finder
