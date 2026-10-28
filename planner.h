// From a sentence to what to look for: the concepts in it, words it did not
// recognise, and the conditions that are not words (time, type, folder,
// size, what to leave out). Nothing here touches Win顺 or the disk.
#pragma once

#include "common.h"
#include "lexicon.h"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <set>
#include <string>
#include <vector>

namespace finder {

struct ConceptMatch {
    int index = -1;
    std::string id;
    std::string label;
    std::string matched; // the word the user used
};

struct TimeRange {
    bool set = false;
    long long from = 0;
    long long to = 0;
    std::string label;
};

struct TypeFilter {
    bool set = false;
    std::string id;
    std::string label;
    std::vector<std::string> extensions;
    bool folders = false;
    std::string matched; // the word the user used, when it is worth searching
};

struct Plan {
    std::string text;
    std::vector<ConceptMatch> concepts;
    std::vector<std::string> terms; // leftovers, quoted phrases first
    std::vector<std::string> exclude;
    TimeRange time;
    TypeFilter type;
    std::string folder; // a prefix, as typed/expanded
    std::string folderLabel;
    bool minSizeSet = false;
    bool maxSizeSet = false;
    long long minSize = 0;
    long long maxSize = 0;

    bool empty() const { return concepts.empty() && terms.empty(); }
};

namespace detail {

inline void eraseAt(std::string& s, std::size_t at, std::size_t length)
{
    if (at == std::string::npos || at > s.size())
        return;
    s.erase(at, length);
}

inline std::tm localNow()
{
    const std::time_t now = std::time(nullptr);
    std::tm tm {};
    localtime_s(&tm, &now);
    return tm;
}

inline long long epochOf(int year, int month, int day)
{
    std::tm tm {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    const std::time_t t = std::mktime(&tm);
    return t == static_cast<std::time_t>(-1) ? 0 : static_cast<long long>(t);
}

inline void addMonths(int& year, int& month, int delta)
{
    int total = year * 12 + (month - 1) + delta;
    year = total / 12;
    month = total % 12 + 1;
}

inline int daysInMonth(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month >= 1 && month <= 12 ? month - 1 : 0];
}

inline long long parseNumber(const std::string& s)
{
    long long n = 0;
    for (const char c : s) {
        if (c < '0' || c > '9')
            break;
        n = n * 10 + (c - '0');
        if (n > 1000000)
            break;
    }
    return n;
}

// 三, 十二, 二十三: the way people write "最近三天". Returns 0 when there
// is no such number at `at`, and how many bytes it took.
inline long long parseChineseNumber(const std::string& s, std::size_t at, std::size_t& used)
{
    struct Digit { const char* word; int value; };
    static const Digit digits[] = {{"零", 0}, {"一", 1}, {"二", 2}, {"两", 2}, {"三", 3}, {"四", 4}, {"五", 5},
        {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}};
    used = 0;
    if (at >= s.size())
        return 0;
    if (s.compare(at, 3, "十") == 0) {
        used = 3;
        long long value = 10;
        for (const Digit& d : digits) {
            if (s.compare(at + 3, std::string(d.word).size(), d.word) == 0) {
                used += std::string(d.word).size();
                value += d.value;
                break;
            }
        }
        return value;
    }
    long long tens = 0;
    std::size_t pos = at;
    for (const Digit& d : digits) {
        if (s.compare(pos, std::string(d.word).size(), d.word) == 0) {
            tens = d.value;
            pos += std::string(d.word).size();
            break;
        }
    }
    if (pos == at)
        return 0;
    if (s.compare(pos, 3, "十") == 0) {
        used = pos - at + 3;
        long long value = tens * 10;
        for (const Digit& d : digits) {
            if (s.compare(pos + 3, std::string(d.word).size(), d.word) == 0) {
                used += std::string(d.word).size();
                value += d.value;
                break;
            }
        }
        return value;
    }
    used = pos - at;
    return tens;
}

inline long long nowSeconds()
{
    return static_cast<long long>(std::time(nullptr));
}

// The range of a named period, or set=false.
inline TimeRange namedRange(const std::string& word)
{
    const std::tm now = localNow();
    const int year = now.tm_year + 1900;
    const int month = now.tm_mon + 1;
    const int day = now.tm_mday;
    const long long end = nowSeconds() + 1;
    const long long today = epochOf(year, month, day);
    const long long yesterday = today - 86400;
    const long long dayBefore = today - 2 * 86400;
    const int weekday = now.tm_wday == 0 ? 7 : now.tm_wday; // Monday = 1
    const long long monday = today - static_cast<long long>(weekday - 1) * 86400;
    TimeRange r;
    r.set = true;
    if (word == "今天" || word == "今日" || word == "today") {
        r.from = today;
        r.to = end;
        r.label = "今天";
    } else if (word == "昨天" || word == "昨日" || word == "yesterday") {
        r.from = yesterday;
        r.to = today;
        r.label = "昨天";
    } else if (word == "前天") {
        r.from = dayBefore;
        r.to = yesterday;
        r.label = "前天";
    } else if (word == "本周" || word == "这周" || word == "这星期" || word == "this week") {
        r.from = monday;
        r.to = end;
        r.label = "本周";
    } else if (word == "上周" || word == "上星期" || word == "上个星期" || word == "last week") {
        r.from = monday - 7 * 86400;
        r.to = monday;
        r.label = "上周";
    } else if (word == "本月" || word == "这个月" || word == "this month") {
        r.from = epochOf(year, month, 1);
        r.to = end;
        r.label = "本月";
    } else if (word == "上月" || word == "上个月" || word == "last month") {
        int y = year;
        int m = month - 1;
        if (m < 1) {
            m = 12;
            --y;
        }
        r.from = epochOf(y, m, 1);
        r.to = epochOf(year, month, 1);
        r.label = "上个月";
    } else if (word == "今年" || word == "this year") {
        r.from = epochOf(year, 1, 1);
        r.to = end;
        r.label = "今年";
    } else if (word == "去年" || word == "上一年" || word == "last year") {
        r.from = epochOf(year - 1, 1, 1);
        r.to = epochOf(year, 1, 1);
        r.label = "去年";
    } else if (word == "前年") {
        r.from = epochOf(year - 2, 1, 1);
        r.to = epochOf(year - 1, 1, 1);
        r.label = "前年";
    } else if (word == "上半年") {
        r.from = epochOf(year, 1, 1);
        r.to = epochOf(year, 7, 1);
        r.label = "今年上半年";
    } else if (word == "下半年") {
        r.from = epochOf(year, 7, 1);
        r.to = epochOf(year + 1, 1, 1);
        r.label = "今年下半年";
    } else {
        r.set = false;
    }
    return r;
}

inline bool parseTime(std::string& work, TimeRange& out)
{
    // 2024-05 / 2024/05
    for (std::size_t i = 0; i + 6 < work.size(); ++i) {
        const bool yearOk = std::isdigit(static_cast<unsigned char>(work[i])) && std::isdigit(static_cast<unsigned char>(work[i + 1]))
            && std::isdigit(static_cast<unsigned char>(work[i + 2])) && std::isdigit(static_cast<unsigned char>(work[i + 3]));
        if (!yearOk || (work[i + 4] != '-' && work[i + 4] != '/'))
            continue;
        if (!std::isdigit(static_cast<unsigned char>(work[i + 5])) || !std::isdigit(static_cast<unsigned char>(work[i + 6])))
            continue;
        const int year = static_cast<int>(parseNumber(work.substr(i, 4)));
        const int month = static_cast<int>(parseNumber(work.substr(i + 5, 2)));
        if (year < 1990 || year > 2100 || month < 1 || month > 12)
            continue;
        out.set = true;
        out.from = epochOf(year, month, 1);
        int ny = year;
        int nm = month + 1;
        if (nm > 12) {
            nm = 1;
            ++ny;
        }
        out.to = epochOf(ny, nm, 1);
        out.label = std::to_string(year) + " 年 " + std::to_string(month) + " 月";
        eraseAt(work, i, 7);
        return true;
    }

    // 2024年5月 / 2024年 / 5月
    {
        const std::size_t nian = work.find("年");
        if (nian != std::string::npos && nian >= 4) {
            std::string digits = work.substr(nian - 4, 4);
            bool all = true;
            for (const char c : digits)
                all = all && std::isdigit(static_cast<unsigned char>(c));
            if (all) {
                const int year = static_cast<int>(parseNumber(digits));
                const std::size_t yue = work.find("月", nian + 3);
                if (yue != std::string::npos && yue > nian + 3 && yue - (nian + 3) <= 2) {
                    const int month = static_cast<int>(parseNumber(work.substr(nian + 3, yue - (nian + 3))));
                    if (month >= 1 && month <= 12) {
                        out.set = true;
                        out.from = epochOf(year, month, 1);
                        int ny = year;
                        int nm = month + 1;
                        if (nm > 12) {
                            nm = 1;
                            ++ny;
                        }
                        out.to = epochOf(ny, nm, 1);
                        out.label = std::to_string(year) + " 年 " + std::to_string(month) + " 月";
                        eraseAt(work, nian - 4, yue + 3 - (nian - 4));
                        return true;
                    }
                }
                out.set = true;
                out.from = epochOf(year, 1, 1);
                out.to = epochOf(year + 1, 1, 1);
                out.label = std::to_string(year) + " 年";
                eraseAt(work, nian - 4, 7);
                return true;
            }
        }
    }

    // 最近 3 天 / 近两周 / 过去 6 个月 / 3天前
    static const char* const recent[] = {"最近", "近", "过去", "past", "last"};
    for (const char* lead : recent) {
        const std::size_t at = work.find(lead);
        if (at == std::string::npos)
            continue;
        std::size_t p = at + std::string(lead).size();
        while (p < work.size() && work[p] == ' ')
            ++p;
        if (work.compare(p, 9, "半个月") == 0) {
            out.set = true;
            out.from = nowSeconds() - 15 * 86400;
            out.to = nowSeconds() + 1;
            out.label = "最近半个月";
            eraseAt(work, at, p - at + 9);
            return true;
        }
        if (work.compare(p, 6, "半年") == 0) {
            out.set = true;
            out.from = nowSeconds() - 180 * 86400;
            out.to = nowSeconds() + 1;
            out.label = "最近半年";
            eraseAt(work, at, p - at + 6);
            return true;
        }
        const std::size_t digitsAt = p;
        while (p < work.size() && std::isdigit(static_cast<unsigned char>(work[p])))
            ++p;
        long long n = 0;
        if (p > digitsAt) {
            n = parseNumber(work.substr(digitsAt, p - digitsAt));
        } else {
            std::size_t used = 0;
            n = parseChineseNumber(work, p, used);
            p += used;
        }
        if (n <= 0)
            continue;
        while (p < work.size() && work[p] == ' ')
            ++p;
        const std::string rest = work.substr(p, 12);
        long long seconds = 0;
        std::string label;
        std::size_t consumed = 0;
        if (rest.rfind("天", 0) == 0 || rest.rfind("日", 0) == 0 || rest.rfind("day", 0) == 0) {
            seconds = n * 86400;
            label = "最近 " + std::to_string(n) + " 天";
            consumed = rest.rfind("day", 0) == 0 ? 3 : 3;
        } else if (rest.rfind("周", 0) == 0 || rest.rfind("个星期", 0) == 0 || rest.rfind("week", 0) == 0) {
            seconds = n * 7 * 86400;
            label = "最近 " + std::to_string(n) + " 周";
            consumed = rest.rfind("个星期", 0) == 0 ? 9 : (rest.rfind("week", 0) == 0 ? 4 : 3);
        } else if (rest.rfind("个月", 0) == 0 || rest.rfind("月", 0) == 0 || rest.rfind("month", 0) == 0) {
            seconds = n * 30 * 86400;
            label = "最近 " + std::to_string(n) + " 个月";
            consumed = rest.rfind("个月", 0) == 0 ? 6 : (rest.rfind("month", 0) == 0 ? 5 : 3);
        } else if (rest.rfind("年", 0) == 0 || rest.rfind("year", 0) == 0) {
            seconds = n * 365 * 86400;
            label = "最近 " + std::to_string(n) + " 年";
            consumed = rest.rfind("year", 0) == 0 ? 4 : 3;
        }
        if (seconds > 0) {
            out.set = true;
            out.from = nowSeconds() - seconds;
            out.to = nowSeconds() + 1;
            out.label = label;
            eraseAt(work, at, p - at + consumed);
            return true;
        }
    }

    // 3 天前 / 三天前 / 两个月前
    {
        const std::size_t qian = work.find("前");
        if (qian != std::string::npos && qian >= 3) {
            std::size_t unitLength = 0;
            long long multiplier = 0;
            if (work.compare(qian - 3, 3, "天") == 0 || work.compare(qian - 3, 3, "日") == 0) {
                unitLength = 3;
                multiplier = 1;
            } else if (work.compare(qian - 3, 3, "周") == 0) {
                unitLength = 3;
                multiplier = 7;
            } else if (qian >= 6 && work.compare(qian - 6, 6, "个月") == 0) {
                unitLength = 6;
                multiplier = 30;
            } else if (work.compare(qian - 3, 3, "年") == 0) {
                unitLength = 3;
                multiplier = 365;
            }
            if (unitLength > 0) {
                const std::size_t numEnd = qian - unitLength;
                std::size_t start = numEnd;
                while (start > 0 && std::isdigit(static_cast<unsigned char>(work[start - 1])))
                    --start;
                long long n = 0;
                if (start < numEnd) {
                    n = parseNumber(work.substr(start, numEnd - start));
                } else {
                    for (std::size_t width : {std::size_t(3), std::size_t(6), std::size_t(9)}) {
                        if (numEnd < width)
                            continue;
                        std::size_t used = 0;
                        const long long candidate = parseChineseNumber(work, numEnd - width, used);
                        if (candidate > 0 && numEnd - width + used == numEnd) {
                            n = candidate;
                            start = numEnd - width;
                            break;
                        }
                    }
                }
                if (n > 0 && n < 1000) {
                    const long long day = n * multiplier;
                    const long long dayStart = epochOf(localNow().tm_year + 1900, localNow().tm_mon + 1, localNow().tm_mday) - day * 86400;
                    out.set = true;
                    out.from = dayStart;
                    out.to = dayStart + 86400;
                    out.label = std::to_string(n) + (multiplier == 7 ? " 周前" : multiplier == 30 ? " 个月前" : multiplier == 365 ? " 年前" : " 天前");
                    eraseAt(work, start, qian + 3 - start);
                    return true;
                }
            }
        }
    }

    static const char* const names[] = {"上个月", "上个星期", "这星期", "上星期", "这个月", "本周", "这周", "上周",
        "本月", "上月", "今天", "昨天", "前天", "今年", "去年", "前年", "上一年", "上半年", "下半年",
        "yesterday", "today", "last week", "this week", "last month", "this month", "last year", "this year"};
    for (const char* name : names) {
        const std::size_t at = findTerm(work, name);
        if (at == std::string::npos)
            continue;
        const TimeRange r = namedRange(name);
        if (!r.set)
            continue;
        out = r;
        eraseAt(work, at, std::string(name).size());
        return true;
    }
    return false;
}

struct TypeWord {
    const char* id;
    const char* label;
    const char* terms;
    const char* extensions;
    bool folders;
    bool keepTerm;
};

inline const std::vector<TypeWord>& typeWords()
{
    static const std::vector<TypeWord> all = {
        {"folders", "文件夹", "文件夹,目录,folders,folder", "", true, false},
        {"pdf", "PDF", "pdf", "pdf", false, false},
        {"word", "Word 文档", "word,docx,doc", "doc,docm,docx,dot,dotm,dotx,rtf,odt,ott,wps,wpt", false, false},
        {"excel", "表格", "excel,xlsx,xls,工作表,表格", "xls,xlsx,xlsm,xlsb,xlt,xltx,xltm,csv,tsv,ods,ots,et,ett,fods,gnumeric,sxc", false, true},
        {"slides", "PPT", "ppt,pptx,幻灯片,演示文稿,presentation,slides,keynote", "ppt,pptx,pptm,pps,ppsm,ppsx,pot,potm,potx,odp,otp,dps,dpt,key", false, true},
        {"sql", "SQL 脚本", "sql,psql,sql脚本", "sql,psql", false, false},
        {"javascript", "JavaScript", "javascript,js,node,nodejs,ecmascript", "js,mjs,cjs,jsx", false, false},
        {"typescript", "TypeScript", "typescript,ts语言", "ts,tsx,mts,cts", false, false},
        {"python", "Python", "python,py", "py,pyw,ipynb", false, false},
        {"java", "Java", "java", "java,class,jar", false, false},
        {"cpp", "C/C++", "c++,cpp,c语言", "c,cpp,cc,cxx,h,hpp,hxx", false, false},
        {"csharp", "C#", "c#,csharp", "cs", false, false},
        {"golang", "Go", "golang,go语言", "go", false, false},
        {"rust", "Rust", "rust", "rs", false, false},
        {"php", "PHP", "php", "php,phtml", false, false},
        {"ruby", "Ruby", "ruby", "rb,erb", false, false},
        {"swift", "Swift", "swift", "swift", false, false},
        {"kotlin", "Kotlin", "kotlin", "kt,kts", false, false},
        {"dart", "Dart", "dart", "dart", false, false},
        {"lua", "Lua", "lua", "lua", false, false},
        {"perl", "Perl", "perl", "pl,pm", false, false},
        {"scala", "Scala", "scala", "scala,sc", false, false},
        {"matlab", "MATLAB", "matlab", "m,mlx", false, false},
        {"rlang", "R", "r语言,rstudio", "r,rds,rdata", false, false},
        {"web", "网页", "html,htm,css,scss,sass,less,vue,网页", "html,htm,xhtml,css,scss,sass,less,vue,svelte,astro", false, true},
        {"json", "JSON", "json,jsonl,ndjson", "json,jsonl,ndjson", false, false},
        {"yaml", "YAML", "yaml,yml", "yaml,yml", false, false},
        {"xml", "XML", "xml,xslt", "xml,xslt,xsd,svg", false, false},
        {"shell", "Shell", "shell,bash,sh脚本", "sh,bash,zsh,ksh,fish", false, false},
        {"powershell", "PowerShell", "powershell,ps1", "ps1,psm1,psd1", false, false},
        {"markdown", "Markdown", "markdown,md文件", "md,markdown,mdown", false, false},
        {"executables", "可执行文件", "可执行文件,可执行程序,exe文件,应用程序,binary,executable,程序文件", "exe,dll,sys,scr,cpl,ocx,com,msi,msix,msixbundle,appx,appxbundle,msu,app,apk,ipa,dmg,pkg,deb,rpm,jar,bin,elf,so,dylib,run,appimage,bat,cmd,ps1,vbs", false, false},
        {"bioinfo", "生物信息", "生物信息,基因,测序,bioinformatics,基因组", "fasta,fa,fna,fastq,fq,sam,bam,cram,vcf,bcf,bed,gtf,gff,gff3,ab1,snap,czi,lif,lsm,oib,oif,nii,dcm", false, true},
        {"chemistry", "化学", "化学,分子,化合物,chemistry,分子结构", "mol,mol2,sdf,smi,smiles,cdx,cdxml,pdb,cif,mmcif,xyz,inchi,rxn,cml", false, true},
        {"astronomy", "天文", "天文,星表,astronomy,天体", "fits,fit,fts", false, true},
        {"statistics", "统计", "统计软件,spss,stata,sas,统计分析", "sav,dta,sas7bdat,xpt,rds,rdata,mat,npy,npz,h5,hdf5,parquet,feather,arrow,orc,avro,pkl,pickle,joblib", false, true},
        {"audiowork", "音频工程", "音频工程,编曲,录音工程,daw,音乐工程", "aup3,als,flp,logicx,ptf,ptx,rpp,cpr,npr,sf2,sfz,omf,aaf", false, true},
        {"videowork", "视频工程", "视频工程,剪辑工程,视频项目,premiere,aftereffects", "prproj,aep,veg,fcpxml,drp,mogrt,braw,r3d,crm,mxf,prores", false, true},
        {"rawphoto", "相机原片", "相机原片,raw照片,cr2,nef,arw,摄影原片", "cr2,cr3,nef,arw,dng,orf,rw2,raf,pef,srw,x3f,raw,heic,xmp,lrcat", false, true},
        {"threed", "三维", "三维,3d,三维模型,3d打印,建模", "stl,obj,3mf,amf,gcode,step,stp,iges,igs,fbx,blend,max,c4d,ma,mb,skp,usd,usdz,gltf,glb,ply,dae,abc,vdb", false, true},
        {"pcb", "电路", "电路,pcb,电路板,eda,原理图", "sch,brd,pcbdoc,schdoc,gbr,ger,drl,gtl,gbl,gts,gbs,gto,gbo,kicad_pcb,kicad_sch,epro,altium,dsn", false, true},
        {"mech", "机械", "机械,cnc,数控,加工,零件图", "sldprt,sldasm,slddrw,ipt,iam,idw,catpart,catproduct,prt,asm,f3d,nc,tap,cnc,gcode,step,stp,iges,igs,stl,dwg,dxf", false, true},
        {"bim", "建筑", "建筑,bim,施工,revit,archicad", "rvt,rfa,ifc,pln,pla,gbxml,nwc,nwd,nwf,skp,dwg,dxf,bcf", false, true},
        {"finance", "财务数据", "财务数据,财务报表,xbrl,ofx,qif", "xbrl,ofx,qif,qbo,qbw,mt940,940,942,camt,iso20022", false, true},
        {"certkeys", "证书密钥", "证书文件,密钥文件,openssl,pem证书", "cer,crt,pem,der,pfx,p12,key,pub,gpg,asc,csr,jks,keystore,ovpn,kdbx,ppk", false, true},
        {"captures", "抓包", "抓包,网络包,pcap,网络流量", "pcap,pcapng,cap,har,net,trc", false, true},
        {"gameassets", "游戏资源", "游戏资源,游戏素材,assetbundle,游戏资产", "pak,uasset,umap,unitypackage,assetbundle,wad,pk3,vpk,bsa,ba2,esp,esm,nif,dds", false, true},
        {"virtual", "虚拟机", "虚拟机,虚拟磁盘,vm,虚拟机镜像", "vhd,vhdx,vmdk,vdi,qcow2,qcow,ova,ovf,iso,img,wim,dmg,esd,swm", false, true},
        {"backups", "备份文件", "备份文件,backup,bak文件", "bak,backup,bkp,old,orig,shadow,sqlitedb", false, true},
        {"database", "数据库", "数据库文件,db文件,sqlite,mdb,accdb,数据库表", "db,db3,sqlite,sqlite3,mdb,accdb,dbf,frm,ibd,myd,myi,mdf,ndf,ldf,parquet,avro,orc,duckdb,realm", false, false},
        {"email", "邮件", "邮件,email,eml,msg,邮箱,邮件存档", "eml,msg,mbox,pst,ost,oft", false, false},
        {"cad", "图纸", "图纸,cad,施工图,dwg,dxf,三维模型,3d模型,stl,step,iges,sketchup", "dwg,dxf,dgn,stp,step,igs,iges,stl,obj,fbx,3ds,blend,max,skp,rvt,ifc,3mf,amf,gcode", false, false},
        {"fonts", "字体", "字体,font,字库", "ttf,otf,woff,woff2,ttc,fon,pfb,pfm", false, false},
        {"subtitles", "字幕", "字幕,subtitle,srt", "srt,ass,ssa,vtt,sub,idx,sup", false, false},
        {"medical", "医学影像", "医学影像,dicom,dcm,影像", "dcm,dicom,nii", false, false},
        {"gis", "地图", "地图,gis,地理数据,shapefile", "shp,shx,dbf,geojson,kml,kmz,gpx,dem,las,laz,topojson", false, false},
        {"design", "设计稿", "设计稿,设计源文件,figma,sketch源文件,indesign", "psd,ai,eps,indd,idml,fig,sketch,xd,afdesign,afphoto,cdr", false, false},
        {"images", "图片", "图片,照片,相片,图像,截图,截屏,壁纸,image,images,picture,photo,photos,screenshot", "jpg,jpeg,jfif,png,gif,bmp,webp,heic,heif,avif,jxl,tif,tiff,ico,svg,psd,ai,eps,raw,cr2,cr3,nef,arw,dng,orf,rw2,exr,hdr,dds,ktx,tga,pcx,pbm,pgm,ppm", false, true},
        {"videos", "视频", "视频,影片,电影,录像,movie,video,videos", "mp4,mkv,avi,mov,wmv,flv,f4v,webm,m4v,mpg,mpeg,m2v,ts,mts,m2ts,3gp,ogv,rm,rmvb,vob,mxf,asf,dv,wtv", false, true},
        {"audio", "音频", "音乐,歌曲,音频,录音,audio,music,song", "mp3,wav,flac,aac,m4a,m4b,ogg,opus,wma,ape,aiff,aif,aifc,alac,mid,midi,amr,caf,dsf,dff,wv", false, true},
        {"disks", "镜像", "镜像,系统镜像,disk image,iso镜像", "iso,img,vhd,vhdx,vmdk,qcow2,wim,dmg,ova,ovf", false, false},
        {"archives", "压缩包", "压缩包,压缩文件,archive,zip,rar,7z", "zip,zipx,rar,7z,tar,gz,tgz,bz2,xz,zst,lz4,lzma,cab,lzh,arj,ace", false, true},
        {"installers", "安装包", "安装包,安装程序,installer,setup", "exe,msi,msix,msixbundle,appx,appxbundle,msu,bat,cmd,vbs,com,lnk,apk,ipa,jar,deb,rpm,pkg", false, true},
        {"ebooks", "电子书", "电子书,ebook,epub", "epub,mobi,azw,azw3,kfx,fb2,djvu,chm,cbz,cbr,cb7,lit,ibooks,pdb", false, true},
        {"data", "数据", "数据集,数据表,表格数据,csv,json,parquet", "csv,tsv,json,jsonl,ndjson,xml,yaml,yml,parquet,avro,orc,feather,arrow,hdf5,h5,npy,npz,mat,sav,dta,rds,rdata", false, false},
        {"code", "代码", "代码,源码,源代码,脚本,code,source,script", "c,h,cpp,hpp,cc,cxx,cs,java,kt,kts,py,js,mjs,cjs,ts,jsx,tsx,go,rs,swift,rb,php,lua,pl,pm,tcl,vb,asm,s,sol,zig,nim,cr,ex,exs,erl,hs,ml,fs,fsx,clj,groovy,gradle,cmake,proto,thrift,graphql,gql,tf,hcl,nix,v,d,pas,f90,jl,wasm,wat,html,htm,css,scss,sass,less,json,xml,yaml,yml,toml,ini,sql,sh,ps1,qml,vue,ipynb,r,m,mm,scala,dart,tex", false, false},
        {"logs", "日志", "日志,日志文件,log", "log,evtx,etl", false, false},
        {"torrents", "种子", "种子,torrent,bt种子", "torrent", false, false},
        {"documents", "文档", "文档,document,documents", "doc,docm,docx,dot,dotm,dotx,rtf,odt,ott,wps,wpd,txt,md,markdown,pdf,xls,xlsx,xlsm,xlsb,csv,ods,ots,et,ett,ppt,pptx,pptm,pps,ppsx,odp,dps,vsd,vsdx,one,pages,numbers,key,ofd,xps,oxps,tex,hwp", false, false},
    };
    return all;
}

inline bool parseExplicitType(std::string& work, TypeFilter& out)
{
    std::vector<std::string> exts;
    auto add = [&](const std::string& raw) {
        std::string ext = raw;
        while (!ext.empty() && (ext.front() == '.' || ext.front() == '*'))
            ext.erase(ext.begin());
        if (const std::size_t dot = ext.find_last_of('.'); dot != std::string::npos && dot + 1 < ext.size())
            ext = ext.substr(dot + 1);
        ext = lowerAscii(ext);
        if (ext.size() < 1 || ext.size() > 12)
            return;
        for (const char c : ext) {
            if (!std::isalnum(static_cast<unsigned char>(c)))
                return;
        }
        if (std::find(exts.begin(), exts.end(), ext) == exts.end())
            exts.push_back(ext);
    };

    // ext:parquet,avro  /  ext: .dwg
    for (std::size_t at = work.find("ext:"); at != std::string::npos; at = work.find("ext:", at)) {
        if (at > 0 && std::isalnum(static_cast<unsigned char>(work[at - 1])))
            break;
        std::size_t p = at + 4;
        while (p < work.size() && (std::isalnum(static_cast<unsigned char>(work[p])) || work[p] == ',' || work[p] == '.' || work[p] == ' '))
            ++p;
        const std::string list = work.substr(at + 4, p - at - 4);
        for (const std::string& item : splitAny(list, ", "))
            add(item);
        eraseAt(work, at, p - at);
    }

    // *.dwg / *.tar.gz
    for (std::size_t at = work.find("*."); at != std::string::npos; at = work.find("*.", at)) {
        std::size_t p = at + 2;
        while (p < work.size() && (std::isalnum(static_cast<unsigned char>(work[p])) || work[p] == '.'))
            ++p;
        add(work.substr(at + 2, p - at - 2));
        eraseAt(work, at, p - at);
    }

    // .dwg, but only for an extension the type table knows: a sentence with
    // "1.5" or "example.com" must not become a file-type filter.
    static const std::set<std::string> known = [] {
        std::set<std::string> set;
        for (const TypeWord& tw : typeWords()) {
            for (const std::string& ext : splitAny(tw.extensions, ","))
                set.insert(ext);
        }
        return set;
    }();
    for (std::size_t at = work.find('.'); at != std::string::npos; at = work.find('.', at + 1)) {
        std::size_t p = at + 1;
        while (p < work.size() && std::isalnum(static_cast<unsigned char>(work[p])))
            ++p;
        const std::string ext = lowerAscii(work.substr(at + 1, p - at - 1));
        if (ext.empty() || !known.count(ext))
            continue;
        const bool boundary = at == 0 || work[at - 1] == ' ' || work[at - 1] == '*' || static_cast<unsigned char>(work[at - 1]) >= 0x80;
        if (!boundary)
            continue;
        add(ext);
        eraseAt(work, at, p - at);
    }

    if (exts.empty())
        return false;
    out.set = true;
    out.id = "custom";
    out.label.clear();
    for (const std::string& ext : exts) {
        if (!out.label.empty())
            out.label += '/';
        out.label += ext;
    }
    out.extensions = exts;
    out.folders = false;
    out.matched = std::string();
    return true;
}

inline bool parseType(std::string& work, TypeFilter& out)
{
    struct Match {
        const TypeWord* word;
        std::string term;
        std::size_t at;
    };
    std::vector<Match> matches;
    for (const TypeWord& tw : typeWords()) {
        std::vector<std::string> terms = splitAny(tw.terms, ",");
        std::sort(terms.begin(), terms.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        for (const std::string& term : terms) {
            const std::size_t at = findTerm(work, term);
            if (at == std::string::npos)
                continue;
            matches.push_back(Match {&tw, term, at});
            break;
        }
    }
    if (matches.empty())
        return false;
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        if (a.at != b.at)
            return a.at < b.at;
        return a.term.size() < b.term.size();
    });

    // The head noun wins: 微信文件夹的压缩包 is an archive, not a folder.
    // Two exceptions: a concrete format beats the generic word 文档, and
    // sql/python/shell before 脚本 is the real format, not generic code.
    std::size_t primary = matches.size() - 1;
    const Match& tail = matches.back();
    if (std::strcmp(tail.word->id, "documents") == 0) {
        for (std::size_t i = matches.size() - 1; i-- > 0;) {
            if (std::strcmp(matches[i].word->id, "documents") != 0) {
                primary = i;
                break;
            }
        }
    } else if (std::strcmp(tail.word->id, "code") == 0
        && (tail.term == "脚本" || tail.term == "代码" || tail.term == "源码" || tail.term == "源代码"
            || tail.term == "script" || tail.term == "source" || tail.term == "code")) {
        for (std::size_t i = matches.size() - 1; i-- > 0;) {
            if (std::strcmp(matches[i].word->id, "code") != 0 && std::strcmp(matches[i].word->id, "documents") != 0) {
                primary = i;
                break;
            }
        }
    }

    // Remove every type word, merging overlaps such as sql脚本 + 脚本.
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    for (const Match& match : matches)
        spans.emplace_back(match.at, match.at + match.term.size());
    std::sort(spans.begin(), spans.end());
    std::vector<std::pair<std::size_t, std::size_t>> merged;
    for (const auto& span : spans) {
        if (!merged.empty() && span.first <= merged.back().second) {
            merged.back().second = std::max(merged.back().second, span.second);
        } else {
            merged.push_back(span);
        }
    }
    for (std::size_t i = merged.size(); i-- > 0;)
        eraseAt(work, merged[i].first, merged[i].second - merged[i].first);

    const TypeWord* chosen = matches[primary].word;
    out.set = true;
    out.id = chosen->id;
    out.label = chosen->label;
    out.folders = chosen->folders;
    out.extensions = splitAny(chosen->extensions, ",");
    out.matched = chosen->keepTerm ? matches[primary].term : std::string();
    return true;
}

inline bool parseExtraType(std::string& work, TypeFilter& out, const std::string& spec)
{
    for (const std::string& item : splitAny(spec, ";")) {
        const std::size_t eq = item.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string label = trim(item.substr(0, eq));
        std::vector<std::string> exts = splitAny(item.substr(eq + 1), ",");
        for (std::string& ext : exts)
            ext = lowerAscii(trim(ext));
        if (label.empty() || exts.empty())
            continue;
        const std::size_t at = findTerm(work, label);
        if (at == std::string::npos)
            continue;
        out.set = true;
        out.id = "custom";
        out.label = label;
        out.extensions = exts;
        out.folders = false;
        out.matched = std::string();
        eraseAt(work, at, label.size());
        return true;
    }
    return false;
}

inline std::string userProfile()
{
    wchar_t buffer[32768];
    const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buffer, 32768);
    if (n == 0 || n >= 32768)
        return {};
    return utf8Of(std::wstring(buffer, n));
}

inline bool parseFolder(std::string& work, std::string& folder, std::string& label)
{
    // An explicit path: D:\work or C:/work
    for (std::size_t i = 0; i + 2 < work.size(); ++i) {
        const bool drive = std::isalpha(static_cast<unsigned char>(work[i])) && work[i + 1] == ':' && (work[i + 2] == '\\' || work[i + 2] == '/');
        if (!drive)
            continue;
        std::size_t end = i;
        while (end < work.size() && work[end] != ' ' && work[end] != ',' && work[end] != ';' && work[end] != '"' && work[end] != '\'')
            ++end;
        folder = work.substr(i, end - i);
        folder = replaceAll(folder, "/", "\\");
        while (folder.size() > 3 && (folder.back() == '\\' || folder.back() == '/'))
            folder.pop_back();
        label = folder;
        eraseAt(work, i, end - i);
        return true;
    }

    struct Named { const char* words; const char* sub; const char* label; };
    static const Named named[] = {
        {"桌面,desktop", "Desktop", "桌面"},
        {"下载,downloads", "Downloads", "下载"},
        {"我的文档", "Documents", "文档"},
    };
    const std::string home = userProfile();
    for (const Named& n : named) {
        for (const std::string& word : splitAny(n.words, ",")) {
            const std::size_t at = findTerm(work, word);
            if (at == std::string::npos)
                continue;
            if (home.empty())
                return false;
            folder = home + "\\" + n.sub;
            label = n.label;
            eraseAt(work, at, word.size());
            return true;
        }
    }

    // D盘 / C 盘
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        const std::string upper = std::string(1, drive) + "盘";
        const std::string lower = std::string(1, static_cast<char>(drive + 32)) + "盘";
        const std::size_t at = work.find(upper);
        if (at != std::string::npos) {
            folder = std::string(1, drive) + ":\\";
            label = std::string(1, drive) + " 盘";
            eraseAt(work, at, upper.size());
            return true;
        }
        const std::size_t at2 = work.find(lower);
        if (at2 != std::string::npos) {
            folder = std::string(1, drive) + ":\\";
            label = std::string(1, drive) + " 盘";
            eraseAt(work, at2, lower.size());
            return true;
        }
    }
    return false;
}

inline long long parseSizeNumber(const std::string& digits, const std::string& unit)
{
    const long long n = parseNumber(digits);
    const std::string u = lowerAscii(unit);
    if (u.rfind("tb", 0) == 0 || u.rfind("t", 0) == 0 || u.rfind("太", 0) == 0)
        return n * 1024LL * 1024 * 1024 * 1024;
    if (u.rfind("gb", 0) == 0 || u.rfind("g", 0) == 0 || u.rfind("吉", 0) == 0)
        return n * 1024LL * 1024 * 1024;
    if (u.rfind("mb", 0) == 0 || u.rfind("m", 0) == 0 || u.rfind("兆", 0) == 0)
        return n * 1024LL * 1024;
    if (u.rfind("kb", 0) == 0 || u.rfind("k", 0) == 0 || u.rfind("千", 0) == 0)
        return n * 1024LL;
    return n;
}

inline bool parseSize(std::string& work, Plan& plan)
{
    struct Lead { const char* words; bool minimum; };
    static const Lead leads[] = {
        {"大于,超过,至少,不小于,多于,>", true},
        {"小于,不到,不超过,最多,少于,<", false},
    };
    for (const Lead& lead : leads) {
        for (const std::string& word : splitAny(lead.words, ",")) {
            const std::size_t at = work.find(word);
            if (at == std::string::npos)
                continue;
            std::size_t p = at + word.size();
            while (p < work.size() && work[p] == ' ')
                ++p;
            const std::size_t digitsAt = p;
            while (p < work.size() && (std::isdigit(static_cast<unsigned char>(work[p])) || work[p] == '.'))
                ++p;
            if (p == digitsAt)
                continue;
            std::string digits = work.substr(digitsAt, p - digitsAt);
            while (p < work.size() && work[p] == ' ')
                ++p;
            std::string unit;
            while (p < work.size() && std::isalpha(static_cast<unsigned char>(work[p])) && unit.size() < 4) {
                unit += work[p];
                ++p;
            }
            if (unit.empty()) {
                static const char* const chineseUnits[] = {"兆字节", "吉字节", "千字节", "字节", "兆", "吉", "千", "太"};
                for (const char* candidate : chineseUnits) {
                    const std::size_t length = std::string(candidate).size();
                    if (work.compare(p, length, candidate) == 0) {
                        unit = candidate;
                        p += length;
                        break;
                    }
                }
            }
            if (unit.empty())
                continue;
            const long long bytes = parseSizeNumber(digits, unit);
            if (bytes <= 0)
                continue;
            if (lead.minimum) {
                plan.minSizeSet = true;
                plan.minSize = bytes;
            } else {
                plan.maxSizeSet = true;
                plan.maxSize = bytes;
            }
            eraseAt(work, at, p - at);
            return true;
        }
    }
    return false;
}

inline bool parseExclusions(std::string& work, std::vector<std::string>& out)
{
    static const char* const leads[] = {"不要", "除了", "不含", "不包含", "排除", "除去", "去掉", "but not", "except"};
    for (const char* lead : leads) {
        const std::size_t at = work.find(lead);
        if (at == std::string::npos)
            continue;
        std::size_t p = at + std::string(lead).size();
        while (p < work.size() && work[p] == ' ')
            ++p;
        const std::size_t start = p;
        int counted = 0;
        while (p < work.size() && counted < 12) {
            if (work[p] == ' ' || work[p] == ',' || work[p] == ';' || work[p] == '.' || work[p] == '"' || work[p] == '\'')
                break;
            if (work.compare(p, 3, "的") == 0 || work.compare(p, 3, "，") == 0 || work.compare(p, 3, "。") == 0)
                break;
            p = utf8Next(work, p);
            ++counted;
        }
        std::string term = trim(work.substr(start, p - start));
        if (!term.empty()) {
            out.push_back(term);
            eraseAt(work, at, p - at);
            return true;
        }
    }
    return false;
}

inline void addTerm(Plan& plan, const std::string& term)
{
    const std::string t = trim(term);
    if (t.size() < 2)
        return;
    if (isAscii(t) && t.size() < 3)
        return;
    if (std::find(plan.terms.begin(), plan.terms.end(), t) == plan.terms.end())
        plan.terms.push_back(t);
}

inline bool isStopToken(const std::string& t)
{
    for (const char* word : stopwords()) {
        if (t == word)
            return true;
    }
    return false;
}

inline std::string stripParticles(std::string t)
{
    static const char* const front[] = {"帮我", "请", "找", "查", "搜", "看", "要", "把", "那个", "这个"};
    static const char* const back[] = {"的", "了", "吗", "呢", "啊", "吧", "呀", "嘛"};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const char* p : front) {
            if (t.rfind(p, 0) == 0 && t.size() > std::string(p).size()) {
                t = t.substr(std::string(p).size());
                changed = true;
            }
        }
        for (const char* p : back) {
            const std::string s(p);
            if (t.size() > s.size() && t.compare(t.size() - s.size(), s.size(), s) == 0) {
                t = t.substr(0, t.size() - s.size());
                changed = true;
            }
        }
    }
    return trim(t);
}

inline void parseConcepts(std::string& work, Plan& plan)
{
    // Longest term first across the whole lexicon: 数据库 must win over
    // 数据, 会议记录 over 会议. Matching concept by concept in lexicon order
    // let a shorter, earlier concept eat the longer word.
    struct Term {
        std::string text;
        int index;
    };
    std::vector<Term> terms;
    for (std::size_t i = 0; i < concepts().size(); ++i) {
        for (const char* term : concepts()[i].terms)
            terms.push_back({term, static_cast<int>(i)});
    }
    std::sort(terms.begin(), terms.end(), [](const Term& a, const Term& b) { return a.text.size() > b.text.size(); });

    std::set<int> taken;
    for (const Term& term : terms) {
        if (plan.concepts.size() >= 6)
            break;
        const std::size_t at = findTerm(work, term.text);
        if (at == std::string::npos)
            continue;
        if (!taken.count(term.index)) {
            const Concept& c = concepts()[static_cast<std::size_t>(term.index)];
            plan.concepts.push_back({term.index, c.id, c.zh, term.text});
            taken.insert(term.index);
        }
        for (std::size_t again = findTerm(work, term.text); again != std::string::npos; again = findTerm(work, term.text))
            eraseAt(work, again, term.text.size());
    }
}

} // namespace detail

// Reads a description; never throws, never touches anything outside `text`.
inline Plan parsePlan(const std::string& text, const std::string& extraTypes = {})
{
    using namespace detail;
    Plan plan;
    plan.text = trim(text);
    std::string work = normalize(text);

    // Quoted words are what the user means exactly: keep them as terms.
    for (std::size_t at = work.find('"'); at != std::string::npos; at = work.find('"', at)) {
        const std::size_t end = work.find('"', at + 1);
        if (end == std::string::npos)
            break;
        const std::string quoted = trim(work.substr(at + 1, end - at - 1));
        addTerm(plan, quoted);
        eraseAt(work, at, end - at + 1);
    }

    parseExclusions(work, plan.exclude);
    parseTime(work, plan.time);
    parseSize(work, plan);
    std::string folder;
    std::string folderLabel;
    if (parseFolder(work, folder, folderLabel)) {
        plan.folder = folder;
        plan.folderLabel = folderLabel;
    }
    if (!parseExtraType(work, plan.type, extraTypes) && !parseExplicitType(work, plan.type))
        parseType(work, plan.type);
    if (plan.type.set && !plan.type.matched.empty())
        addTerm(plan, plan.type.matched);

    parseConcepts(work, plan);

    // Generic phrases that are not worth searching.
    static const char* const filler[] = {"帮我找一下", "帮我找", "帮忙找", "找一下", "搜索一下", "查一下", "看一下",
        "我想找", "我想要", "需要找", "有没有", "关于", "有关", "相关", "里面", "里提到", "中提到", "提到",
        "写到", "写到过", "说的是", "讲的是", "那份", "这份", "内容", "文件名", "文件", "东西", "资料", "材料", "扫描件", "里的"};
    for (const char* phrase : filler)
        work = replaceAll(work, phrase, " ");
    work = replaceAll(work, "的", " ");
    work = replaceAll(work, "记录", " ");

    // What is left: words the knowledge base did not know.
    std::vector<std::string> tokens = splitAny(work, " \t\r\n,;:.!?()[]{}<>|/\\*\"'");
    for (std::string token : tokens) {
        token = stripParticles(token);
        if (token.empty() || isStopToken(token))
            continue;
        if (utf8Length(token) < 2 && !std::isdigit(static_cast<unsigned char>(token[0])))
            continue;
        if (utf8Length(token) == 1 && isAscii(token))
            continue;
        addTerm(plan, token);
        // An unknown phrase ending in a generic noun is often a known word
        // plus that noun: 报账材料 → 报账. A phrase like 服务器迁移 is not.
        static const char* const genericTails[] = {"材料", "文件", "记录", "报告", "申请", "单据", "表格",
            "清单", "合同", "发票", "证明", "资料", "文档", "扫描件"};
        if (!isAscii(token) && utf8Length(token) >= 4) {
            for (const char* tail : genericTails) {
                const std::string suffix(tail);
                if (token.size() > suffix.size() && token.compare(token.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    addTerm(plan, utf8Prefix(token, 2));
                    break;
                }
            }
        }
        if (plan.terms.size() >= 6)
            break;
    }
    return plan;
}

} // namespace finder
