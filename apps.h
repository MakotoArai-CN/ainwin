#pragma once

#include "common.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <windows.h>

namespace finder {

struct AppInfo {
    const char* title;
    const char* aliases;
    const char* category;
    const char* target;
    bool builtin;
};

struct InstalledApp {
    std::string title;
    std::string path;
    std::string category;
    std::string aliases;
    bool builtin = false;
};

struct AppHit {
    std::string title;
    std::string target;
    std::string subtitle;
    int score = 0;
};

inline const std::vector<AppInfo>& appCatalog()
{
    // Installed programs are discovered from the Start Menu; this table adds
    // Chinese/English aliases and function categories. target is only set for
    // Windows tools that are always present.
    static const std::vector<AppInfo> all = {
        {"微信", "微信,wechat,weixin,聊天,即时通讯", "聊天", "", false},
        {"企业微信", "企业微信,wecom,work wechat", "聊天", "", false},
        {"QQ", "qq,tim,腾讯qq,聊天", "聊天", "", false},
        {"钉钉", "钉钉,dingtalk,聊天,办公", "聊天", "", false},
        {"飞书", "飞书,feishu,lark,聊天,办公", "聊天", "", false},
        {"Telegram", "telegram,电报,聊天", "聊天", "", false},
        {"WhatsApp", "whatsapp,聊天", "聊天", "", false},
        {"Discord", "discord,聊天", "聊天", "", false},
        {"Slack", "slack,聊天", "聊天", "", false},
        {"腾讯会议", "腾讯会议,tencent meeting,会议", "会议", "", false},
        {"Zoom", "zoom,会议", "会议", "", false},
        {"Microsoft Teams", "teams,microsoft teams,会议,聊天", "会议", "", false},
        {"Google Chrome", "chrome,google chrome,谷歌浏览器,浏览器", "浏览器", "", false},
        {"Microsoft Edge", "edge,microsoft edge,浏览器", "浏览器", "", false},
        {"Mozilla Firefox", "firefox,火狐,浏览器", "浏览器", "", false},
        {"360浏览器", "360浏览器,360se,浏览器", "浏览器", "", false},
        {"QQ浏览器", "qq浏览器,qqbrowser,浏览器", "浏览器", "", false},
        {"搜狗浏览器", "搜狗浏览器,sogou explorer,浏览器", "浏览器", "", false},
        {"Brave", "brave,浏览器", "浏览器", "", false},
        {"Opera", "opera,浏览器", "浏览器", "", false},
        {"Vivaldi", "vivaldi,浏览器", "浏览器", "", false},
        {"Microsoft Word", "word,微软word,文档,办公", "办公", "", false},
        {"Microsoft Excel", "excel,表格,办公", "办公", "", false},
        {"Microsoft PowerPoint", "powerpoint,ppt,幻灯片,办公", "办公", "", false},
        {"Microsoft Outlook", "outlook,邮件,办公", "办公", "", false},
        {"Microsoft OneNote", "onenote,笔记,办公", "办公", "", false},
        {"WPS Office", "wps,wps office,金山办公,办公", "办公", "", false},
        {"金山文档", "金山文档,办公,文档", "办公", "", false},
        {"腾讯文档", "腾讯文档,文档,办公", "办公", "", false},
        {"石墨文档", "石墨文档,文档,办公", "办公", "", false},
        {"印象笔记", "印象笔记,evernote,笔记", "办公", "", false},
        {"Notion", "notion,笔记", "办公", "", false},
        {"Obsidian", "obsidian,笔记", "办公", "", false},
        {"Typora", "typora,markdown,笔记", "办公", "", false},
        {"语雀", "语雀,yuque,文档,笔记", "办公", "", false},
        {"Adobe Acrobat", "acrobat,adobe acrobat,pdf", "办公", "", false},
        {"福昕阅读器", "福昕,foxit,pdf", "办公", "", false},
        {"ONLYOFFICE", "onlyoffice,办公,文档", "办公", "", false},
        {"LibreOffice", "libreoffice,办公,文档", "办公", "", false},
        {"Visual Studio Code", "vs code,vscode,visual studio code,代码,编辑器", "开发", "", false},
        {"Visual Studio", "visual studio,vs,开发,ide", "开发", "", false},
        {"IntelliJ IDEA", "intellij idea,idea,开发,ide", "开发", "", false},
        {"PyCharm", "pycharm,开发,ide,python", "开发", "", false},
        {"WebStorm", "webstorm,开发,ide", "开发", "", false},
        {"CLion", "clion,开发,ide", "开发", "", false},
        {"GoLand", "goland,开发,ide", "开发", "", false},
        {"Android Studio", "android studio,安卓开发,开发,ide", "开发", "", false},
        {"Eclipse", "eclipse,开发,ide,java", "开发", "", false},
        {"Sublime Text", "sublime,sublime text,编辑器", "开发", "", false},
        {"Notepad++", "notepad++,记事本++,编辑器", "开发", "", false},
        {"Vim", "vim,编辑器", "开发", "", false},
        {"Neovim", "neovim,nvim,编辑器", "开发", "", false},
        {"Git Bash", "git bash,git,命令行", "开发", "", false},
        {"GitHub Desktop", "github desktop,git,开发", "开发", "", false},
        {"SourceTree", "sourcetree,git,开发", "开发", "", false},
        {"Postman", "postman,接口调试,api", "开发", "", false},
        {"Apifox", "apifox,接口调试,api", "开发", "", false},
        {"Docker Desktop", "docker,docker desktop,容器", "开发", "", false},
        {"Xshell", "xshell,ssh,终端", "开发", "", false},
        {"Xftp", "xftp,sftp,ftp", "开发", "", false},
        {"PuTTY", "putty,ssh,终端", "开发", "", false},
        {"MobaXterm", "mobaxterm,ssh,终端", "开发", "", false},
        {"SecureCRT", "securecrt,ssh,终端", "开发", "", false},
        {"WinSCP", "winscp,sftp,ftp", "开发", "", false},
        {"FileZilla", "filezilla,ftp", "开发", "", false},
        {"Navicat", "navicat,数据库,mysql", "数据库", "", false},
        {"DBeaver", "dbeaver,数据库,sql", "数据库", "", false},
        {"Redis Desktop Manager", "redis desktop,redis,数据库", "数据库", "", false},
        {"MySQL Workbench", "mysql workbench,mysql,数据库", "数据库", "", false},
        {"SQL Server Management Studio", "ssms,sql server,数据库", "数据库", "", false},
        {"Python", "python,idle,编程", "开发", "", false},
        {"Anaconda", "anaconda,conda,python", "开发", "", false},
        {"Jupyter", "jupyter,notebook,python", "开发", "", false},
        {"Node.js", "node,nodejs,node.js,javascript", "开发", "", false},
        {"CMake", "cmake,构建", "开发", "", false},
        {"Qt", "qt,qt creator,开发", "开发", "", false},
        {"Unity", "unity,游戏开发,3d", "开发", "", false},
        {"Unreal Engine", "unreal,unreal engine,ue,虚幻,游戏开发", "开发", "", false},
        {"Blender", "blender,3d,建模", "设计", "", false},
        {"MATLAB", "matlab,数学,仿真", "开发", "", false},
        {"RStudio", "rstudio,r语言,统计", "开发", "", false},
        {"Arduino IDE", "arduino,单片机", "开发", "", false},
        {"Keil", "keil,单片机,嵌入式", "开发", "", false},
        {"STM32CubeMX", "stm32cubemx,stm32,嵌入式", "开发", "", false},
        {"Altium Designer", "altium,ad,pcb,电路", "设计", "", false},
        {"AutoCAD", "autocad,cad,图纸", "设计", "", false},
        {"SolidWorks", "solidworks,sw,机械,3d", "设计", "", false},
        {"FreeCAD", "freecad,cad,3d", "设计", "", false},
        {"KiCad", "kicad,pcb,电路", "设计", "", false},
        {"网易云音乐", "网易云音乐,netease cloud music,音乐", "音频", "", false},
        {"QQ音乐", "qq音乐,qq music,音乐", "音频", "", false},
        {"酷狗音乐", "酷狗,酷狗音乐,kugou,音乐", "音频", "", false},
        {"酷我音乐", "酷我,酷我音乐,kuwo,音乐", "音频", "", false},
        {"Spotify", "spotify,音乐", "音频", "", false},
        {"PotPlayer", "potplayer,播放器,视频", "视频", "", false},
        {"VLC", "vlc,vlc media player,播放器,视频", "视频", "", false},
        {"MPV", "mpv,播放器", "视频", "", false},
        {"剪映", "剪映,jianying,capcut,视频剪辑", "视频", "", false},
        {"Adobe Premiere Pro", "premiere,pr,视频剪辑", "视频", "", false},
        {"Adobe After Effects", "after effects,ae,视频特效", "视频", "", false},
        {"DaVinci Resolve", "davinci,davinci resolve,达芬奇,视频剪辑", "视频", "", false},
        {"OBS Studio", "obs,obs studio,录屏,直播", "视频", "", false},
        {"Audacity", "audacity,音频,录音", "音频", "", false},
        {"格式工厂", "格式工厂,format factory,转码", "视频", "", false},
        {"HandBrake", "handbrake,转码,视频", "视频", "", false},
        {"foobar2000", "foobar2000,音乐,播放器", "音频", "", false},
        {"Adobe Photoshop", "photoshop,ps,修图,设计", "设计", "", false},
        {"Adobe Illustrator", "illustrator,ai,矢量,设计", "设计", "", false},
        {"Adobe Lightroom", "lightroom,lr,照片,修图", "设计", "", false},
        {"Adobe InDesign", "indesign,id,排版,设计", "设计", "", false},
        {"Figma", "figma,设计,ui", "设计", "", false},
        {"Sketch", "sketch,设计,ui", "设计", "", false},
        {"Adobe XD", "xd,adobe xd,设计,ui", "设计", "", false},
        {"CorelDRAW", "coreldraw,cdr,设计", "设计", "", false},
        {"3ds Max", "3ds max,3d,建模", "设计", "", false},
        {"Maya", "maya,3d,建模", "设计", "", false},
        {"Cinema 4D", "cinema 4d,c4d,3d", "设计", "", false},
        {"ZBrush", "zbrush,3d,雕刻", "设计", "", false},
        {"7-Zip", "7-zip,7zip,压缩,解压", "压缩", "", false},
        {"WinRAR", "winrar,rar,压缩,解压", "压缩", "", false},
        {"Bandizip", "bandizip,压缩,解压", "压缩", "", false},
        {"360压缩", "360压缩,压缩,解压", "压缩", "", false},
        {"好压", "好压,2345好压,压缩", "压缩", "", false},
        {"PeaZip", "peazip,压缩,解压", "压缩", "", false},
        {"WinZip", "winzip,压缩,解压", "压缩", "", false},
        {"迅雷", "迅雷,xunlei,下载,bt", "下载", "", false},
        {"IDM", "idm,internet download manager,下载", "下载", "", false},
        {"Neat Download Manager", "neat download manager,ndm,下载", "下载", "", false},
        {"qBittorrent", "qbittorrent,bt,种子,下载", "下载", "", false},
        {"比特彗星", "比特彗星,bitcomet,bt,种子", "下载", "", false},
        {"Free Download Manager", "free download manager,fdm,下载", "下载", "", false},
        {"百度网盘", "百度网盘,baidu netdisk,网盘,云盘", "网盘", "", false},
        {"阿里云盘", "阿里云盘,aliyun drive,网盘,云盘", "网盘", "", false},
        {"夸克网盘", "夸克网盘,quark,网盘", "网盘", "", false},
        {"天翼云盘", "天翼云盘,189云盘,网盘", "网盘", "", false},
        {"OneDrive", "onedrive,网盘,云盘", "网盘", "", false},
        {"Google Drive", "google drive,网盘,云盘", "网盘", "", false},
        {"Dropbox", "dropbox,网盘,云盘", "网盘", "", false},
        {"火绒安全", "火绒,火绒安全,杀毒,安全", "安全", "", false},
        {"360安全卫士", "360安全卫士,360,杀毒,安全", "安全", "", false},
        {"腾讯电脑管家", "腾讯电脑管家,电脑管家,杀毒,安全", "安全", "", false},
        {"金山毒霸", "金山毒霸,毒霸,杀毒,安全", "安全", "", false},
        {"Malwarebytes", "malwarebytes,杀毒,安全", "安全", "", false},
        {"搜狗输入法", "搜狗输入法,sogou input,输入法", "输入法", "", false},
        {"微软拼音", "微软拼音,microsoft pinyin,输入法", "输入法", "", false},
        {"QQ输入法", "qq输入法,输入法", "输入法", "", false},
        {"百度输入法", "百度输入法,baidu input,输入法", "输入法", "", false},
        {"讯飞输入法", "讯飞输入法,iflytek,输入法", "输入法", "", false},
        {"小狼毫", "小狼毫,rime,weasel,输入法", "输入法", "", false},
        {"向日葵", "向日葵,sunlogin,远程控制", "远程", "", false},
        {"ToDesk", "todesk,远程控制", "远程", "", false},
        {"AnyDesk", "anydesk,远程控制", "远程", "", false},
        {"TeamViewer", "teamviewer,远程控制", "远程", "", false},
        {"Parsec", "parsec,远程控制,串流", "远程", "", false},
        {"RustDesk", "rustdesk,远程控制", "远程", "", false},
        {"Everything", "everything,文件搜索", "系统工具", "", false},
        {"Listary", "listary,文件搜索", "系统工具", "", false},
        {"PowerToys", "powertoys,微软工具箱", "系统工具", "", false},
        {"Snipaste", "snipaste,截图,贴图", "截图", "", false},
        {"PixPin", "pixpin,截图,贴图", "截图", "", false},
        {"Steam", "steam,游戏", "游戏", "", false},
        {"Epic Games", "epic,epic games,游戏", "游戏", "", false},
        {"WeGame", "wegame,腾讯游戏,游戏", "游戏", "", false},
        {"暴雪战网", "暴雪战网,battlenet,战网,游戏", "游戏", "", false},
        {"EA App", "ea app,ea,origin,游戏", "游戏", "", false},
        {"Ubisoft Connect", "ubisoft,uplay,游戏", "游戏", "", false},
        {"Xbox", "xbox,游戏", "游戏", "", false},
        {"原神", "原神,genshin,游戏", "游戏", "", false},
        {"崩坏：星穹铁道", "崩坏,星穹铁道,star rail,游戏", "游戏", "", false},
        {"英雄联盟", "英雄联盟,lol,league of legends,游戏", "游戏", "", false},
        {"穿越火线", "穿越火线,cf,crossfire,游戏", "游戏", "", false},
        {"微信开发者工具", "微信开发者工具,wechat devtools,开发", "开发", "", false},
        {"计算器", "计算器,calc,calculator", "系统工具", "C:\\Windows\\System32\\calc.exe", true},
        {"记事本", "记事本,notepad", "系统工具", "C:\\Windows\\System32\\notepad.exe", true},
        {"画图", "画图,paint,mspaint", "系统工具", "C:\\Windows\\System32\\mspaint.exe", true},
        {"截图工具", "截图工具,截图,截屏,snipping tool,snippingtool", "截图", "C:\\Windows\\System32\\SnippingTool.exe", true},
        {"任务管理器", "任务管理器,task manager,taskmgr", "系统工具", "C:\\Windows\\System32\\Taskmgr.exe", true},
        {"控制面板", "控制面板,control panel,control", "系统工具", "C:\\Windows\\System32\\control.exe", true},
        {"文件资源管理器", "文件资源管理器,资源管理器,file explorer,explorer", "系统工具", "C:\\Windows\\explorer.exe", true},
        {"命令提示符", "命令提示符,命令行,cmd,command prompt", "系统工具", "C:\\Windows\\System32\\cmd.exe", true},
        {"PowerShell", "powershell,pwsh", "系统工具", "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", true},
        {"服务", "服务,services,services.msc", "系统工具", "C:\\Windows\\System32\\services.msc", true},
        {"设备管理器", "设备管理器,驱动,device manager,devmgmt", "系统工具", "C:\\Windows\\System32\\devmgmt.msc", true},
        {"注册表编辑器", "注册表,注册表编辑器,regedit,registry", "系统工具", "C:\\Windows\\regedit.exe", true},
        {"系统信息", "系统信息,硬件信息,msinfo32,system information", "系统工具", "C:\\Windows\\System32\\msinfo32.exe", true},
        {"磁盘清理", "磁盘清理,cleanmgr,disk cleanup", "系统工具", "C:\\Windows\\System32\\cleanmgr.exe", true},
        {"磁盘管理", "磁盘管理,diskmgmt,disk management", "系统工具", "C:\\Windows\\System32\\diskmgmt.msc", true},
        {"事件查看器", "事件查看器,event viewer,eventvwr", "系统工具", "C:\\Windows\\System32\\eventvwr.exe", true},
        {"性能监视器", "性能监视器,performance monitor,perfmon", "系统工具", "C:\\Windows\\System32\\perfmon.exe", true},
        {"字符映射表", "字符映射表,charmap", "系统工具", "C:\\Windows\\System32\\charmap.exe", true},
        {"放大镜", "放大镜,magnify", "系统工具", "C:\\Windows\\System32\\Magnify.exe", true},
        {"屏幕键盘", "屏幕键盘,on-screen keyboard,osk", "系统工具", "C:\\Windows\\System32\\osk.exe", true},
        {"讲述人", "讲述人,narrator", "系统工具", "C:\\Windows\\System32\\Narrator.exe", true},
        {"远程桌面连接", "远程桌面,远程桌面连接,remote desktop,mstsc", "远程", "C:\\Windows\\System32\\mstsc.exe", true},
    };
    return all;
}

inline const std::vector<std::pair<const char*, const char*>>& appCategories()
{
    static const std::vector<std::pair<const char*, const char*>> all = {
        {"浏览器", "浏览器,网页,上网,浏览网页,browser"},
        {"聊天", "聊天,即时通讯,通讯,社交,chat"},
        {"办公", "办公,文档,office"},
        {"开发", "开发,编程,代码,ide,程序员"},
        {"设计", "设计,修图,作图,建模,3d"},
        {"视频", "视频,剪辑,视频编辑,转码"},
        {"音频", "音频,音乐,录音,播放器"},
        {"压缩", "压缩,解压,压缩包,压缩软件"},
        {"下载", "下载,bt,种子,下载工具"},
        {"安全", "安全,杀毒,防护,杀毒软件"},
        {"输入法", "输入法,打字"},
        {"网盘", "网盘,云盘"},
        {"远程", "远程,远程控制"},
        {"数据库", "数据库,sql"},
        {"游戏", "游戏"},
        {"会议", "会议,视频会议"},
        {"截图", "截图,截屏,贴图"},
        {"系统工具", "系统,工具,系统工具"},
    };
    return all;
}

inline std::string appNameFromFile(const std::string& name)
{
    std::string title = name;
    const std::size_t dot = title.find_last_of('.');
    if (dot != std::string::npos)
        title.erase(dot);
    const std::string suffix = " - 快捷方式";
    if (title.size() > suffix.size() && title.compare(title.size() - suffix.size(), suffix.size(), suffix) == 0)
        title.erase(title.size() - suffix.size());
    return trim(title);
}

inline bool containsToken(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return false;
    const bool ascii = std::all_of(needle.begin(), needle.end(), [](unsigned char c) { return c < 0x80; });
    if (!ascii)
        return haystack.find(needle) != std::string::npos;
    std::size_t at = haystack.find(needle);
    while (at != std::string::npos) {
        const char before = at == 0 ? ' ' : haystack[at - 1];
        const char after = at + needle.size() >= haystack.size() ? ' ' : haystack[at + needle.size()];
        const bool left = !std::isalnum(static_cast<unsigned char>(before)) && before != '_';
        const bool right = !std::isalnum(static_cast<unsigned char>(after)) && after != '_';
        if (left && right)
            return true;
        at = haystack.find(needle, at + 1);
    }
    return false;
}

inline bool skipAppName(const std::string& title)
{
    static const char* const skip[] = {"卸载", "uninstall", "帮助", "help", "readme", "documentation",
        "manual", "release notes", "faq", "website", "web site", "on the web", "support", "wiki", "license", "sample",
        "update", "check for updates", "configure", "settings", "add to archive", "extract", "open as archive",
        "新建 pdf", "新建表格", "新建幻灯片", "新建文档", "administrative tools", "system tools", "accessories",
        "log file", "report", "diagnostics", "uninstaller", "module docs", "docs", "语言首选项",
        "language preferences", "office 工具"};
    const std::string lower = lowerAscii(title);
    for (const char* part : skip) {
        if (lower.find(part) != std::string::npos)
            return true;
    }
    return title.empty();
}

inline void scanStartMenu(const std::wstring& dir, std::vector<InstalledApp>& out)
{
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
            scanStartMenu(full, out);
            continue;
        }
        const std::string path = utf8Of(full);
        const std::string lower = lowerAscii(path);
        if (lower.size() < 4)
            continue;
        const bool shortcut = (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".lnk") == 0)
            || (lower.size() >= 9 && lower.compare(lower.size() - 9, 9, ".appref-ms") == 0);
        if (!shortcut)
            continue;
        const std::string title = appNameFromFile(utf8Of(name));
        if (skipAppName(title))
            continue;
        out.push_back(InstalledApp {title, path, "", "", false});
    } while (::FindNextFileW(handle, &data));
    ::FindClose(handle);
}

inline std::vector<InstalledApp> installedApps()
{
    static std::mutex mutex;
    static std::vector<InstalledApp> cache;
    static std::uint64_t cachedAt = 0;
    const std::uint64_t now = ::GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex);
    if (cachedAt != 0 && now - cachedAt < 300000)
        return cache;

    std::vector<InstalledApp> apps;
    std::vector<std::wstring> roots;
    wchar_t buffer[32768];
    if (::GetEnvironmentVariableW(L"APPDATA", buffer, 32768) > 0)
        roots.push_back(std::wstring(buffer) + L"\\Microsoft\\Windows\\Start Menu\\Programs");
    if (::GetEnvironmentVariableW(L"PROGRAMDATA", buffer, 32768) > 0)
        roots.push_back(std::wstring(buffer) + L"\\Microsoft\\Windows\\Start Menu\\Programs");
    for (const std::wstring& root : roots)
        scanStartMenu(root, apps);

    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) { return lowerAscii(a.title) < lowerAscii(b.title); });
    std::set<std::string> seen;
    std::vector<InstalledApp> unique;
    for (InstalledApp& app : apps) {
        const std::string key = lowerAscii(app.title);
        if (key.empty() || !seen.insert(key).second)
            continue;
        const AppInfo* info = nullptr;
        std::size_t best = 0;
        for (const AppInfo& candidate : appCatalog()) {
            if (candidate.builtin)
                continue;
            for (const std::string& alias : splitAny(candidate.aliases, ",")) {
                const std::string needle = lowerAscii(alias);
                if (needle.size() >= 2 && (key == needle || (needle.size() >= 4 && containsToken(key, needle)))
                    && needle.size() > best) {
                    info = &candidate;
                    best = needle.size();
                }
            }
        }
        if (info) {
            app.aliases = info->aliases;
            app.category = info->category;
        }
        unique.push_back(std::move(app));
    }
    cache = unique;
    cachedAt = now;
    return cache;
}

inline int appScore(const std::string& query, const std::string& title, const std::string& aliases,
    const std::string& category, bool builtin)
{
    int best = 0;
    const auto scoreText = [&](const std::string& value) {
        if (value.empty())
            return 0;
        if (value == query) return 1000;
        if (containsToken(value, query)) return 650;
        if (containsToken(query, value) && value.size() >= 2) return 600;
        return 0;
    };
    best = std::max(best, scoreText(lowerAscii(title)));
    for (const std::string& alias : splitAny(aliases, ",")) {
        const std::string value = lowerAscii(alias);
        if (value == query)
            best = std::max(best, 950);
        else if (value.rfind(query, 0) == 0)
            best = std::max(best, 780);
        else if (containsToken(value, query))
            best = std::max(best, 640);
        else if (containsToken(query, value) && value.size() >= 2)
            best = std::max(best, 590);
    }
    for (const auto& categoryAlias : appCategories()) {
        if (category != categoryAlias.first)
            continue;
        for (const std::string& alias : splitAny(categoryAlias.second, ",")) {
            const std::string value = lowerAscii(alias);
            if (value == query)
                best = std::max(best, 500);
            else if (query.find(value) != std::string::npos && value.size() >= 2)
                best = std::max(best, 450);
            else if (value.find(query) != std::string::npos && query.size() >= 2)
                best = std::max(best, 400);
        }
    }
    if (best <= 0)
        return 0;
    if (builtin)
        best += 20;
    return best;
}

inline std::string appQuery(const std::string& text)
{
    std::string query = lowerAscii(trim(text));
    static const char* const prefixes[] = {"帮我找", "帮我", "找一下", "找", "搜索", "查", "打开", "启动", "运行"};
    for (const char* prefix : prefixes) {
        if (query.rfind(prefix, 0) == 0) {
            query = trim(query.substr(std::strlen(prefix)));
            break;
        }
    }
    static const char* const suffixes[] = {"软件", "应用", "程序", "客户端", "工具", "一下"};
    for (const char* suffix : suffixes) {
        const std::string value(suffix);
        if (query.size() > value.size() && query.compare(query.size() - value.size(), value.size(), value) == 0)
            query.erase(query.size() - value.size());
    }
    return trim(query);
}

inline std::vector<AppHit> matchApps(const std::string& text, int limit = 5)
{
    const std::string query = appQuery(text);
    if (query.size() < 2)
        return {};
    std::vector<AppHit> hits;
    std::set<std::string> seen;

    for (const InstalledApp& app : installedApps()) {
        const int score = appScore(query, app.title, app.aliases, app.category, false);
        if (score <= 0)
            continue;
        const std::string key = lowerAscii(app.title);
        if (!seen.insert(key).second)
            continue;
        hits.push_back(AppHit {app.title, app.path, "应用 · 开始菜单" + (app.category.empty() ? std::string() : " · " + app.category), score});
    }

    for (const AppInfo& app : appCatalog()) {
        if (!app.builtin || !app.target || !*app.target)
            continue;
        const std::string key = lowerAscii(app.title);
        if (seen.count(key))
            continue;
        bool shadowed = false;
        for (const AppHit& hit : hits) {
            const std::string title = lowerAscii(hit.title);
            for (const std::string& alias : splitAny(app.aliases, ",")) {
                const std::string value = lowerAscii(alias);
                if (value.size() >= 2 && containsToken(title, value)) {
                    shadowed = true;
                    break;
                }
            }
            if (shadowed)
                break;
        }
        if (shadowed)
            continue;
        const int score = appScore(query, app.title, app.aliases, app.category, true);
        if (score <= 0)
            continue;
        seen.insert(key);
        hits.push_back(AppHit {app.title, app.target,
            "应用 · Windows 内置" + (app.category && *app.category ? " · " + std::string(app.category) : std::string()), score});
    }

    std::sort(hits.begin(), hits.end(), [](const AppHit& a, const AppHit& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.title.size() < b.title.size();
    });
    if (static_cast<int>(hits.size()) > limit)
        hits.resize(limit);
    return hits;
}

} // namespace finder
