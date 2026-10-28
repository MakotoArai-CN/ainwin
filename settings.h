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
            "环境变量,系统变量,用户变量,path变量,环境参数,python,python路径,java,java路径,path,environment variable,environment variables,env"},
        {"display", "显示设置", "分辨率、缩放、多显示器", "ms-settings:display", true,
            "显示设置,屏幕亮度,屏幕太亮,调暗,亮度,亮度调节,不黑屏,黑屏,双屏,主屏,分辨率,缩放,多显示器,显示器,display,brightness,resolution,scale"},
        {"nightlight", "夜间模式", "护眼、色温", "ms-settings:nightlight", true,
            "夜间模式,护眼模式,色温,night light,nightlight"},
        {"sound", "声音设置", "扬声器、麦克风、音量", "ms-settings:sound", true,
            "声音设置,调声音,声音,耳机,扬声器,麦克风,话筒,录音,没声音,回音,音量,输出设备,音频,sound,speaker,microphone,volume"},
        {"bluetooth", "蓝牙设置", "配对、开关", "ms-settings:bluetooth", true,
            "蓝牙,bluetooth"},
        {"network", "网络设置", "网络状态、适配器", "ms-settings:network", true,
            "网络设置,网络状态,网卡,网络适配器,network,internet,网络和internet,adapter"},
        {"wifi", "WLAN 设置", "Wi-Fi 开关、已知网络", "ms-settings:network-wifi", true,
            "wifi,wlan,无线网络,无线网,wi-fi"},
        {"proxy", "代理设置", "手动代理、自动代理", "ms-settings:network-proxy", true,
            "代理,代理服务器,proxy"},
        {"vpn", "VPN 设置", "添加或管理 VPN", "ms-settings:network-vpn", true,
            "vpn,虚拟专用网络"},
        {"power", "电源和睡眠", "休眠、屏幕关闭时间", "ms-settings:powersleep", true,
            "电源,系统电源,电源设置,电源和睡眠,睡眠,休眠,熄屏,关机,不睡觉,一直亮,合盖,合上盖子,黑屏,power,sleep"},
        {"battery", "电池设置", "省电模式、电池使用情况", "ms-settings:batterysaver", true,
            "电池,省电模式,battery"},
        {"storage", "存储设置", "磁盘空间、存储感知", "ms-settings:storagesense", true,
            "存储设置,磁盘空间,c盘爆满,清理垃圾,临时文件,存储感知,清理磁盘,storage,disk space"},
        {"apps", "应用和功能", "卸载、修改应用", "ms-settings:appsfeatures", true,
            "应用和功能,卸载软件,卸载残留,卸载程序,卸载应用,apps,uninstall"},
        {"defaultapps", "默认应用", "按文件类型选择默认程序", "ms-settings:defaultapps", true,
            "默认应用,默认程序,默认打开方式,打开方式,默认浏览器,默认邮件,默认播放器,文件关联,default apps,default programs"},
        {"startup", "启动应用", "开机启动项", "ms-settings:startupapps", true,
            "启动项,开机启动,开机自启,自启动,启动应用,电脑慢,电脑快,开机慢,加速,startup"},
        {"update", "Windows 更新", "检查更新、更新历史", "ms-settings:windowsupdate", true,
            "windows更新,系统更新,更新和安全,更新重启,自动更新,关闭自动更新,关闭更新,检查更新,windows update,update"},
        {"security", "Windows 安全中心", "病毒防护、防火墙", "ms-settings:windowsdefender", true,
            "windows安全,安全中心,病毒防护,defender,security"},
        {"firewall", "防火墙设置", "Windows Defender 防火墙", "C:\\Windows\\System32\\firewall.cpl", false,
            "防火墙,firewall"},
        {"privacy", "隐私设置", "权限、诊断数据", "ms-settings:privacy", true,
            "隐私,隐私设置,隐私和安全性,privacy"},
        {"camera", "摄像头权限", "哪些应用可以用摄像头", "ms-settings:privacy-webcam", true,
            "摄像头,摄像头被占用,相机,摄像头权限,相机权限,打不开,无法使用,camera"},
        {"microphone", "麦克风权限", "哪些应用可以用麦克风", "ms-settings:privacy-microphone", true,
            "麦克风,话筒,麦克风权限,话筒权限,没声音,microphone"},
        {"notifications", "通知设置", "通知、专注助手", "ms-settings:notifications", true,
            "通知设置,通知,notifications"},
        {"focus", "专注助手", "免打扰、专注时间", "ms-settings:quiethours", true,
            "专注助手,专注模式,免打扰,quiet hours,focus assist"},
        {"datetime", "日期和时间", "时区、自动同步", "ms-settings:dateandtime", true,
            "日期和时间,时间设置,时间准,时间不对,时间同步,时区,日期格式,日期,date,time,timezone"},
        {"language", "语言和区域", "显示语言、输入法", "ms-settings:regionlanguage", true,
            "语言设置,语言和区域,语言包,时间和语言,输入法,输入法切换,区域,language,region,ime"},
        {"fonts", "字体设置", "安装、预览字体", "ms-settings:fonts", true,
            "字体设置,字体,fonts"},
        {"mouse", "鼠标和触摸板", "指针速度、触摸板手势", "ms-settings:mousetouchpad", true,
            "鼠标设置,鼠标速度,指针速度,鼠标指针,触摸板,mouse,touchpad"},
        {"keyboard", "键盘设置", "键盘布局、快捷键", "ms-settings:keyboard", true,
            "键盘设置,键盘,keyboard"},
        {"about", "关于本机", "设备规格、Windows 版本", "ms-settings:about", true,
            "关于本机,系统信息,电脑配置,设备规格,关于,about,system info"},
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
            "电源选项,电源计划,高性能,性能模式,快速启动,usb选择性暂停,休眠文件,power options,powercfg"},
        {"netconnections", "网络连接", "适配器、网络连接", "C:\\Windows\\System32\\ncpa.cpl", false,
            "网络连接,网络适配器,网卡设置,DNS,IP地址,dns设置,network connections,ncpa"},
        {"msinfo", "系统信息", "硬件、组件、环境", "C:\\Windows\\System32\\msinfo32.exe", false,
            "系统信息,硬件信息,msinfo32,system information"},
        {"taskmgr", "任务管理器", "进程、启动、性能", "C:\\Windows\\System32\\taskmgr.exe", false,
            "任务管理器,进程管理,task manager,taskmgr"},
        {"cmd", "命令提示符", "打开 cmd", "C:\\Windows\\System32\\cmd.exe", false,
            "命令提示符,命令行,cmd,command prompt"},
        {"taskbar", "任务栏设置", "任务栏位置、图标、隐藏", "ms-settings:taskbar", true,
            "任务栏,任务栏设置,任务栏位置,任务栏隐藏,taskbar"},
        {"background", "桌面背景", "壁纸、背景图片", "ms-settings:personalization-background", true,
            "壁纸,桌面背景,背景图片,换壁纸,background,wallpaper"},
        {"colors", "颜色和主题", "深色模式、主题色", "ms-settings:personalization-colors", true,
            "颜色,主题色,深色模式,浅色模式,夜间模式颜色,dark mode,color"},
        {"lockscreen", "锁屏界面", "锁屏图片、锁屏超时", "ms-settings:lockscreen", true,
            "锁屏,锁屏界面,锁屏图片,lock screen"},
        {"themes", "主题", "桌面主题、声音、鼠标指针", "ms-settings:themes", true,
            "主题,桌面主题,个性化,theme"},
        {"startmenu", "开始菜单设置", "开始菜单布局、显示项", "ms-settings:personalization-start", true,
            "开始菜单,开始屏幕,start menu"},
        {"signin", "登录选项", "密码、PIN、Windows Hello", "ms-settings:signinoptions", true,
            "登录选项,登录密码,登录,密码,pin,pin重置,windows hello,sign-in"},
        {"users", "用户账户", "添加用户、其他账户", "ms-settings:otherusers", true,
            "用户账户,用户,添加用户,其他用户,账户头像,管理员账户,accounts,users"},
        {"uac", "用户账户控制", "UAC 滑块", "C:\\Windows\\System32\\UserAccountControlSettings.exe", false,
            "uac,用户账户控制,账户控制,权限弹窗"},
        {"restore", "系统还原", "还原点、系统保护", "C:\\Windows\\System32\\SystemPropertiesProtection.exe", false,
            "系统还原,还原点,系统保护,restore"},
        {"bitlocker", "BitLocker", "设备加密、驱动器加密", "ms-settings:bitlocker", true,
            "bitlocker,设备加密,磁盘加密,驱动器加密,加密u盘,u盘加密"},
        {"printers", "打印机和扫描仪", "添加打印机、扫描仪", "ms-settings:printers", true,
            "打印机,打印机脱机,打印,扫描仪,添加打印机,printer,scanner"},
        {"project", "投影和投屏", "第二屏幕、无线显示", "ms-settings:project", true,
            "投影,投屏,第二屏幕,无线显示,project,cast"},
        {"displayadv", "高级显示设置", "刷新率、HDR、分辨率", "ms-settings:display-advanced", true,
            "刷新率,hdr,高级显示,显示适配器,refresh rate"},
        {"optionalfeatures", "启用或关闭 Windows 功能", "Windows 可选组件", "C:\\Windows\\System32\\OptionalFeatures.exe", false,
            "windows功能,启用或关闭windows功能,可选功能,optional features"},
        {"taskschd", "任务计划程序", "计划任务、定时任务", "C:\\Windows\\System32\\taskschd.msc", false,
            "计划任务,任务计划,定时任务,task scheduler,taskschd"},
        {"gpedit", "组策略", "本地组策略编辑器", "C:\\Windows\\System32\\gpedit.msc", false,
            "组策略,本地组策略,gpedit,group policy"},
        {"msconfig", "系统配置", "启动配置、服务、引导", "C:\\Windows\\System32\\msconfig.exe", false,
            "系统配置,启动配置,干净启动,引导,msconfig"},
        {"resmon", "资源监视器", "CPU、内存、磁盘、网络", "C:\\Windows\\System32\\resmon.exe", false,
            "资源监视器,端口,端口占用,端口转发,网络端口,性能,resource monitor,resmon"},
        {"certmgr", "证书管理器", "用户证书、受信任的根证书", "C:\\Windows\\System32\\certmgr.msc", false,
            "证书,证书管理器,certificate,certmgr"},
        {"colorcpl", "颜色管理", "显示器颜色配置文件", "C:\\Windows\\System32\\colorcpl.exe", false,
            "颜色管理,色彩管理,颜色偏黄,偏黄,icc,color management"},
        {"sysdm", "系统属性（高级）", "计算机名、性能、环境变量", "C:\\Windows\\System32\\SystemPropertiesAdvanced.exe", false,
            "系统属性,高级系统设置,计算机名,性能设置,性能选项,虚拟内存,system properties"},
        {"wfs", "传真和扫描", "Windows 传真和扫描", "C:\\Windows\\System32\\wfs.exe", false,
            "传真,扫描,传真和扫描,fax,scan"},
        {"eventvwr", "事件查看器", "系统日志、蓝屏日志", "C:\\Windows\\System32\\eventvwr.exe", false,
            "事件查看器,系统日志,蓝屏日志,事件日志,event viewer,eventvwr"},
        {"clipboard", "剪贴板设置", "剪贴板历史、跨设备同步", "ms-settings:clipboard", true,
            "剪贴板,剪贴板历史,复制历史,clipboard"},
        {"multitasking", "多任务", "分屏、虚拟桌面、任务视图", "ms-settings:multitasking", true,
            "多任务,多任务分屏,分屏,虚拟桌面,任务视图,multitasking,virtual desktop"},
        {"savelocations", "保存位置", "下载文件夹、默认保存位置", "ms-settings:savelocations", true,
            "保存位置,下载文件夹,下载文件夹位置,默认保存位置,下载位置,save locations"},
        {"backup", "备份", "文件历史、OneDrive 同步", "ms-settings:backup", true,
            "备份,文件历史,onedrive同步,onedrive,file history,backup"},
        {"recovery", "恢复", "恢复出厂、启动修复、安全模式", "ms-settings:recovery", true,
            "恢复,恢复出厂,重置电脑,启动修复,安全模式,高级启动,recovery,reset"},
        {"exploreropts", "文件资源管理器选项", "显示扩展名、隐藏文件", "C:\\Windows\\System32\\control.exe", false,
            "文件资源管理器选项,文件夹选项,显示文件扩展名,显示扩展名,隐藏文件,explorer options"},
        {"hosts", "Hosts 文件", "域名解析、hosts 文件位置", "C:\\Windows\\System32\\drivers\\etc\\hosts", false,
            "hosts,hosts文件,host文件,域名解析,hosts路径"},
        {"advertising", "广告和隐私", "广告 ID、个性化广告", "ms-settings:privacy-general", true,
            "广告,关闭广告,广告id,个性化广告,advertising,ad id"},
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
