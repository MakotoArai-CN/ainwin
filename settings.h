#pragma once

#include "common.h"

#include <algorithm>
#include <string>
#include <vector>

namespace finder {

struct SettingsEntry {
    const char* id;
    const char* title;
    const char* subtitle;
    const char* target;
    bool url;
    const char* aliases;
};

inline const std::vector<SettingsEntry>& settingsEntries()
{
    static const std::vector<SettingsEntry> all = {
        {"env", "编辑环境变量", "系统属性 → 高级 → 环境变量", "C:\\Windows\\System32\\SystemPropertiesAdvanced.exe", false,
            "环境变量,系统变量,用户变量,path变量,环境参数,environment variable,environment variables,env"},
        {"display", "显示设置", "分辨率、缩放、多显示器", "ms-settings:display", true,
            "显示设置,分辨率,缩放,多显示器,显示器,display,resolution,scale"},
        {"nightlight", "夜间模式", "护眼、色温", "ms-settings:nightlight", true,
            "夜间模式,护眼模式,色温,night light,nightlight"},
        {"sound", "声音设置", "扬声器、麦克风、音量", "ms-settings:sound", true,
            "声音设置,扬声器,音量,输出设备,sound,speaker,volume"},
        {"bluetooth", "蓝牙设置", "配对、开关", "ms-settings:bluetooth", true,
            "蓝牙,bluetooth"},
        {"network", "网络设置", "网络状态、适配器", "ms-settings:network", true,
            "网络设置,网络状态,网卡,network"},
        {"wifi", "WLAN 设置", "Wi-Fi 开关、已知网络", "ms-settings:network-wifi", true,
            "wifi,wlan,无线网络,无线网,wi-fi"},
        {"proxy", "代理设置", "手动代理、自动代理", "ms-settings:network-proxy", true,
            "代理,代理服务器,proxy"},
        {"vpn", "VPN 设置", "添加或管理 VPN", "ms-settings:network-vpn", true,
            "vpn,虚拟专用网络"},
        {"power", "电源和睡眠", "休眠、屏幕关闭时间", "ms-settings:powersleep", true,
            "电源,睡眠,休眠,熄屏,power,sleep"},
        {"battery", "电池设置", "省电模式、电池使用情况", "ms-settings:batterysaver", true,
            "电池,省电模式,battery"},
        {"storage", "存储设置", "磁盘空间、存储感知", "ms-settings:storagesense", true,
            "存储设置,磁盘空间,存储感知,清理磁盘,storage,disk space"},
        {"apps", "应用和功能", "卸载、修改应用", "ms-settings:appsfeatures", true,
            "应用和功能,卸载程序,卸载应用,apps,uninstall"},
        {"defaultapps", "默认应用", "按文件类型选择默认程序", "ms-settings:defaultapps", true,
            "默认应用,默认程序,文件关联,default apps,default programs"},
        {"startup", "启动应用", "开机启动项", "ms-settings:startupapps", true,
            "启动项,开机启动,启动应用,startup"},
        {"update", "Windows 更新", "检查更新、更新历史", "ms-settings:windowsupdate", true,
            "windows更新,系统更新,检查更新,windows update,update"},
        {"security", "Windows 安全中心", "病毒防护、防火墙", "ms-settings:windowsdefender", true,
            "windows安全,安全中心,病毒防护,defender,security"},
        {"firewall", "防火墙设置", "Windows Defender 防火墙", "C:\\Windows\\System32\\firewall.cpl", false,
            "防火墙,firewall"},
        {"privacy", "隐私设置", "权限、诊断数据", "ms-settings:privacy", true,
            "隐私,隐私设置,privacy"},
        {"camera", "摄像头权限", "哪些应用可以用摄像头", "ms-settings:privacy-webcam", true,
            "摄像头权限,相机权限,camera"},
        {"microphone", "麦克风权限", "哪些应用可以用麦克风", "ms-settings:privacy-microphone", true,
            "麦克风权限,话筒权限,microphone"},
        {"notifications", "通知设置", "通知、专注助手", "ms-settings:notifications", true,
            "通知设置,通知,notifications"},
        {"focus", "专注助手", "免打扰、专注时间", "ms-settings:quiethours", true,
            "专注助手,免打扰,quiet hours,focus assist"},
        {"datetime", "日期和时间", "时区、自动同步", "ms-settings:dateandtime", true,
            "日期和时间,时间设置,时区,date,time,timezone"},
        {"language", "语言和区域", "显示语言、输入法", "ms-settings:regionlanguage", true,
            "语言设置,输入法,区域,language,region,ime"},
        {"fonts", "字体设置", "安装、预览字体", "ms-settings:fonts", true,
            "字体设置,字体,fonts"},
        {"mouse", "鼠标和触摸板", "指针速度、触摸板手势", "ms-settings:mousetouchpad", true,
            "鼠标设置,触摸板,指针速度,mouse,touchpad"},
        {"keyboard", "键盘设置", "键盘布局、快捷键", "ms-settings:keyboard", true,
            "键盘设置,键盘,keyboard"},
        {"about", "关于本机", "设备规格、Windows 版本", "ms-settings:about", true,
            "关于本机,系统信息,设备规格,about,system info"},
        {"remote", "远程桌面", "允许远程连接", "ms-settings:remotedesktop", true,
            "远程桌面,远程连接,remote desktop"},
        {"developers", "开发者模式", "开发人员设置", "ms-settings:developers", true,
            "开发者模式,开发人员模式,developer mode"},
        {"services", "服务", "Windows 服务管理", "C:\\Windows\\System32\\services.msc", false,
            "服务,服务管理,services.msc,services"},
        {"devicemgr", "设备管理器", "驱动、硬件设备", "C:\\Windows\\System32\\devmgmt.msc", false,
            "设备管理器,驱动,硬件,device manager,devmgmt"},
        {"regedit", "注册表编辑器", "注册表", "C:\\Windows\\regedit.exe", false,
            "注册表,注册表编辑器,regedit,registry"},
        {"control", "控制面板", "经典控制面板", "C:\\Windows\\System32\\control.exe", false,
            "控制面板,control panel,control"},
        {"programs", "程序和功能", "卸载、启用 Windows 功能", "C:\\Windows\\System32\\appwiz.cpl", false,
            "程序和功能,卸载程序,启用或关闭windows功能,programs and features"},
        {"powercfg", "电源选项", "电源计划、高级电源设置", "C:\\Windows\\System32\\powercfg.cpl", false,
            "电源选项,电源计划,power options,powercfg"},
        {"netconnections", "网络连接", "适配器、网络连接", "C:\\Windows\\System32\\ncpa.cpl", false,
            "网络连接,网络适配器,网卡设置,network connections,ncpa"},
        {"msinfo", "系统信息", "硬件、组件、环境", "C:\\Windows\\System32\\msinfo32.exe", false,
            "系统信息,硬件信息,msinfo32,system information"},
        {"taskmgr", "任务管理器", "进程、启动、性能", "C:\\Windows\\System32\\taskmgr.exe", false,
            "任务管理器,进程管理,task manager,taskmgr"},
        {"cmd", "命令提示符", "打开 cmd", "C:\\Windows\\System32\\cmd.exe", false,
            "命令提示符,命令行,cmd,command prompt"},
    };
    return all;
}

inline std::vector<const SettingsEntry*> matchSettings(const std::string& text, int limit = 3)
{
    const std::string lower = lowerAscii(text);
    struct Hit {
        const SettingsEntry* entry;
        std::size_t score;
    };
    std::vector<Hit> hits;
    for (const SettingsEntry& entry : settingsEntries()) {
        std::size_t best = 0;
        for (const std::string& alias : splitAny(entry.aliases, ",")) {
            if (alias.size() < 2)
                continue;
            if (lower.find(lowerAscii(alias)) != std::string::npos)
                best = std::max(best, alias.size());
        }
        if (best > 0)
            hits.push_back(Hit {&entry, best});
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
    std::vector<const SettingsEntry*> out;
    for (const Hit& hit : hits) {
        if (static_cast<int>(out.size()) >= limit)
            break;
        out.push_back(hit.entry);
    }
    return out;
}

} // namespace finder
