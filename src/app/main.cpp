// Win顺 常驻插件：按描述找本机文件。
//
// 查询只做一件事：把一句话拆成概念、时间、类型、位置和大小条件，再调用
// Win顺 自己的索引（search_files / search_content）。它不遍历磁盘、不打开
// 文件、不联网、不读剪贴板，也不写任何文件。Enter 打开结果由 Win顺 执行。
//
// 协议：stdio 上 JSON-RPC 2.0，一行一条；stdout 只有协议消息，诊断走 stderr。

#include "winshun.h"

#include "common.h"
#include "embedder.h"
#include "lexicon.h"
#include "planner.h"
#include "settings.h"
#include "appfiles.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using finder::Plan;
using winshun::Item;
using winshun::Items;
using winshun::Json;

namespace {

constexpr int kContentLimit = 40;
constexpr int kNameLimit = 100;
constexpr int kContentTimeoutMs = 1200;
constexpr long long kCacheSeconds = 60;

// ---- small text helpers -------------------------------------------------------

std::string join(const std::vector<std::string>& parts, const std::string& separator)
{
    std::string out;
    for (const std::string& part : parts) {
        if (part.empty())
            continue;
        if (!out.empty())
            out += separator;
        out += part;
    }
    return out;
}

std::string lowerKey(const std::string& path)
{
    return finder::lowerAscii(path);
}

std::string trimSnippet(const std::string& text, std::size_t codePoints)
{
    std::string s = finder::trim(text);
    s = finder::replaceAll(s, "\r", " ");
    s = finder::replaceAll(s, "\n", " ");
    s = finder::replaceAll(s, "\t", " ");
    while (s.find("  ") != std::string::npos)
        s = finder::replaceAll(s, "  ", " ");
    return finder::utf8Cut(s, codePoints);
}

std::string quoteWord(const std::string& word)
{
    if (word.find(' ') == std::string::npos && word.find('"') == std::string::npos)
        return word;
    return '"' + finder::replaceAll(word, "\"", "") + '"';
}

std::string humanSize(long long bytes)
{
    if (bytes < 0)
        return {};
    if (bytes < 1024)
        return std::to_string(bytes) + " B";
    const char* units[] = {"KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = -1;
    while (value >= 1024 && unit < 3) {
        value /= 1024;
        ++unit;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f %s", value, units[unit]);
    return buffer;
}

// The index query syntax understands size:>10mb and dm:2025-01-01..2025-12-31.
std::string sizeToken(long long bytes)
{
    constexpr long long mb = 1024LL * 1024;
    constexpr long long kb = 1024LL;
    if (bytes > 0 && bytes % mb == 0)
        return std::to_string(bytes / mb) + "mb";
    if (bytes > 0 && bytes % kb == 0)
        return std::to_string(bytes / kb) + "kb";
    return std::to_string(bytes);
}

std::string dateToken(long long seconds)
{
    const std::time_t t = static_cast<std::time_t>(seconds);
    std::tm tm {};
    localtime_s(&tm, &t);
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buffer;
}

std::string folderTerm(const std::string& folder)
{
    std::string name = finder::basename(folder);
    if (name.size() < 2 || name == "\\" || name == "/")
        return {};
    return name;
}

// App data folders often have English names even on a Chinese system:
// 微信 stores under WeChat Files, QQ under Tencent Files, etc. Searching the
bool systemPath(const std::string& path)
{
    const std::string lower = finder::lowerAscii(path);
    static const char* const parts[] = {"\\windows\\", "\\program files", "\\programdata\\", "\\appdata\\",
        "\\$recycle.bin\\", "\\system volume information\\", "\\winsxs\\", "\\temp\\"};
    for (const char* part : parts) {
        if (lower.find(part) != std::string::npos)
            return true;
    }
    return lower.rfind("c:\\windows", 0) == 0;
}

// SDK headers under Windows Kits are source code, but not the user's code.
bool systemCodePath(const std::string& path)
{
    const std::string lower = finder::lowerAscii(path);
    static const char* const parts[] = {"\\windows\\", "\\windows kits\\", "\\program files", "\\programdata\\", "\\winsxs\\"};
    for (const char* part : parts) {
        if (lower.find(part) != std::string::npos)
            return true;
    }
    return lower.rfind("c:\\windows", 0) == 0;
}

// A "biggest files" listing otherwise shows build artifacts and agent session
// logs before anything the user would call a file.
bool noisePath(const std::string& path)
{
    const std::string lower = finder::lowerAscii(path);
    static const char* const parts[] = {"\\.", "\\zig-cache\\", "\\zig-out\\", "\\node_modules\\", "\\target\\",
        "\\obj\\", "\\bin\\", "\\build\\", "\\dist\\", "\\packages\\", "\\vendor\\", "\\venv\\", "\\__pycache__\\",
        "\\site-packages\\", "\\logs\\", "\\cache\\"};
    for (const char* part : parts) {
        if (lower.find(part) != std::string::npos)
            return true;
    }
    const std::string name = finder::lowerAscii(finder::basename(path));
    return name.rfind("ntuser", 0) == 0 || name.rfind("usrclass", 0) == 0;
}

std::string formatWhen(long long seconds)
{
    if (seconds <= 0)
        return {};
    const std::time_t t = static_cast<std::time_t>(seconds);
    std::tm when {};
    localtime_s(&when, &t);
    const std::tm now = finder::detail::localNow();
    const long long today = finder::detail::epochOf(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    const long long day = finder::detail::epochOf(when.tm_year + 1900, when.tm_mon + 1, when.tm_mday);
    const long long diffDays = (today - day) / 86400;
    if (diffDays == 0)
        return "今天";
    if (diffDays == 1)
        return "昨天";
    if (diffDays > 1 && diffDays < 7)
        return std::to_string(diffDays) + " 天前";
    if (diffDays >= 7 && diffDays < 60)
        return std::to_string(diffDays / 7) + " 周前";
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02d", when.tm_year + 1900, when.tm_mon + 1, when.tm_mday);
    return buffer;
}

long long parseIsoTime(const std::string& text)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) < 3
        && std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) < 3)
        return 0;
    if (year < 1970 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;
    std::tm tm {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;

    int offsetSeconds = 0;
    bool utc = false;
    const std::size_t tAt = text.find('T');
    if (tAt != std::string::npos) {
        if (text.find('Z', tAt) != std::string::npos)
            utc = true;
        else {
            const std::size_t plus = text.find('+', tAt);
            const std::size_t minus = text.find('-', tAt + 1);
            const std::size_t signAt = plus != std::string::npos ? plus : minus;
            if (signAt != std::string::npos && signAt > tAt + 8) {
                int oh = 0;
                int om = 0;
                if (std::sscanf(text.c_str() + signAt + 1, "%d:%d", &oh, &om) >= 1) {
                    offsetSeconds = oh * 3600 + om * 60;
                    if (text[signAt] == '-')
                        offsetSeconds = -offsetSeconds;
                }
            }
        }
    }
    if (utc || offsetSeconds != 0) {
        const std::time_t asUtc = _mkgmtime(&tm);
        return static_cast<long long>(asUtc) - offsetSeconds;
    }
    const std::time_t local = std::mktime(&tm);
    return local == static_cast<std::time_t>(-1) ? 0 : static_cast<long long>(local);
}

long long jsonTime(const Json& value)
{
    if (value.isNumber()) {
        double d = value.toNumber();
        if (d > 1e12)
            d /= 1000; // milliseconds
        return static_cast<long long>(d);
    }
    if (value.isString())
        return parseIsoTime(value.toString());
    return 0;
}

std::string extensionGroup(const std::string& ext)
{
    static const std::set<std::string> docs = {"doc", "docm", "docx", "dot", "dotm", "dotx", "rtf", "odt", "wps", "wpd", "txt", "md", "pdf"};
    static const std::set<std::string> sheets = {"xls", "xlsx", "xlsm", "xlsb", "csv", "ods", "et", "ett"};
    static const std::set<std::string> slides = {"ppt", "pptx", "pptm", "pps", "ppsx", "odp", "dps", "dpt"};
    static const std::set<std::string> images = {"jpg", "jpeg", "jfif", "png", "gif", "bmp", "webp", "heic", "heif", "avif", "tif", "tiff", "ico", "svg", "psd", "ai", "raw"};
    static const std::set<std::string> videos = {"mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "ts", "m2ts", "3gp", "rm", "rmvb", "vob"};
    static const std::set<std::string> audio = {"mp3", "wav", "flac", "aac", "m4a", "ogg", "opus", "wma", "ape", "aiff", "alac", "mid", "midi", "amr"};
    static const std::set<std::string> archives = {"zip", "rar", "7z", "tar", "gz", "tgz", "bz2", "xz", "zst", "iso", "cab", "lzh", "arj"};
    static const std::set<std::string> database = {"db", "sqlite", "sqlite3", "db3", "mdb", "accdb"};
    static const std::set<std::string> sql = {"sql", "psql"};
    static const std::set<std::string> ebooks = {"epub", "mobi", "azw", "azw3", "fb2", "djvu", "chm", "cbz", "cbr"};
    static const std::set<std::string> code = {"c", "cpp", "h", "hpp", "cc", "cxx", "cs", "java", "kt", "py", "js", "mjs", "ts", "jsx", "tsx", "go", "rs", "swift", "rb", "php", "lua", "html", "htm", "css", "scss", "json", "xml", "yaml", "yml", "toml", "ini", "sql", "sh", "ps1", "qml", "vue"};
    if (docs.count(ext))
        return "docs";
    if (sheets.count(ext))
        return "sheets";
    if (slides.count(ext))
        return "slides";
    if (images.count(ext))
        return "images";
    if (videos.count(ext))
        return "videos";
    if (audio.count(ext))
        return "audio";
    if (archives.count(ext))
        return "archives";
    if (ebooks.count(ext))
        return "ebooks";
    if (sql.count(ext))
        return "sql";
    if (database.count(ext))
        return "database";
    if (code.count(ext))
        return "code";
    if (ext == "exe" || ext == "msi" || ext == "msix" || ext == "appx")
        return "installers";
    return "other";
}

// What kind of file a concept usually lives in. Only used as a small
// preference, never as a hard filter: a policy can be a PDF or a DOCX.
std::string preferredGroup(const Plan& plan)
{
    static const std::set<std::string> docs = {"policy", "contract", "notice", "report", "summary", "plan", "proposal",
        "manual", "tutorial", "paper", "resume", "certificate", "passport", "medical", "checkup", "score", "menu",
        "insurance", "bank", "loan", "rent", "travel", "flight", "hotel", "study", "english", "receipt", "shipping",
        "contact", "password", "key", "checklist"};
    static const std::set<std::string> sheets = {"budget", "quote", "finance", "sheet", "data"};
    static const std::set<std::string> images = {"photo", "screenshot", "design", "drawing"};
    static const std::set<std::string> code = {"code", "config", "database"};
    static const std::set<std::string> archives = {"archive", "backup", "installer"};
    if (plan.concepts.empty())
        return {};
    const std::string& id = plan.concepts.front().id;
    if (id == "database")
        return "database";
    if (id == "sql")
        return "sql";
    if (docs.count(id))
        return "docs";
    if (sheets.count(id))
        return "sheets";
    if (images.count(id))
        return "images";
    if (code.count(id))
        return "code";
    if (archives.count(id))
        return "archives";
    if (id == "slides")
        return "slides";
    if (id == "video")
        return "videos";
    if (id == "music")
        return "audio";
    return {};
}

int snippetProximity(const Plan& plan, const std::string& snippet)
{
    if (plan.concepts.size() < 2 || snippet.empty())
        return 0;
    const std::string hay = finder::lowerAscii(snippet);
    std::vector<std::size_t> positions;
    for (const auto& concept : plan.concepts) {
        std::size_t best = std::string::npos;
        const finder::Concept& known = finder::concepts()[static_cast<std::size_t>(concept.index)];
        for (const char* term : known.terms) {
            const std::size_t at = finder::findTerm(hay, term);
            if (at != std::string::npos && (best == std::string::npos || at < best))
                best = at;
        }
        if (best != std::string::npos)
            positions.push_back(best);
    }
    if (positions.size() < 2)
        return 0;
    std::sort(positions.begin(), positions.end());
    std::size_t closest = std::string::npos;
    for (std::size_t i = 1; i < positions.size(); ++i)
        closest = std::min(closest, positions[i] - positions[i - 1]);
    if (closest <= 40)
        return 5;
    if (closest <= 120)
        return 3;
    return 0;
}

bool numericName(const std::string& name)
{
    const std::string stem = name.substr(0, name.find_last_of('.') == std::string::npos ? name.size() : name.find_last_of('.'));
    if (stem.size() < 6)
        return false;
    int digits = 0;
    int letters = 0;
    for (const unsigned char c : stem) {
        if (c >= '0' && c <= '9')
            ++digits;
        else if (std::isalpha(c))
            ++letters;
    }
    return digits >= 6 && digits > letters * 3;
}

// The kinds of file a concept almost always names, so "数据库文件" can find
// foo.db instead of looking for a file literally called 数据库.
std::string conceptGroup(const std::string& id)
{
    static const std::map<std::string, std::string> groups = {
        {"database", "database"}, {"sql", "sql"}, {"photo", "images"}, {"screenshot", "images"}, {"video", "videos"},
        {"music", "audio"}, {"code", "code"}, {"archive", "archives"}, {"backup", "archives"},
        {"installer", "installers"}, {"sheet", "sheets"}, {"slides", "slides"}, {"novel", "ebooks"},
    };
    if (const auto it = groups.find(id); it != groups.end())
        return it->second;
    return {};
}

std::vector<std::string> extensionsForType(const std::string& id)
{
    for (const auto& word : finder::detail::typeWords()) {
        if (id == word.id)
            return finder::splitAny(word.extensions, ",");
    }
    return {};
}

std::vector<std::string> conceptExtensions(const std::string& group)
{
    if (group == "database")
        return extensionsForType("database");
    if (group == "sql")
        return extensionsForType("sql");
    if (group == "images")
        return extensionsForType("images");
    if (group == "videos")
        return extensionsForType("videos");
    if (group == "audio")
        return extensionsForType("audio");
    if (group == "archives")
        return extensionsForType("archives");
    if (group == "installers")
        return extensionsForType("installers");
    if (group == "sheets")
        return extensionsForType("excel");
    if (group == "slides")
        return extensionsForType("slides");
    if (group == "ebooks")
        return extensionsForType("ebooks");
    if (group == "code")
        return extensionsForType("code");
    return {};
}

std::string typeLabel(const std::string& path)
{
    const std::string ext = finder::extensionOf(path);
    if (ext.empty())
        return "文件";
    static const std::set<std::string> docs = {"doc", "docm", "docx", "dot", "dotm", "dotx", "rtf", "odt", "wps", "wpd", "txt", "md", "pdf"};
    static const std::set<std::string> sheets = {"xls", "xlsx", "xlsm", "xlsb", "csv", "ods", "et", "ett"};
    static const std::set<std::string> slides = {"ppt", "pptx", "pptm", "pps", "ppsx", "odp", "dps", "dpt"};
    static const std::set<std::string> images = {"jpg", "jpeg", "jfif", "png", "gif", "bmp", "webp", "heic", "heif", "avif", "tif", "tiff", "ico", "svg", "psd", "ai", "raw"};
    static const std::set<std::string> videos = {"mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "ts", "m2ts", "3gp", "rm", "rmvb", "vob"};
    static const std::set<std::string> audio = {"mp3", "wav", "flac", "aac", "m4a", "ogg", "opus", "wma", "ape", "aiff", "alac", "mid", "midi", "amr"};
    static const std::set<std::string> archives = {"zip", "rar", "7z", "tar", "gz", "tgz", "bz2", "xz", "zst", "iso", "cab", "lzh", "arj"};
    static const std::set<std::string> code = {"c", "cpp", "h", "hpp", "cc", "cxx", "cs", "java", "kt", "py", "js", "mjs", "ts", "jsx", "tsx", "go", "rs", "swift", "rb", "php", "lua", "html", "htm", "css", "scss", "json", "xml", "yaml", "yml", "toml", "ini", "sql", "sh", "ps1", "qml", "vue"};
    if (docs.count(ext))
        return ext == "pdf" ? "PDF" : "文档";
    if (sheets.count(ext))
        return "表格";
    if (slides.count(ext))
        return "PPT";
    if (images.count(ext))
        return "图片";
    if (videos.count(ext))
        return "视频";
    if (audio.count(ext))
        return "音频";
    if (archives.count(ext))
        return "压缩包";
    if (ext == "db" || ext == "sqlite" || ext == "sqlite3" || ext == "mdb" || ext == "accdb")
        return "数据库";
    if (ext == "sql" || ext == "psql")
        return "SQL 脚本";
    if (code.count(ext))
        return "代码";
    if (ext == "exe" || ext == "msi" || ext == "msix" || ext == "appx")
        return "安装包";
    return ext + " 文件";
}

// ---- query plan -> searches ---------------------------------------------------

struct QuerySpec {
    std::string text;
    std::string why;
    bool content = false;
    int weight = 1; // 3 exact, 2 broad, 1 synonym
};

struct Candidate {
    std::string path;
    std::string name;
    bool folder = false;
    long long modified = 0;
    long long size = -1;
    std::string snippet;
    int hostScore = 0;
    bool nameHit = false;
    bool contentHit = false;
    bool phraseHit = false;
    bool fallbackOnly = false;
    int bestRank = 1000;
    int bestWeight = 0;
    int finalScore = 0;
    int covered = 0;
    float semantic = -1;
    std::set<int> concepts;
    std::set<std::string> matched;
};

struct ToolFailure {
    std::string tool;
    std::string message;
    int code = 0;
};

struct SearchOutcome {
    std::map<std::string, Candidate> candidates;
    std::vector<ToolFailure> failures;
    bool contentOff = false;
    bool denied = false;
    bool incomplete = false;
    bool phraseTried = false;
    int phraseHits = 0;
    std::set<std::string> phrasePaths;
    bool typeRelaxed = false;
    bool mediaFallback = false;
    bool archiveFallback = false;
    int calls = 0;
    long long elapsedMs = 0;
};

std::vector<std::string> conceptTerms(const Plan& plan)
{
    std::vector<std::string> out;
    for (const auto& concept : plan.concepts) {
        if (concept.matched.size() >= 2 && std::find(out.begin(), out.end(), concept.matched) == out.end())
            out.push_back(concept.matched);
    }
    return out;
}

std::vector<std::string> allTerms(const Plan& plan)
{
    std::vector<std::string> out = conceptTerms(plan);
    for (const std::string& term : plan.terms) {
        if (std::find(out.begin(), out.end(), term) == out.end())
            out.push_back(term);
    }
    return out;
}

std::string alternateTerm(const std::string& id)
{
    static const std::pair<const char*, const char*> table[] = {
        {"reimburse", "费用"}, {"policy", "制度"}, {"contract", "协议"}, {"resume", "履历"}, {"meeting", "纪要"},
        {"weekly", "汇报"}, {"budget", "成本"}, {"quote", "价格"}, {"tax", "纳税"}, {"checkup", "健康"},
        {"medical", "诊断"}, {"vaccine", "接种"}, {"pet", "宠物"}, {"photo", "图片"}, {"screenshot", "截屏"},
        {"video", "影片"}, {"music", "音频"}, {"tutorial", "指南"}, {"paper", "文献"}, {"novel", "阅读"},
        {"code", "源码"}, {"config", "参数"}, {"backup", "存档"}, {"driver", "固件"}, {"certificate", "资质"},
        {"passport", "签证"}, {"idcard", "身份证明"}, {"score", "分数"}, {"menu", "菜谱"}, {"proposal", "提案"},
        {"plan", "规划"}, {"summary", "复盘"}, {"notice", "公告"}, {"template", "范例"}, {"finance", "会计"},
        {"hr", "考勤"}, {"project", "工程"}, {"design", "原型"}, {"drawing", "施工图"}, {"manual", "使用说明"},
        {"installer", "安装"}, {"data", "统计"}, {"sheet", "工作表"}, {"slides", "幻灯片"}, {"archive", "解压"},
        {"download", "下载"}, {"insurance", "保单"}, {"bank", "流水"}, {"loan", "贷款"}, {"rent", "租赁"},
        {"travel", "旅游"}, {"flight", "航班"}, {"hotel", "住宿"}, {"study", "笔记"}, {"english", "单词"},
        {"database", "数据库"}, {"log", "日志"}, {"receipt", "小票"}, {"shipping", "物流"}, {"contact", "名片"},
        {"password", "密码"}, {"key", "密钥"}, {"checklist", "列表"}, {"report", "汇报"},
    };
    for (const auto& [key, value] : table) {
        if (id == key)
            return value;
    }
    return {};
}

// Phrases that put two concepts together, the way a document title would:
// 报销规定, 费用规定, 费用管理办法. A document that says one of these is
// much more likely to be about the pair than one that says both words
// somewhere far apart.
std::vector<std::string> phraseProbes(const Plan& plan)
{
    std::vector<std::string> out;
    if (plan.concepts.size() < 2)
        return out;
    const auto& first = plan.concepts[0];
    const auto& second = plan.concepts[1];
    const std::string alt = alternateTerm(first.id);
    static const std::set<std::string> policyLike = {"policy", "notice", "template", "report", "summary",
        "plan", "proposal", "manual", "contract", "checklist"};
    std::vector<std::string> raw;
    if (policyLike.count(second.id)) {
        // 报销 + 规定: 报销规定, 费用规定, 费用管理办法, 费用报销
        raw = {first.matched + second.matched, alt + second.matched, alt + "管理办法", alt + first.matched};
    } else {
        // 租房 + 合同 can be said either way round.
        raw = {first.matched + second.matched, second.matched + first.matched, alt + second.matched, alt + first.matched};
    }
    for (const std::string& phrase : raw) {
        if (finder::utf8Length(phrase) < 4)
            continue;
        if (std::find(out.begin(), out.end(), phrase) != out.end())
            continue;
        out.push_back(phrase);
        if (out.size() >= 4)
            break;
    }
    return out;
}

class Plugin : public winshun::Plugin {
public:
    std::string name() const override { return "ainwin"; }

    winshun::Items query(const winshun::Query& q) override
    {
        const auto started = std::chrono::steady_clock::now();
        const finder::Plan plan = finder::parsePlan(q.text, currentSettings().extraTypes);
        const std::string text = finder::trim(q.text);
        const std::string prefix = q.keyword.empty() ? (chinese() ? "找 " : "find ") : q.keyword + " ";

        if (text.empty())
            return usage(prefix);
        if (finder::utf8Length(text) < 2 && !plan.type.set && plan.concepts.empty())
            return needsMore();
        const bool conditioned = plan.time.set || plan.minSizeSet || plan.maxSizeSet || !plan.folder.empty();
        if (plan.empty() && !plan.type.set && !conditioned)
            return needsMore(plan);

        const std::string key = text + "\x1f" + settingsKey();
        const long long now = static_cast<long long>(std::time(nullptr));
        if (const auto it = m_cache.find(key); it != m_cache.end() && now - it->second.at < kCacheSeconds)
            return it->second.items;

        // WeChat's transferred files live outside the normal document index and
        // are mostly hash-named. Scan only FileStorage/msg subtrees, not the
        // client's config, cache or emoji data.
        if (finder::isAppFileQuery(text) && plan.folder.empty() && !plan.type.folders) {
            const Settings s = currentSettings();
            const std::vector<finder::AppFile> found = finder::findAppFiles(plan, s.wechatDirs, s.maxResults);
            if (!found.empty()) {
                Items items;
                for (const finder::AppFile& file : found) {
                    Item item("w:" + lowerKey(file.path), file.name,
                        (chinese() ? finder::sourceLabel(file.source) + " · " : finder::sourceLabel(file.source) + " · ") + formatWhen(file.modified) + " · " + humanSize(file.size));
                    const std::string prev = file.path + "\n" + formatWhen(file.modified) + " · " + humanSize(file.size);
                    item.withPath(file.path).withIcon("file:" + file.path).withCopy(file.path).withModified(file.modified).withSize(file.size).withPreview(prev)
                        .withData(Json::Object {{"kind", "file"}, {"path", file.path}, {"source", file.source}});
                    items.push_back(std::move(item));
                }
                winshun::debug("[find] app files=" + std::to_string(items.size()));
                if (m_cache.size() > 24)
                    m_cache.clear();
                m_cache[key] = CacheEntry {now, items};
                return items;
            }
        }

        progress(q.queryId, chinese() ? "正在按描述查找…" : "Looking by what you described…");
        SearchOutcome outcome = search(plan);

        Items items = render(plan, outcome, prefix);
        int files = 0;
        for (const Item& item : items) {
            if (item.id.rfind("f:", 0) == 0)
                ++files;
        }
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        winshun::debug("[find] chars=" + std::to_string(finder::utf8Length(text)) + " concepts=" + std::to_string(plan.concepts.size())
            + " terms=" + std::to_string(plan.terms.size()) + " calls=" + std::to_string(outcome.calls) + " files=" + std::to_string(files)
            + " items=" + std::to_string(items.size()) + " ms=" + std::to_string(ms));

        if (m_cache.size() > 24)
            m_cache.clear();
        m_cache[key] = CacheEntry {now, items};
        return items;
    }

    winshun::Effects action(const winshun::ActionRequest& a) override
    {
        const std::string action = a.action;
        const std::string path = a.item["data"]["path"].isString() ? a.item["data"]["path"].toString()
            : (a.item["open"]["path"].isString() ? a.item["open"]["path"].toString() : std::string());
        if (action == "copy" || action == "copyPath") {
            const std::string text = path.empty() ? a.item["copy"].toString() : path;
            return {winshun::effect::copy(text), winshun::effect::notify(chinese() ? "已复制路径" : "Path copied"), winshun::effect::hide()};
        }
        if (action == "reveal") {
            if (path.empty())
                return {winshun::effect::notify(chinese() ? "没有可显示的路径" : "No path to show"), winshun::effect::hide()};
            const Json r = tool("show_in_folder", Json::Object {{"path", path}});
            if (r.has("error"))
                return {winshun::effect::notify(chinese() ? "无法在文件夹中显示" : "Could not show it in its folder"), winshun::effect::hide()};
            return {winshun::effect::hide()};
        }
        if (action == "example" || action == "fallback") {
            const std::string text = a.item["data"]["text"].isString() ? a.item["data"]["text"].toString() : a.item["title"].toString();
            return {winshun::effect::setQuery(text), winshun::effect::keep()};
        }
        return {winshun::effect::hide()};
    }

    void settingsChanged(const Json& settings) override
    {
        (void)settings;
        m_cache.clear();
        m_embedderTried = false;
        m_embedder.reset();
    }

private:
    struct CacheEntry {
        long long at = 0;
        Items items;
    };

    Items usage(const std::string& prefix) const
    {
        const bool zh = chinese();
        Items items;
        items.push_back(Item("help", zh ? "描述你要找的文件" : "Describe the file you want",
                                 zh ? "例如：去年报销的规定、上周的会议记录、桌面上的合同 PDF" : "e.g. last year's expense rules, last week's meeting notes")
                             .withIcon("glyph:U+E721")
                             .invalid());
        const char* examples[][2] = {
            {"去年报销的规定", "last year's expense rules"},
            {"上周的会议记录", "last week's meeting notes"},
            {"桌面上的合同 PDF", "the contract PDF on the desktop"},
        };
        int n = 1;
        for (const auto& example : examples) {
            const std::string text = prefix + (zh ? example[0] : example[1]);
            items.push_back(Item("example-" + std::to_string(n), text, zh ? "回车填入搜索框" : "Enter puts it in the box")
                                .withIcon("glyph:U+E8A5")
                                .withActions({{"example", zh ? "填入" : "Use", "Enter"}})
                                .withData(Json::Object {{"text", text}}));
            ++n;
        }
        items.push_back(Item("keys", zh ? "键盘就能完成" : "All by keyboard",
                                 zh ? "Enter 打开 · Ctrl+Enter 复制路径 · Shift+Enter 在文件夹中显示"
                                    : "Enter opens · Ctrl+Enter copies the path · Shift+Enter shows it in its folder")
                             .withIcon("glyph:U+E765")
                             .invalid());
        return items;
    }

    Items needsMore() const { return needsMore(finder::Plan {}); }

    Items needsMore(const Plan& plan) const
    {
        const bool zh = chinese();
        Items items;
        if (plan.time.set) {
            items.push_back(Item("more", zh ? "时间条件有了，还差一句描述" : "A time, but nothing to look for",
                                     zh ? "例如：上周的会议记录、去年的报销规定" : "e.g. last week's meeting notes")
                                 .withIcon("glyph:U+E721")
                                 .invalid());
        } else {
            items.push_back(Item("more", zh ? "再多说几个字" : "A little more to go on",
                                     zh ? "写清楚是什么文件、什么时候、在哪：例如 去年报销的规定" : "What, when, where: e.g. last year's expense rules")
                                 .withIcon("glyph:U+E721")
                                 .invalid());
        }
        items.push_back(Item("hint", zh ? "可以只描述，不必记得文件名" : "Describe it; the name is not needed",
                                 zh ? "插件会拆出概念、时间和类型，再用 Win顺 的索引找" : "It splits concepts, time and type, then uses WinShun's index")
                             .withIcon("glyph:U+E8A5")
                             .invalid());
        return items;
    }

    // ---- searching -------------------------------------------------------------

    struct Settings {
        std::vector<std::string> folders;
        bool content = true;
        int maxResults = 12;
        bool snippets = true;
        bool useModel = true;
        std::string modelPreset = "auto";
        std::string device = "auto";
        std::string modelDir = "models\\bge-small-zh-v1.5-int8";
        int threads = 4;
        int maxLength = 128;
        std::string extraTypes;
        std::string wechatDirs;
    };

    Settings currentSettings() const
    {
        Settings s;
        s.content = settings()["content"].isBool() ? settings()["content"].toBool(true) : true;
        s.maxResults = static_cast<int>(settings()["maxResults"].toInt(12));
        s.maxResults = std::max(0, std::min(200, s.maxResults));
        s.snippets = settings()["snippets"].isBool() ? settings()["snippets"].toBool(true) : true;
        s.useModel = settings()["useModel"].isBool() ? settings()["useModel"].toBool(true) : true;
        if (settings()["modelPreset"].isString() && !settings()["modelPreset"].toString().empty())
            s.modelPreset = settings()["modelPreset"].toString();
        if (settings()["device"].isString() && !settings()["device"].toString().empty())
            s.device = settings()["device"].toString();
        if (settings()["modelDir"].isString() && !settings()["modelDir"].toString().empty())
            s.modelDir = settings()["modelDir"].toString();
        s.threads = std::max(1, std::min(8, static_cast<int>(settings()["threads"].toInt(4))));
        s.maxLength = std::max(64, std::min(512, static_cast<int>(settings()["maxLength"].toInt(128))));
        s.extraTypes = settings()["extraTypes"].toString();
        s.wechatDirs = settings()["wechatDirs"].toString();
        std::string list = settings()["folders"].toString();
        std::replace(list.begin(), list.end(), '\n', ';');
        for (std::string folder : finder::splitAny(list, ";")) {
            folder = finder::expandEnv(finder::trim(folder));
            if (folder.empty())
                continue;
            folder = finder::replaceAll(folder, "/", "\\");
            while (folder.size() > 3 && (folder.back() == '\\' || folder.back() == '/'))
                folder.pop_back();
            if (!folder.empty() && s.folders.size() < 8)
                s.folders.push_back(folder);
        }
        return s;
    }

    std::string settingsKey() const
    {
        return settings()["folders"].toString() + "|" + (settings()["content"].toBool(true) ? "1" : "0") + "|"
            + std::to_string(settings()["maxResults"].toInt(12)) + "|" + (settings()["snippets"].toBool(true) ? "1" : "0")
            + "|" + (settings()["useModel"].toBool(true) ? "1" : "0") + "|" + settings()["modelPreset"].toString()
            + "|" + settings()["device"].toString() + "|" + settings()["modelDir"].toString() + "|"
            + std::to_string(settings()["threads"].toInt(4)) + "|" + std::to_string(settings()["maxLength"].toInt(128))
            + "|" + settings()["extraTypes"].toString() + "|" + settings()["wechatDirs"].toString();
    }

    static bool dirExists(const std::string& dir)
    {
        const std::wstring wide = finder::widenOf(dir);
        return !wide.empty() && ::GetFileAttributesW(wide.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    std::string modelDirFor(const Settings& s) const
    {
        const std::string base = pluginDir().empty() ? std::string(".") : pluginDir();
        const std::string light = base + "\\models\\bge-small-zh-v1.5-int8";
        const std::string balanced = base + "\\models\\bge-base-zh-v1.5-int8";
        const std::string large = base + "\\models\\bge-large-zh-v1.5-int8";

        std::string preset = s.modelPreset;
        if (preset.empty())
            preset = "auto";
        if (preset == "auto") {
            MEMORYSTATUSEX memory {};
            memory.dwLength = sizeof memory;
            const bool plenty = ::GlobalMemoryStatusEx(&memory) && memory.ullTotalPhys >= 16ULL * 1024 * 1024 * 1024;
            preset = (plenty && dirExists(balanced)) ? "balanced" : "light";
        }
        std::string dir;
        if (preset == "balanced")
            dir = balanced;
        else if (preset == "large")
            dir = large;
        else if (preset == "custom")
            dir = s.modelDir;
        else
            dir = light;

        if (dir.size() >= 2 && std::isalpha(static_cast<unsigned char>(dir[0])) && dir[1] == ':') {
            // absolute
        } else if (dir.rfind("\\\\", 0) != 0) {
            dir = base + "\\" + dir;
        }
        if (dirExists(dir))
            return dir;
        if (preset != "light" && dirExists(light)) {
            winshun::debug("[find] model preset '" + preset + "' is missing, falling back to light");
            return light;
        }
        return dir;
    }

    void ensureEmbedder() const
    {
        if (m_embedderTried)
            return;
        m_embedderTried = true;
        const Settings s = currentSettings();
        if (!s.useModel) {
            winshun::debug("[find] local embeddings off by setting");
            return;
        }
        const std::string dir = modelDirFor(s);
        const auto started = std::chrono::steady_clock::now();
        std::string error;
        if (!m_embedder.load(dir, s.threads, s.maxLength)) {
            m_embedderError = m_embedder.error();
            winshun::debug("[find] model not loaded: " + m_embedderError);
            return;
        }
        if (!m_embedder.embed({chinese() ? "测试" : "test"}, true, m_probeVector, error)) {
            m_embedderError = error;
            winshun::debug("[find] model probe failed: " + error);
            m_embedder.reset();
            return;
        }
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        winshun::debug("[find] model loaded: " + m_embedder.modelName() + " dim=" + std::to_string(m_embedder.dimension())
            + " threads=" + std::to_string(s.threads) + " preset=" + s.modelPreset + " ms=" + std::to_string(ms));
    }

    std::string semanticQuery(const Plan& plan) const
    {
        std::vector<std::string> words;
        for (const auto& concept : plan.concepts)
            words.push_back(concept.matched);
        if (plan.concepts.empty() && plan.type.set)
            words.push_back(plan.type.label);
        for (const std::string& term : plan.terms)
            words.push_back(term);
        const std::string text = join(words, " ");
        return text.empty() ? plan.text : text;
    }

    void semanticRerank(const Plan& plan, std::vector<Candidate*>& kept) const
    {
        if (kept.size() < 2)
            return;
        // A type listing ("找视频") is already exact; there is nothing for a
        // sentence encoder to add, and it would only cost time.
        if (plan.concepts.empty() && plan.type.set)
            return;
        bool anySnippet = false;
        for (const Candidate* c : kept)
            anySnippet = anySnippet || !c->snippet.empty();
        if (!anySnippet && plan.concepts.size() < 2)
            return;
        ensureEmbedder();
        if (!m_embedder.ready())
            return;

        std::vector<Candidate*> targets;
        if (kept.size() <= 18) {
            targets = kept;
        } else {
            targets = kept;
            std::stable_sort(targets.begin(), targets.end(), [](const Candidate* a, const Candidate* b) {
                const int pa = (a->phraseHit ? 1000 : 0) + (a->nameHit ? 400 : 0) + a->hostScore + a->covered * 25;
                const int pb = (b->phraseHit ? 1000 : 0) + (b->nameHit ? 400 : 0) + b->hostScore + b->covered * 25;
                return pa > pb;
            });
            targets.resize(18);
        }

        std::vector<std::string> texts;
        texts.reserve(targets.size());
        for (const Candidate* c : targets) {
            std::string text = c->name;
            if (!c->snippet.empty())
                text += "\n" + trimSnippet(c->snippet, 120);
            texts.push_back(std::move(text));
        }
        std::vector<std::vector<float>> docs;
        std::string error;
        const auto started = std::chrono::steady_clock::now();
        if (!m_embedder.embed(texts, false, docs, error)) {
            winshun::debug("[find] embed documents failed: " + error);
            return;
        }
        std::vector<std::vector<float>> query;
        if (!m_embedder.embed({semanticQuery(plan)}, true, query, error) || query.empty()) {
            winshun::debug("[find] embed query failed: " + error);
            return;
        }
        for (std::size_t i = 0; i < targets.size() && i < docs.size(); ++i)
            targets[i]->semantic = finder::Embedder::cosine(query.front(), docs[i]);
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        winshun::debug("[find] rerank docs=" + std::to_string(targets.size()) + " ms=" + std::to_string(ms));
    }

    Json contentArgs(const std::string& query, const Plan& plan, const Settings& s) const
    {
        Json args = Json::Object {{"query", query}, {"limit", kContentLimit}, {"timeout_ms", kContentTimeoutMs}};
        std::vector<std::string> folders;
        if (!plan.folder.empty())
            folders.push_back(plan.folder);
        else
            folders = s.folders;
        if (!folders.empty()) {
            Json::Array array;
            for (const std::string& folder : folders)
                array.push_back(folder);
            args.set("folders", Json(std::move(array)));
        }
        return args;
    }

    // One tool result becomes candidates. `conceptIds` is what this query was
    // about: a strict query matched all of them, a broad one only the first.
    void absorb(const Json& result, const std::string& toolName, const std::string& query, bool content, int weight,
        const std::vector<int>& conceptIds, SearchOutcome& out, bool phrase = false, bool fallbackOnly = false) const
    {
        const Json::Array& items = result["items"].toArray();
        int rank = 0;
        for (const Json& item : items) {
            const std::string path = item["path"].toString();
            if (path.empty()) {
                ++rank;
                continue;
            }
            const std::string key = lowerKey(path);
            Candidate& c = out.candidates[key];
            if (c.path.empty()) {
                c.path = path;
                c.name = item["name"].toString().empty() ? finder::basename(path) : item["name"].toString();
                c.folder = item["type"].toString() == "folder" || path.back() == '\\' || path.back() == '/';
                c.modified = jsonTime(item["modified"]);
                c.size = item["size"].isNumber() ? item["size"].toInt(-1) : -1;
            } else {
                if (c.modified == 0)
                    c.modified = jsonTime(item["modified"]);
                if (c.size < 0)
                    c.size = item["size"].isNumber() ? item["size"].toInt(-1) : -1;
            }
            c.bestRank = std::min(c.bestRank, rank);
            c.bestWeight = std::max(c.bestWeight, weight);
            if (content) {
                c.contentHit = true;
                const std::string snippet = item["snippet"].toString();
                if (snippet.size() > c.snippet.size())
                    c.snippet = snippet;
                c.hostScore = std::max(c.hostScore, static_cast<int>(item["score"].toInt(0)));
                // A content query promises the terms somewhere in the file,
                // not in the 300-character snippet: coverage is measured
                // from what is actually visible (coverage()).
            } else {
                c.nameHit = true;
                c.hostScore = std::max(c.hostScore, static_cast<int>(item["score"].toInt(0)));
                for (int id : conceptIds)
                    c.concepts.insert(id);
            }
            c.matched.insert(query);
            if (phrase) {
                c.phraseHit = true;
                out.phrasePaths.insert(key);
            }
            if (fallbackOnly)
                c.fallbackOnly = true;
            ++rank;
        }
        (void)toolName;
    }

    void fail(const std::string& toolName, const Json& result, SearchOutcome& out) const
    {
        ToolFailure f;
        f.tool = toolName;
        const Json& error = result["error"];
        f.message = error["message"].toString();
        f.code = static_cast<int>(error["code"].toInt(0));
        if (f.message.empty())
            f.message = "no answer";
        out.failures.push_back(f);
        const std::string lower = finder::lowerAscii(f.message);
        if (lower.find("permission") != std::string::npos)
            out.denied = true;
        if (lower.find("content search") != std::string::npos && (lower.find("turned off") != std::string::npos || lower.find("off") != std::string::npos))
            out.contentOff = true;
        winshun::debug("[find] tool failed: " + toolName + " code=" + std::to_string(f.code) + " " + f.message);
    }

    void logTool(const std::string& toolName, const std::string& query, const Json& result) const
    {
        std::string line = "[find] " + toolName + " q=" + finder::utf8Cut(query, 48);
        if (result.has("error")) {
            line += " error=" + finder::utf8Cut(result["error"]["message"].toString(), 120);
        } else {
            const Json::Array& items = result["items"].toArray();
            line += " items=" + std::to_string(items.size());
            std::string scores;
            for (std::size_t i = 0; i < items.size() && i < 5; ++i) {
                if (i > 0)
                    scores += ",";
                scores += std::to_string(items[i]["score"].toInt(-1));
            }
            if (!scores.empty())
                line += " top=" + scores;
        }
        winshun::debug(line);
    }

    void mergeOutcome(SearchOutcome& into, SearchOutcome& from) const
    {
        for (auto& [key, c] : from.candidates) {
            auto it = into.candidates.find(key);
            if (it == into.candidates.end()) {
                into.candidates.emplace(key, std::move(c));
                continue;
            }
            Candidate& dst = it->second;
            dst.nameHit = dst.nameHit || c.nameHit;
            dst.contentHit = dst.contentHit || c.contentHit;
            dst.phraseHit = dst.phraseHit || c.phraseHit;
            dst.fallbackOnly = dst.fallbackOnly && c.fallbackOnly;
            dst.hostScore = std::max(dst.hostScore, c.hostScore);
            dst.bestRank = std::min(dst.bestRank, c.bestRank);
            dst.bestWeight = std::max(dst.bestWeight, c.bestWeight);
            if (dst.modified == 0)
                dst.modified = c.modified;
            if (dst.size < 0)
                dst.size = c.size;
            if (c.snippet.size() > dst.snippet.size())
                dst.snippet = c.snippet;
            dst.concepts.insert(c.concepts.begin(), c.concepts.end());
            dst.matched.insert(c.matched.begin(), c.matched.end());
        }
        into.failures.insert(into.failures.end(), from.failures.begin(), from.failures.end());
        into.calls += from.calls;
        into.incomplete = into.incomplete || from.incomplete;
        into.contentOff = into.contentOff || from.contentOff;
        into.denied = into.denied || from.denied;
        into.phraseTried = into.phraseTried || from.phraseTried;
        into.phrasePaths.insert(from.phrasePaths.begin(), from.phrasePaths.end());
        into.phraseHits = static_cast<int>(into.phrasePaths.size());
        into.typeRelaxed = into.typeRelaxed || from.typeRelaxed;
        into.mediaFallback = into.mediaFallback || from.mediaFallback;
        into.archiveFallback = into.archiveFallback || from.archiveFallback;
    }

    SearchOutcome search(const Plan& plan, bool allowRelax = true)
    {
        SearchOutcome out;
        const Settings s = currentSettings();
        const auto started = std::chrono::steady_clock::now();

        const std::vector<std::string> terms = allTerms(plan);
        const std::vector<int> allConcepts = [&] {
            std::vector<int> ids;
            for (const auto& concept : plan.concepts)
                ids.push_back(concept.index);
            return ids;
        }();

        // "找视频" / "上周的 PPT" / "大于 1M 的文件" is a listing, not a
        // search for those words inside documents. "截图" is not: it narrows
        // pictures to screenshots.
        const bool listing = isListing(plan);

        std::string wordQuery;
        if (!listing) {
            for (const std::string& term : terms) {
                if (literal(term) || isTypeAlias(plan, term))
                    continue;
                if (!wordQuery.empty())
                    wordQuery += ' ';
                wordQuery += quoteWord(term);
            }
        }

        std::string strict = wordQuery;
        if (strict.empty() && plan.type.set && !plan.type.folders) {
            std::vector<std::string> exts;
            for (const std::string& ext : plan.type.extensions) {
                if (exts.size() >= 64)
                    break;
                exts.push_back(ext);
            }
            strict = "ext:" + join(exts, ",");
        }
        if (plan.type.set && plan.type.folders && strict.empty())
            strict = "*";

        // A folder name is a term the name index understands (it matches a
        // parent folder too), so "桌面上的大文件" is not a global scan.
        if (!plan.folder.empty()) {
            const std::string folder = folderTerm(plan.folder);
            if (!folder.empty())
                strict = quoteWord(folder) + (strict.empty() ? std::string() : " " + strict);
        }

        std::vector<std::string> filters;
        if (plan.minSizeSet)
            filters.push_back("size:>" + sizeToken(plan.minSize));
        if (plan.maxSizeSet)
            filters.push_back("size:<" + sizeToken(plan.maxSize));
        if (plan.time.set)
            filters.push_back("dm:" + dateToken(plan.time.from) + ".." + dateToken(plan.time.to > 86400 ? plan.time.to - 86400 : plan.time.to));
        if (!filters.empty())
            strict += (strict.empty() ? std::string() : " ") + join(filters, " ");
        const std::string filterText = join(filters, " ");
        if (strict.empty())
            strict = "*";

        const bool typeOnly = listing; // no content query to run
        const bool foldersOnly = plan.type.set && plan.type.folders && wordQuery.empty();
        // Content results carry no size, so a size condition must stay a
        // name-index query where Win顺 returns the size it filtered on.
        const bool skipContent = listing || plan.minSizeSet || plan.maxSizeSet;

        if (!strict.empty()) {
            Json args = Json::Object {{"query", strict}, {"kind", plan.type.folders ? "folders" : "all"},
                {"limit", listing ? 100 : kNameLimit}, {"sort", listing ? "modified" : "relevance"}};
            const Json r = tool("search_files", args);
            ++out.calls;
            logTool("search_files", strict, r);
            if (r.has("error"))
                fail("search_files", r, out);
            else
                absorb(r, "search_files", strict, false, 3, allConcepts, out);
        }

        if (s.content && !skipContent && !wordQuery.empty() && !out.denied) {
            const Json r = tool("search_content", contentArgs(wordQuery, plan, s));
            ++out.calls;
            logTool("search_content", wordQuery, r);
            if (r.has("error")) {
                fail("search_content", r, out);
            } else {
                absorb(r, "search_content", wordQuery, true, 3, allConcepts, out);
                if (r["complete"].isBool() && !r["complete"].toBool(true))
                    out.incomplete = true;
            }
        }

        // If the whole sentence (all terms ANDed) found nothing, ask the
        // content index for one or two of its words on their own. A file
        // that says 报账 but not 材料 is still the file the user means.
        if (s.content && !skipContent && !out.denied && !out.contentOff && !usable(plan, out)) {
            int extra = 0;
            std::vector<std::pair<std::string, int>> probes;
            for (const auto& concept : plan.concepts) {
                const finder::Concept& known = finder::concepts()[static_cast<std::size_t>(concept.index)];
                int perConcept = 0;
                for (const char* term : known.terms) {
                    const std::string word(term);
                    if (perConcept >= 2 || extra >= 2)
                        break;
                    if (finder::utf8Length(word) < 2)
                        continue;
                    if (word == concept.matched)
                        continue;
                    probes.emplace_back(word, concept.index);
                    ++perConcept;
                }
                if (extra >= 2)
                    break;
                (void)extra;
            }
            if (probes.empty()) {
                for (const std::string& term : plan.terms) {
                    if (finder::utf8Length(term) >= 2)
                        probes.emplace_back(term, -1);
                    if (probes.size() >= 2)
                        break;
                }
            }
            for (const auto& [word, conceptIndex] : probes) {
                if (extra >= 2 || out.contentOff)
                    break;
                const Json r = tool("search_content", contentArgs(word, plan, s));
                ++out.calls;
                logTool("search_content", word, r);
                if (r.has("error")) {
                    fail("search_content", r, out);
                    continue;
                }
                absorb(r, "search_content", word, true, 2, conceptIndex >= 0 ? std::vector<int> {conceptIndex} : std::vector<int> {}, out);
                if (r["complete"].isBool() && !r["complete"].toBool(true))
                    out.incomplete = true;
                ++extra;
            }
        }

        // A type word still needs the extension index when the sentence has
        // no other narrowing word: "大于1kb的c++代码" is a .cpp listing, not a
        // search for a file whose name contains 代码.
        bool extensionLike = plan.terms.empty();
        if (plan.terms.size() == 1) {
            const std::string& term = plan.terms.front();
            extensionLike = std::find(plan.type.extensions.begin(), plan.type.extensions.end(), finder::lowerAscii(term)) != plan.type.extensions.end();
            if (!extensionLike && !term.empty()) {
                extensionLike = std::all_of(term.begin(), term.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
            }
        }
        const bool typeProbe = plan.type.id == "custom" || extensionLike;
        if (!out.denied && plan.type.set && !plan.type.folders && typeProbe && !usable(plan, out)) {
            std::vector<std::string> exts = plan.type.extensions;
            if (exts.size() > 64)
                exts.resize(64);
            if (!exts.empty()) {
                std::string query = "ext:" + join(exts, ",");
                if (!filterText.empty())
                    query += " " + filterText;
                if (!plan.folder.empty()) {
                    const std::string folder = folderTerm(plan.folder);
                    if (!folder.empty())
                        query = quoteWord(folder) + " " + query;
                }
                const Json r = tool("search_files", Json::Object {{"query", query}, {"limit", 100}, {"sort", "modified"}});
                ++out.calls;
                logTool("search_files", query, r);
                if (r.has("error"))
                    fail("search_files", r, out);
                else
                    absorb(r, "search_files", query, false, 2, {}, out, false, true);
            }
        }

        // Pictures and videos have no useful text to match; when the word
        // search finds nothing, list the type in the requested time/folder
        // instead of pretending there is no such photo.
        if (!out.denied && !listing && !usable(plan, out)
            && (plan.type.id == "images" || plan.type.id == "videos" || plan.type.id == "audio")) {
            std::vector<std::string> exts = plan.type.extensions;
            if (exts.size() > 64)
                exts.resize(64);
            if (!exts.empty()) {
                std::string query = "ext:" + join(exts, ",");
                if (!filterText.empty())
                    query += " " + filterText;
                if (!plan.folder.empty()) {
                    const std::string folder = folderTerm(plan.folder);
                    if (!folder.empty())
                        query = quoteWord(folder) + " " + query;
                }
                const Json r = tool("search_files", Json::Object {{"query", query}, {"limit", 100}, {"sort", "modified"}});
                ++out.calls;
                logTool("search_files", query, r);
                if (r.has("error"))
                    fail("search_files", r, out);
                else {
                    absorb(r, "search_files", query, false, 1, {}, out);
                    out.mediaFallback = true;
                }
            }
        }

        // Archives often have generic names ("backup.zip", "微信文件夹.zip").
        // If the name words do not hit, list archives in the requested time
        // and folder instead of returning nothing.
        if (!out.denied && !listing && !usable(plan, out) && plan.type.id == "archives" && !plan.type.folders) {
            std::vector<std::string> exts = plan.type.extensions;
            if (exts.size() > 64)
                exts.resize(64);
            if (!exts.empty()) {
                std::string query = "ext:" + join(exts, ",");
                if (!filterText.empty())
                    query += " " + filterText;
                if (!plan.folder.empty()) {
                    const std::string folder = folderTerm(plan.folder);
                    if (!folder.empty())
                        query = quoteWord(folder) + " " + query;
                }
                const Json r = tool("search_files", Json::Object {{"query", query}, {"limit", 100}, {"sort", "modified"}});
                ++out.calls;
                logTool("search_files", query, r);
                if (r.has("error"))
                    fail("search_files", r, out);
                else {
                    absorb(r, "search_files", query, false, 1, {}, out);
                    out.archiveFallback = true;
                }
            }
        }

        // A concept that names a file kind ("数据库", "照片") should also
        // find foo.db / IMG_1234.jpg, whose names never contain the word.
        // If the sentence also gives a type ("数据库脚本"), the two narrow
        // each other: 数据库 ∩ 脚本 = .sql, not every .db and every script.
        if (!out.denied && plan.concepts.size() == 1 && plan.terms.empty()) {
            const std::string group = conceptGroup(plan.concepts.front().id);
            std::vector<std::string> probeExts = conceptExtensions(group);
            // 数据库脚本 is a .sql script, never a .db file: the type word
            // narrows 数据库 to its textual form.
            if (plan.type.set && plan.type.id == "code" && plan.concepts.front().id == "database") {
                probeExts = {"sql"};
            } else if (plan.type.set && !plan.type.folders) {
                std::vector<std::string> narrowed;
                for (const std::string& ext : plan.type.extensions) {
                    if (probeExts.empty() || std::find(probeExts.begin(), probeExts.end(), ext) != probeExts.end())
                        narrowed.push_back(ext);
                }
                if (!narrowed.empty())
                    probeExts = narrowed;
                else if (probeExts.empty())
                    probeExts = plan.type.extensions;
            }
            if (probeExts.size() > 64)
                probeExts.resize(64);
            if (!probeExts.empty()) {
                std::string query = "ext:" + join(probeExts, ",");
                if (!filterText.empty())
                    query += " " + filterText;
                if (!plan.folder.empty()) {
                    const std::string folder = folderTerm(plan.folder);
                    if (!folder.empty())
                        query = quoteWord(folder) + " " + query;
                }
                const Json r = tool("search_files", Json::Object {{"query", query}, {"limit", 100}, {"sort", "modified"}});
                ++out.calls;
                logTool("search_files", query, r);
                if (r.has("error"))
                    fail("search_files", r, out);
                else
                    absorb(r, "search_files", query, false, 2, {}, out, false, true);
            }
        }

        // A size/time condition with a word still needs a name query, but if
        // that found nothing the same conditions without the word can be
        // filtered locally (a .db file does not have 数据库 in its name).
        if (!out.denied && !filterText.empty() && !plan.concepts.empty() && !usable(plan, out)) {
            std::string query = filterText;
            if (!plan.folder.empty()) {
                const std::string folder = folderTerm(plan.folder);
                if (!folder.empty())
                    query = quoteWord(folder) + " " + query;
            }
            const Json r = tool("search_files", Json::Object {{"query", query}, {"limit", 100}, {"sort", "modified"}});
            ++out.calls;
            logTool("search_files", query, r);
            if (r.has("error"))
                fail("search_files", r, out);
            else
                absorb(r, "search_files", query, false, 1, {}, out, false, true);
        }

        // Two concepts together are stronger than two words apart. Ask for
        // the exact phrases a title would use; these results get the highest
        // confidence boost, and are what keeps a random document that
        // happens to contain both words from outranking a real policy.
        if (s.content && !skipContent && !typeOnly && !out.denied && !out.contentOff && plan.concepts.size() >= 2) {
            out.phraseTried = true;
            for (const std::string& phrase : phraseProbes(plan)) {
                if (out.contentOff)
                    break;
                Json args = contentArgs("\"" + phrase + "\"", plan, s);
                args.set("limit", 8);
                args.set("timeout_ms", 900);
                const Json r = tool("search_content", args);
                ++out.calls;
                logTool("search_content", "\"" + phrase + "\"", r);
                if (r.has("error"))
                    fail("search_content", r, out);
                else
                    absorb(r, "search_content", phrase, true, 4, allConcepts, out, true);
            }
            out.phraseHits = static_cast<int>(out.phrasePaths.size());
        }

        // Nothing exact: one broad word from the first concept, then one
        // synonym ("报销" also finds 费用). These are what make it work when
        // the document does not use the user's words.
        if (!out.denied && !foldersOnly && !usable(plan, out)) {
            if (!plan.concepts.empty()) {
                const auto& first = plan.concepts.front();
                const std::string broad = first.matched;
                if (!broad.empty() && broad != strict) {
                    const Json r = tool("search_files", Json::Object {{"query", broad}, {"limit", kNameLimit}});
                    ++out.calls;
                    logTool("search_files", broad, r);
                    if (!r.has("error"))
                        absorb(r, "search_files", broad, false, 2, {first.index}, out);
                    else
                        fail("search_files", r, out);
                    if (s.content && !out.contentOff && !plan.minSizeSet && !plan.maxSizeSet) {
                        const Json c = tool("search_content", contentArgs(broad, plan, s));
                        ++out.calls;
                        logTool("search_content", broad, c);
                        if (!c.has("error")) {
                            absorb(c, "search_content", broad, true, 2, {first.index}, out);
                            if (c["complete"].isBool() && !c["complete"].toBool(true))
                                out.incomplete = true;
                        } else
                            fail("search_content", c, out);
                    }
                }
                const std::string alt = alternateTerm(first.id);
                if (!usable(plan, out) && !alt.empty() && alt != broad && alt != strict) {
                    const Json r = tool("search_files", Json::Object {{"query", alt}, {"limit", kNameLimit}});
                    ++out.calls;
                    logTool("search_files", alt, r);
                    if (!r.has("error"))
                        absorb(r, "search_files", alt, false, 1, {first.index}, out);
                    else
                        fail("search_files", r, out);
                    if (s.content && !out.contentOff && !plan.minSizeSet && !plan.maxSizeSet) {
                        const Json c = tool("search_content", contentArgs(alt, plan, s));
                        ++out.calls;
                        logTool("search_content", alt, c);
                        if (!c.has("error")) {
                            absorb(c, "search_content", alt, true, 1, {first.index}, out);
                            if (c["complete"].isBool() && !c["complete"].toBool(true))
                                out.incomplete = true;
                        } else
                            fail("search_content", c, out);
                    }
                }
            }
        }

        // A format that is not there is not a reason to say "no such file":
        // if the content exists as another format, show it and say the type
        // was relaxed. (A .db is not a .sql script, so code/database stays.)
        if (allowRelax && plan.type.set && !isListing(plan) && !usable(plan, out) && relaxableType(plan.type.id)) {
            Plan relaxed = plan;
            relaxed.type = finder::TypeFilter {};
            SearchOutcome more = search(relaxed, false);
            mergeOutcome(out, more);
            out.typeRelaxed = true;
        }

        out.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        return out;
    }

    static bool relaxableType(const std::string& id)
    {
        return id == "pdf" || id == "word" || id == "excel" || id == "slides" || id == "documents"
            || id == "images" || id == "videos" || id == "audio" || id == "ebooks";
    }

    static bool isTypeAlias(const Plan& plan, const std::string& term)
    {
        if (!plan.type.set)
            return false;
        // 截图 is a kind of image, not the word "image": keep it as a search
        // term so screenshots are not every picture.
        if (term == "截图" || term == "截屏")
            return false;
        if (term == plan.type.label)
            return true;
        for (const auto& word : finder::detail::typeWords()) {
            if (plan.type.id != word.id)
                continue;
            for (const std::string& alias : finder::splitAny(word.terms, ",")) {
                if (alias == term)
                    return true;
            }
        }
        for (const std::string& ext : plan.type.extensions) {
            if (ext == finder::lowerAscii(term))
                return true;
        }
        return false;
    }

    static bool literal(const std::string& term)
    {
        return term.find('"') != std::string::npos || term.find('\\') != std::string::npos;
    }

    static bool genericTypeWord(const std::string& word)
    {
        static const char* const generic[] = {"视频", "图片", "照片", "表格", "ppt", "文档", "音频", "音乐",
            "压缩包", "安装包", "代码", "电子书", "文件夹", "pdf", "word", "excel"};
        for (const char* item : generic) {
            if (word == item)
                return true;
        }
        return false;
    }

    // "找视频", "上周的 PPT", "大于 1M 的文件": conditions/type only, no
    // sentence to embed and no content query worth running.
    static bool isListing(const Plan& plan)
    {
        if (!plan.concepts.empty())
            return false;
        if (plan.terms.empty())
            return true;
        return plan.type.set && !plan.type.folders && plan.terms.size() == 1
            && plan.terms.front() == plan.type.matched && genericTypeWord(plan.type.matched);
    }

    bool inScope(const std::string& path, const std::vector<std::string>& folders) const
    {
        if (folders.empty())
            return true;
        for (const std::string& folder : folders) {
            if (finder::lowerAscii(path) == finder::lowerAscii(folder))
                return true;
            std::string prefix = folder;
            if (!prefix.empty() && prefix.back() != '\\' && prefix.back() != '/')
                prefix += '\\';
            if (finder::startsWithNoCase(path, prefix))
                return true;
        }
        return false;
    }

    bool typeFits(const Plan& plan, const Candidate& c) const
    {
        if (!plan.type.set)
            return true;
        if (plan.type.folders)
            return c.folder;
        if (c.folder)
            return false;
        const std::string ext = finder::extensionOf(c.path);
        if (ext.empty())
            return true; // a file without an extension can still be the one
        return std::find(plan.type.extensions.begin(), plan.type.extensions.end(), ext) != plan.type.extensions.end();
    }

    bool timeFits(const Plan& plan, const Candidate& c) const
    {
        if (!plan.time.set || c.modified <= 0)
            return true;
        return c.modified >= plan.time.from && c.modified < plan.time.to;
    }

    bool sizeFits(const Plan& plan, const Candidate& c) const
    {
        if (c.size < 0)
            return true;
        if (plan.minSizeSet && c.size < plan.minSize)
            return false;
        if (plan.maxSizeSet && c.size > plan.maxSize)
            return false;
        return true;
    }

    bool excluded(const Plan& plan, const Candidate& c) const
    {
        for (const std::string& word : plan.exclude) {
            if (finder::containsNoCase(c.path, word) || finder::containsNoCase(c.snippet, word))
                return true;
        }
        return false;
    }

    // At least one result that the conditions still allow; otherwise the
    // broader search is worth its time.
    bool usable(const Plan& plan, const SearchOutcome& out) const
    {
        const Settings s = currentSettings();
        const std::vector<std::string> scope = !plan.folder.empty() ? std::vector<std::string> {plan.folder} : s.folders;
        for (const auto& [key, c] : out.candidates) {
            (void)key;
            if (typeFits(plan, c) && timeFits(plan, c) && sizeFits(plan, c) && inScope(c.path, scope) && !excluded(plan, c))
                return true;
        }
        return false;
    }

    // Which concepts this candidate really shows: the query that found it,
    // plus any term visible in its name or snippet.
    int coverage(const Plan& plan, Candidate& c) const
    {
        for (std::size_t i = 0; i < plan.concepts.size(); ++i) {
            const auto& concept = plan.concepts[i];
            if (c.concepts.count(concept.index))
                continue;
            bool seen = finder::containsTerm(c.name, concept.matched) || finder::containsTerm(c.snippet, concept.matched);
            if (!seen) {
                const finder::Concept& known = finder::concepts()[static_cast<std::size_t>(concept.index)];
                for (const char* term : known.terms) {
                    if (finder::containsTerm(c.name, term) || finder::containsTerm(c.snippet, term)) {
                        seen = true;
                        break;
                    }
                }
            }
            if (!seen) {
                const std::string group = conceptGroup(concept.id);
                const std::string ext = finder::extensionOf(c.path);
                if (!group.empty() && extensionGroup(ext) == group)
                    seen = true;
                // 数据库脚本 is .sql: the concept is the database, the type
                // word is what makes it textual.
                if (!seen && group == "database" && ext == "sql" && plan.type.set && plan.type.id == "code")
                    seen = true;
            }
            if (seen)
                c.concepts.insert(concept.index);
        }
        return static_cast<int>(c.concepts.size());
    }

    std::string explain(const Plan& plan, const Candidate& c) const
    {
        std::vector<std::string> labels;
        for (const auto& concept : plan.concepts) {
            if (c.concepts.count(concept.index) && std::find(labels.begin(), labels.end(), concept.label) == labels.end())
                labels.push_back(concept.label);
        }
        for (const std::string& term : plan.terms) {
            if (c.matched.count(term) && std::find(labels.begin(), labels.end(), term) == labels.end())
                labels.push_back(term);
        }
        return join(labels, "、");
    }

    int score(const Plan& plan, Candidate& c) const
    {
        const int covered = coverage(plan, c);
        c.covered = covered;
        const double conceptShare = plan.concepts.empty() ? (c.matched.empty() ? 0 : 1) : static_cast<double>(covered) / plan.concepts.size();
        int nameConcepts = 0;
        bool conceptInName = false;
        for (const auto& concept : plan.concepts) {
            bool inName = false;
            const finder::Concept& known = finder::concepts()[static_cast<std::size_t>(concept.index)];
            for (const char* term : known.terms) {
                if (finder::containsTerm(c.name, term)) {
                    inName = true;
                    break;
                }
            }
            if (inName) {
                ++nameConcepts;
                conceptInName = true;
            }
        }
        const double nameShare = plan.concepts.empty() ? (c.nameHit ? 1.0 : 0.0) : static_cast<double>(nameConcepts) / plan.concepts.size();

        // The host's own relevance is the strongest signal it gives us; the
        // concept coverage and the name only adjust it. Recency is a tie
        // breaker, never the reason a random recent file wins.
        int value = 22;
        value += static_cast<int>(26 * conceptShare);
        value += static_cast<int>(12 * nameShare);
        if (c.nameHit)
            value += 8;
        if (c.contentHit)
            value += 4;
        value += std::min(30, std::max(0, c.hostScore) * 30 / 100);
        value += std::max(0, 5 - c.bestRank / 3);
        if (c.phraseHit)
            value += 16;
        // The local encoder knows that a passage about "费用管理办法" is
        // closer to "报销的规定" than a random page that happens to contain
        // both words. The host's own score cannot tell those apart.
        if (c.semantic >= 0) {
            const double semantic = std::max(0.0, std::min(100.0, (static_cast<double>(c.semantic) - 0.30) / 0.40 * 100.0));
            const double weight = c.snippet.empty() ? 0.55 : 1.0;
            value += static_cast<int>((semantic - 50.0) * 0.7 * weight);
        }
        value += snippetProximity(plan, c.snippet);
        if (c.modified > 0) {
            const long long days = (finder::detail::nowSeconds() - c.modified) / 86400;
            if (days >= 0 && days < 7)
                value += 3;
            else if (days < 30)
                value += 2;
            else if (days < 365)
                value += 1;
        }
        if (plan.time.set && c.modified > 0 && c.modified >= plan.time.from && c.modified < plan.time.to)
            value += 2;

        const std::string wanted = preferredGroup(plan);
        if (!wanted.empty() && !c.folder) {
            const std::string group = extensionGroup(finder::extensionOf(c.path));
            if (group == wanted)
                value += 3;
            else if (group != "other" && group != "docs")
                value -= 2;
        }
        if (numericName(c.name) && !conceptInName)
            value -= 10;
        const std::string ext = finder::extensionOf(c.path);
        if (ext == "html" || ext == "htm" || c.snippet.find("<span") != std::string::npos
            || c.snippet.find("</") != std::string::npos || c.snippet.find("http") != std::string::npos)
            value -= 8;
        const std::string lowerPath = finder::lowerAscii(c.path);
        if (lowerPath.find("\\appdata\\") != std::string::npos || lowerPath.find("\\temp\\") != std::string::npos
            || lowerPath.find("\\cache\\") != std::string::npos || lowerPath.find("\\node_modules\\") != std::string::npos
            || lowerPath.find("\\.git\\") != std::string::npos)
            value -= 5;
        return std::max(1, std::min(99, value));
    }

    Items render(const Plan& plan, SearchOutcome& out, const std::string& prefix) const
    {
        const Settings s = currentSettings();
        const bool zh = chinese();
        const bool listing = isListing(plan);
        const bool sizeQuery = plan.minSizeSet || plan.maxSizeSet;
        // Programs and source code live in AppData and Program Files too;
        // documents and media there are usually noise.
        static const std::set<std::string> codeTypes = {"custom", "executables", "code", "javascript", "typescript", "python",
            "java", "cpp", "csharp", "golang", "rust", "php", "ruby", "swift", "kotlin", "dart", "lua", "perl",
            "scala", "matlab", "rlang", "web", "json", "yaml", "xml", "shell", "powershell", "markdown", "sql"};
        const bool hideSystem = (listing || sizeQuery) && !codeTypes.count(plan.type.id);
        const bool codeType = codeTypes.count(plan.type.id) != 0;

        // Windows settings are not files. When the sentence names a known
        // switch/page, put an actionable settings row above the file rows.
        Items settingsItems;
        if (!plan.type.set && !sizeQuery && !plan.time.set && plan.folder.empty()) {
            for (const finder::SettingsEntry* entry : finder::matchSettings(plan.text)) {
                Item item("s:" + std::string(entry->id), entry->title,
                          std::string(entry->subtitle) + (zh ? " · 系统设置" : " · Windows Settings"));
                if (entry->url)
                    item.withUrl(entry->target);
                else
                    item.withPath(entry->target);
                item.withIcon("glyph:U+E713").withCopy(entry->target).withPreview(entry->target)
                    .withData(Json::Object {{"kind", "setting"}, {"target", entry->target}});
                settingsItems.push_back(std::move(item));
            }
        }

        const std::vector<std::string> scope = !plan.folder.empty() ? std::vector<std::string> {plan.folder} : s.folders;
        std::vector<Candidate*> kept;
        int noise = 0;
        for (auto& [key, c] : out.candidates) {
            (void)key;
            if (!typeFits(plan, c) || !timeFits(plan, c) || !sizeFits(plan, c) || !inScope(c.path, scope) || excluded(plan, c))
                continue;
            // "大于 1M 的文件" should not be a tour of Windows logs and
            // browser caches: a listing leaves system and program folders
            // out unless the user named something (then it is not a listing).
            // A size condition on a real concept ("big database files") also
            // should not return AppData indexes before the user's own files.
            if (hideSystem && systemPath(c.path)) {
                ++noise;
                continue;
            }
            if (codeType && plan.type.id != "custom" && (listing || sizeQuery) && systemCodePath(c.path)) {
                ++noise;
                continue;
            }
            if (listing && noisePath(c.path)) {
                ++noise;
                continue;
            }
            c.covered = coverage(plan, c);
            if (c.fallbackOnly && !plan.concepts.empty() && c.covered == 0)
                continue;
            kept.push_back(&c);
        }
        semanticRerank(plan, kept);
        for (Candidate* c : kept)
            c->finalScore = score(plan, *c);
        int bestHost = 0;
        int strongCount = 0;
        for (const Candidate* c : kept) {
            if (c->contentHit)
                bestHost = std::max(bestHost, c->hostScore);
            const bool strong = c->nameHit || c->phraseHit || plan.concepts.empty()
                || c->covered >= static_cast<int>(plan.concepts.size());
            if (strong)
                ++strongCount;
        }
        // An exact phrase a title would use is the best evidence we have.
        // Keep those (and names that matched); a file that merely mentions
        // the words far apart is not worth a row when a real phrase exists.
        if (plan.concepts.size() >= 2 && out.phraseHits >= 1) {
            kept.erase(std::remove_if(kept.begin(), kept.end(), [&](const Candidate* c) {
                return !c->phraseHit && !c->nameHit;
            }), kept.end());
        }
        // With several concepts, a file that shows only one of them in the
        // one snippet Win顺 gives us is a weak guess. If there are enough
        // files that show all of them, keep the weak ones out.
        if (plan.concepts.size() >= 2 && strongCount >= 3) {
            kept.erase(std::remove_if(kept.begin(), kept.end(), [&](const Candidate* c) {
                return !(c->nameHit || c->phraseHit || c->covered >= static_cast<int>(plan.concepts.size()));
            }), kept.end());
        }
        // The content index returns one window per file, and its own score
        // separates "this file is about it" from "the words happen to be
        // somewhere". Drop the tail when the gap is clear, unless the name
        // itself matched or every concept is visible.
        if (bestHost >= 50) {
            kept.erase(std::remove_if(kept.begin(), kept.end(), [&](const Candidate* c) {
                return c->contentHit && !c->nameHit && c->covered < static_cast<int>(plan.concepts.size())
                    && c->hostScore * 100 < bestHost * 45;
            }), kept.end());
        }
        std::sort(kept.begin(), kept.end(), [](const Candidate* a, const Candidate* b) {
            if (a->finalScore != b->finalScore)
                return a->finalScore > b->finalScore;
            if (a->hostScore != b->hostScore)
                return a->hostScore > b->hostScore;
            if (a->modified != b->modified)
                return a->modified > b->modified;
            return a->path < b->path;
        });

        Items items;
        int limit = s.maxResults == 0 ? 200 : std::min<int>(s.maxResults, 200);
        if (s.maxResults != 0 && plan.concepts.size() >= 2 && out.phraseTried && out.phraseHits == 0)
            limit = std::min(limit, 6);
        bool anyStrong = false;
        for (const Candidate* c : kept) {
            if (c->nameHit || c->phraseHit || (!plan.concepts.empty() && c->covered >= static_cast<int>(plan.concepts.size())))
                anyStrong = true;
        }
        for (Candidate* c : kept) {
            if (static_cast<int>(items.size()) >= limit)
                break;
            const std::string why = explain(plan, *c);
            const std::string when = formatWhen(c->modified);
            const std::string size = humanSize(c->size);
            std::string subtitle;
            if (!why.empty()) {
                subtitle = (c->phraseHit ? (zh ? "同段命中 " : "same passage: ") : (zh ? "命中 " : "matched ")) + why;
            } else if (c->nameHit) {
                subtitle = zh ? "文件名相近" : "similar name";
            } else {
                subtitle = zh ? "内容相近" : "similar content";
            }
            if (!when.empty())
                subtitle += " · " + when;
            if (!size.empty())
                subtitle += " · " + size;
            if (s.snippets && !c->snippet.empty())
                subtitle += " · " + trimSnippet(c->snippet, zh ? 42 : 60);

            std::string title = c->folder ? c->name + "\\" : c->name;
            Item item("f:" + lowerKey(c->path), title, subtitle);
            item.withIcon("file:" + c->path)
                .withPath(c->path)
                .withScore(c->finalScore)
                .withCopy(c->path)
                .withModified(c->modified)
                .withSize(c->size)
                .withData(Json::Object {{"path", c->path}, {"kind", c->folder ? "folder" : "file"}});
            item.match = why.empty() ? c->name : why;
            std::string preview = c->path;
            if (!when.empty() || !size.empty())
                preview += "\n" + when + (!when.empty() && !size.empty() ? " · " : "") + size;
            if (!why.empty())
                preview += std::string("\n") + (zh ? "匹配：" : "matched: ") + why;
            if (!c->snippet.empty())
                preview += "\n\n" + trimSnippet(c->snippet, 240);
            item.withPreview(preview);
            items.push_back(std::move(item));
        }

        if (listing && kept.size() > static_cast<std::size_t>(limit)) {
            const std::size_t more = kept.size() - static_cast<std::size_t>(limit);
            items.push_back(Item("note-more", zh ? "还有 " + std::to_string(more) + " 条" : std::to_string(more) + " more",
                                     zh ? "设置 → 最多显示几条，0 为不限（最多 200）；也可以加文件名、文件夹或时间缩小范围"
                                        : "Raise Settings → Most results; 0 means unlimited (up to 200), or add a name, folder or time")
                                 .withIcon("glyph:U+E7BA")
                                 .invalid());
        }

        // What went wrong or was left out, after the rows that did work.
        if (!settingsItems.empty())
            items.insert(items.begin(), settingsItems.begin(), settingsItems.end());
        if (items.empty()) {
            if (out.denied) {
                items.push_back(Item("error-permission", zh ? "没有搜索权限" : "No permission to search",
                                         zh ? "设置 → 插件 → 随心搜：重新打开开关，允许 winshun:search" : "Settings → Plugins: turn it on and allow winshun:search")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            } else if (out.contentOff) {
                items.push_back(Item("error-content", zh ? "文件内容搜索没有打开" : "File content search is off",
                                         zh ? "设置 → 文件内容搜索；打开后就能按意思找，而不只是文件名" : "Settings → File content search; then it can look by meaning, not only by name")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            } else if (!out.failures.empty()) {
                items.push_back(Item("error-search", zh ? "搜索没有完成" : "The search did not finish",
                                         out.failures.front().message + (zh ? "（回车重试）" : " (Enter retries)"))
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            } else {
                std::string hint = zh ? "换一种说法，或去掉时间/类型限制" : "Try other words, or drop the time/type condition";
                if (plan.type.set)
                    hint = zh ? "不限制类型再试一次，或换一种说法" : "Try without the type, or other words";
                items.push_back(Item("none", zh ? "没有找到符合描述的文件" : "No file fits that description", hint)
                                     .withIcon("glyph:U+E721")
                                     .invalid());
                const std::string fallback = strictQuery(plan);
                if (!fallback.empty() && fallback.rfind("ext:", 0) != 0) {
                    const std::string text = prefix + fallback;
                    items.push_back(Item("fallback", zh ? "用 Win顺 搜索这些词：" + fallback : "Search WinShun for: " + fallback,
                                             zh ? "回车填入搜索框" : "Enter puts it in the box")
                                         .withIcon("glyph:U+E8A5")
                                         .withActions({{"fallback", zh ? "填入" : "Use", "Enter"}})
                                         .withData(Json::Object {{"text", text}}));
                }
            }
        } else {
            if (out.contentOff)
                items.push_back(Item("note-content", zh ? "内容搜索没开，只按文件名找" : "Content search is off; names only",
                                         zh ? "设置 → 文件内容搜索，可以按文件里写的意思找" : "Settings → File content search finds by what is written inside")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            else if (!out.failures.empty())
                items.push_back(Item("note-partial", zh ? "部分内容没有查完" : "Part of the search did not finish",
                                         out.failures.front().message)
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            else if (out.incomplete)
                items.push_back(Item("note-reading", zh ? "内容索引还在读，结果可能不全" : "The content index is still reading; results may be incomplete")
                                         .withIcon("glyph:U+E7BA")
                                         .invalid());
            else if (out.phraseTried && out.phraseHits == 0 && plan.concepts.size() >= 2 && !anyStrong)
                items.push_back(Item("note-phrase", zh ? "没有找到把这些意思放在一起的表述" : "No passage puts these ideas together",
                                         zh ? "下面只是分别提到这些词的文件，可以换一种说法" : "The rows only mention the words separately; try another wording")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            else if (out.typeRelaxed)
                items.push_back(Item("note-type", zh ? "按这个文件类型没找到，已放宽类型" : "No file of that type; the type was relaxed",
                                         zh ? "结果里可能是另一种格式" : "A row may be another format")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            else if (out.mediaFallback)
                items.push_back(Item("note-media", zh ? "按类型和时间列出" : "Listed by type and date",
                                         zh ? "图片/视频里没有可检索的文字，没能按内容判断" : "Pictures and videos have no text to compare")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
            else if (out.archiveFallback)
                items.push_back(Item("note-archive", zh ? "按类型和时间列出压缩包" : "Archives listed by type and date",
                                         zh ? "文件名没有匹配到原词，列的是该时间的压缩包" : "The name words missed; these are archives from that time")
                                     .withIcon("glyph:U+E7BA")
                                     .invalid());
        }
        if (listing && noise > 0)
            items.push_back(Item("note-noise", zh ? "已略过系统文件夹和开发缓存" : "System folders and build caches are left out",
                                     zh ? "列大文件时不把 Windows 日志、浏览器缓存和构建产物算进来" : "A size listing skips Windows logs, browser caches and build output")
                                 .withIcon("glyph:U+E7BA")
                                 .invalid());
        return items;
    }

    std::string strictQuery(const Plan& plan) const
    {
        std::vector<std::string> words;
        for (const auto& concept : plan.concepts)
            words.push_back(concept.matched);
        for (const std::string& term : plan.terms)
            words.push_back(term);
        if (words.empty() && plan.type.set && !plan.type.extensions.empty())
            return "ext:" + plan.type.extensions.front();
        return join(words, " ");
    }

    std::unordered_map<std::string, CacheEntry> m_cache;
    mutable finder::Embedder m_embedder;
    mutable bool m_embedderTried = false;
    mutable std::string m_embedderError;
    mutable std::vector<std::vector<float>> m_probeVector;
};

} // namespace

// Developer benchmark: retrieval accuracy and encoding speed for one model.
// Usage: ainwin.exe --bench <modelDir> <dataset.json>
static int runBench(const std::string& modelDir, const std::string& datasetPath)
{
    std::ifstream in(datasetPath, std::ios::binary);
    if (!in) {
        std::printf("{\"error\":\"cannot open dataset\"}\n");
        return 1;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    const auto parsed = winshun::Json::parse(buffer.str());
    if (!parsed) {
        std::printf("{\"error\":\"bad dataset json\"}\n");
        return 1;
    }
    const winshun::Json& dataset = *parsed;
    const auto docs = dataset["docs"].toArray();
    const auto queries = dataset["queries"].toArray();
    if (docs.empty() || queries.empty()) {
        std::printf("{\"error\":\"dataset needs docs and queries\"}\n");
        return 1;
    }

    std::vector<std::string> docTexts;
    std::vector<std::string> docIds;
    for (const auto& doc : docs) {
        docIds.push_back(doc["id"].toString());
        docTexts.push_back(doc["text"].toString());
    }
    std::vector<std::string> queryTexts;
    std::vector<std::vector<std::string>> relevant;
    for (const auto& query : queries) {
        queryTexts.push_back(query["text"].toString());
        std::vector<std::string> ids;
        for (const auto& id : query["relevant"].toArray())
            ids.push_back(id.toString());
        relevant.push_back(ids);
    }

    int threads = 4;
    if (const char* value = std::getenv("AINWIN_THREADS")) {
        const int n = std::atoi(value);
        if (n >= 1 && n <= 8)
            threads = n;
    }
    const int maxLength = 128;
    finder::Embedder embedder;
    const auto allStart = std::chrono::steady_clock::now();
    const auto loadStart = std::chrono::steady_clock::now();
    if (!embedder.load(modelDir, threads, maxLength)) {
        std::printf("{\"error\":\"%s\"}\n", embedder.error().c_str());
        return 1;
    }
    const long long loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loadStart).count();

    std::vector<std::vector<float>> docVectors;
    std::vector<std::vector<float>> queryVectors;
    std::string error;
    const auto docsStart = std::chrono::steady_clock::now();
    if (!embedder.embed(docTexts, false, docVectors, error)) {
        std::printf("{\"error\":\"%s\"}\n", error.c_str());
        return 1;
    }
    const long long docsMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - docsStart).count();

    const auto queriesStart = std::chrono::steady_clock::now();
    if (!embedder.embed(queryTexts, true, queryVectors, error)) {
        std::printf("{\"error\":\"%s\"}\n", error.c_str());
        return 1;
    }
    const long long queriesMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - queriesStart).count();

    std::unordered_map<std::string, std::size_t> docIndex;
    for (std::size_t i = 0; i < docIds.size(); ++i)
        docIndex[docIds[i]] = i;

    const auto rankStart = std::chrono::steady_clock::now();
    double sumRecall1 = 0;
    double sumRecall5 = 0;
    double sumRecall10 = 0;
    double sumMrr = 0;
    double sumNdcg = 0;
    double sumTop1 = 0;
    int counted = 0;
    winshun::Json::Array perQuery;
    for (std::size_t qi = 0; qi < queryTexts.size(); ++qi) {
        std::vector<std::pair<float, std::size_t>> scored;
        scored.reserve(docVectors.size());
        for (std::size_t di = 0; di < docVectors.size(); ++di)
            scored.emplace_back(finder::Embedder::cosine(queryVectors[qi], docVectors[di]), di);
        std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

        std::set<std::size_t> wanted;
        for (const std::string& id : relevant[qi]) {
            const auto found = docIndex.find(id);
            if (found != docIndex.end())
                wanted.insert(found->second);
        }
        if (wanted.empty())
            continue;

        int firstRank = 0;
        for (int rank = 1; rank <= 10 && rank <= static_cast<int>(scored.size()); ++rank) {
            if (wanted.count(scored[rank - 1].second)) {
                firstRank = rank;
                break;
            }
        }
        int hits1 = 0;
        int hits5 = 0;
        int hits10 = 0;
        for (int rank = 1; rank <= 10 && rank <= static_cast<int>(scored.size()); ++rank) {
            if (!wanted.count(scored[rank - 1].second))
                continue;
            if (rank <= 1) ++hits1;
            if (rank <= 5) ++hits5;
            if (rank <= 10) ++hits10;
        }
        double dcg = 0;
        for (int rank = 1; rank <= 10 && rank <= static_cast<int>(scored.size()); ++rank) {
            if (wanted.count(scored[rank - 1].second))
                dcg += 1.0 / std::log2(static_cast<double>(rank) + 1.0);
        }
        double idcg = 0;
        const int ideal = std::min<int>(10, static_cast<int>(wanted.size()));
        for (int rank = 1; rank <= ideal; ++rank)
            idcg += 1.0 / std::log2(static_cast<double>(rank) + 1.0);

        sumRecall1 += static_cast<double>(hits1) / wanted.size();
        sumRecall5 += static_cast<double>(hits5) / wanted.size();
        sumRecall10 += static_cast<double>(hits10) / wanted.size();
        sumMrr += firstRank > 0 ? 1.0 / firstRank : 0.0;
        sumNdcg += idcg > 0 ? dcg / idcg : 0.0;
        sumTop1 += firstRank == 1 ? 1.0 : 0.0;
        ++counted;
        perQuery.push_back(winshun::Json::Object {{"query", queryTexts[qi]}, {"first_rank", firstRank},
            {"top1", scored.empty() ? "" : docIds[scored[0].second]},
            {"top1_score", scored.empty() ? 0.0 : scored[0].first}});
    }
    const long long rankMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - rankStart).count();
    const long long totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - allStart).count();
    const double n = counted > 0 ? static_cast<double>(counted) : 1.0;

    winshun::Json out = winshun::Json::Object {
        {"model", embedder.modelName()},
        {"dim", embedder.dimension()},
        {"threads", threads},
        {"max_length", maxLength},
        {"docs", static_cast<long long>(docTexts.size())},
        {"queries", static_cast<long long>(counted)},
        {"load_ms", loadMs},
        {"docs_ms", docsMs},
        {"queries_ms", queriesMs},
        {"rank_ms", rankMs},
        {"total_ms", totalMs},
        {"query_ms_avg", static_cast<double>(queriesMs) / n},
        {"doc_ms_avg", static_cast<double>(docsMs) / static_cast<double>(docTexts.size())},
        {"accuracy@1", sumTop1 / n},
        {"recall@1", sumRecall1 / n},
        {"recall@5", sumRecall5 / n},
        {"recall@10", sumRecall10 / n},
        {"mrr@10", sumMrr / n},
        {"ndcg@10", sumNdcg / n},
        {"per_query", perQuery},
    };
    std::printf("%s\n", out.dump().c_str());
    return 0;
}

int main(int argc, char** argv)
{
    const std::string command = argc >= 3 ? finder::argToUtf8(argv[1]) : std::string();
    if (command == "--appfiles" || command == "--wechat") {
        const std::string text = finder::argToUtf8(argv[2]);
        const finder::Plan plan = finder::parsePlan(text);
        for (const finder::AppFile& file : finder::findAppFiles(plan, "", 50))
            std::printf("%s\t%s\t%s\t%s\t%s\n", file.source.c_str(), file.name.c_str(),
                formatWhen(file.modified).c_str(), humanSize(file.size).c_str(), file.path.c_str());
        return 0;
    }
    if (command == "--settings") {
        const std::string text = finder::argToUtf8(argv[2]);
        for (const finder::SettingsEntry* entry : finder::matchSettings(text, 10))
            std::printf("%s\t%s\t%s\n", entry->id, entry->title, entry->target);
        return 0;
    }
    if (argc >= 4 && finder::argToUtf8(argv[1]) == "--bench")
        return runBench(finder::argToUtf8(argv[2]), finder::argToUtf8(argv[3]));

    // Developer aid: check the local encoder without Win顺.
    if (argc >= 3 && finder::argToUtf8(argv[1]) == "--embed") {
        std::string modelDir = "models\\bge-small-zh-v1.5-int8";
        if (const char* envModel = std::getenv("AINWIN_MODEL"))
            modelDir = envModel;
        std::vector<std::string> texts;
        for (int i = 2; i < argc; ++i)
            texts.push_back(finder::argToUtf8(argv[i]));
        finder::Embedder embedder;
        int embedThreads = 4;
        if (const char* envThreads = std::getenv("AINWIN_THREADS")) {
            const int n = std::atoi(envThreads);
            if (n >= 1 && n <= 8)
                embedThreads = n;
        }
        const auto loadStarted = std::chrono::steady_clock::now();
        if (!embedder.load(modelDir, embedThreads, 192)) {
            std::fprintf(stderr, "load failed: %s\n", embedder.error().c_str());
            return 1;
        }
        const long long loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loadStarted).count();
        std::vector<std::vector<float>> query;
        std::string error;
        const auto queryStarted = std::chrono::steady_clock::now();
        if (!embedder.embed({texts.front()}, true, query, error)) {
            std::fprintf(stderr, "embed failed: %s\n", error.c_str());
            return 1;
        }
        const long long queryMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - queryStarted).count();
        std::vector<std::vector<float>> docs;
        const auto docsStarted = std::chrono::steady_clock::now();
        if (texts.size() > 1 && !embedder.embed(std::vector<std::string>(texts.begin() + 1, texts.end()), false, docs, error)) {
            std::fprintf(stderr, "embed failed: %s\n", error.c_str());
            return 1;
        }
        const long long docsMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - docsStarted).count();
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loadStarted).count();
        Json cosines(Json::Array {});
        for (const auto& doc : docs)
            cosines.push(Json(static_cast<double>(finder::Embedder::cosine(query.front(), doc))));
        Json out = Json::Object {{"model", embedder.modelName()}, {"dim", embedder.dimension()}, {"ms", ms},
            {"load_ms", loadMs}, {"query_ms", queryMs}, {"docs_ms", docsMs}, {"cos", cosines}};
        std::printf("%s\n", out.dump().c_str());
        return 0;
    }

    // A developer aid, not part of the protocol: show how a sentence is read.
    if (argc >= 2 && finder::argToUtf8(argv[1]) == "--plan") {
        std::string text;
        for (int i = 2; i < argc; ++i) {
            if (i > 2)
                text += ' ';
            text += finder::argToUtf8(argv[i]);
        }
        const Plan plan = finder::parsePlan(text);
        Json concepts(Json::Array {});
        for (const auto& c : plan.concepts) {
            Json one = Json::Object {{"id", c.id}, {"label", c.label}, {"matched", c.matched}};
            concepts.push(one);
        }
        Json terms(Json::Array {});
        for (const auto& t : plan.terms)
            terms.push(t);
        Json exclude(Json::Array {});
        for (const auto& t : plan.exclude)
            exclude.push(t);
        Json out = Json::Object {{"text", plan.text}, {"concepts", concepts}, {"terms", terms}, {"exclude", exclude},
            {"time", Json::Object {{"set", plan.time.set}, {"from", plan.time.from}, {"to", plan.time.to}, {"label", plan.time.label}}},
            {"type", Json::Object {{"set", plan.type.set}, {"id", plan.type.id}, {"label", plan.type.label}}},
            {"folder", plan.folder}, {"minSize", plan.minSizeSet ? Json(plan.minSize) : Json(nullptr)},
            {"maxSize", plan.maxSizeSet ? Json(plan.maxSize) : Json(nullptr)}};
        std::string textOut = out.dump();
        std::printf("%s\n", textOut.c_str());
        return 0;
    }
    return Plugin().serve();
}


