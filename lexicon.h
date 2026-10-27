// The words this plugin knows. A description is matched against concepts,
// not against one exact keyword: "去年报销的规定" also looks for 差旅/费用
// and 管理办法/制度. This is a small, editable knowledge base, not a model.
#pragma once

#include <string>
#include <vector>

namespace finder {

struct Concept {
    const char* id;
    const char* zh; // the label shown to the user
    const char* en;
    std::vector<const char*> terms; // Chinese and English, all lower-case
};

// Order matters: specific concepts come before general ones. A description
// containing 体检 will not be scored as a plain 报告.
inline const std::vector<Concept>& concepts()
{
    static const std::vector<Concept> all = {
        {"reimburse", "报销", "reimbursement",
            {"报销", "报销单", "费用报销", "差旅", "出差", "差旅费", "发票", "票据", "支出", "申请单", "reimbursement", "expense", "invoice", "receipt"}},
        {"policy", "规定", "rules",
            {"规定", "制度", "管理办法", "办法", "规范", "条例", "政策", "流程", "守则", "policy", "rule", "regulation", "procedure", "guideline"}},
        {"contract", "合同", "contract",
            {"合同", "协议", "契约", "甲方", "乙方", "签署", "盖章", "续签", "contract", "agreement"}},
        {"resume", "简历", "resume",
            {"简历", "履历", "求职", "应聘", "招聘", "resume", "cv"}},
        {"meeting", "会议", "meeting",
            {"会议", "纪要", "议题", "参会", "例会", "开会", "会议记录", "meeting", "minutes"}},
        {"weekly", "汇报", "report",
            {"周报", "日报", "月报", "周总结", "汇报", "weekly", "report"}},
        {"budget", "预算", "budget",
            {"预算", "概算", "费用预算", "成本", "budget", "cost"}},
        {"quote", "报价", "quote",
            {"报价", "询价", "报价单", "价格", "单价", "quote", "quotation", "price"}},
        {"tax", "税务", "tax",
            {"个税", "所得税", "纳税", "税务", "增值税", "开票", "税", "tax", "vat"}},
        {"checkup", "体检", "checkup",
            {"体检", "健康检查", "化验", "化验单", "体检报告", "医院", "checkup", "medical"}},
        {"medical", "病历", "medical record",
            {"病历", "就诊", "诊断", "处方", "挂号", "住院", "门诊", "药", "medical record", "diagnosis"}},
        {"vaccine", "疫苗", "vaccine",
            {"疫苗", "打针", "接种", "防疫", "针剂", "vaccine", "vaccination"}},
        {"pet", "宠物", "pet",
            {"宠物", "猫咪", "猫粮", "狗狗", "狗粮", "养猫", "养狗", "猫", "狗", "pet", "cat", "dog"}},
        {"photo", "照片", "photo",
            {"照片", "相片", "图片", "图像", "摄影", "相册", "photo", "picture", "image"}},
        {"screenshot", "截图", "screenshot",
            {"截图", "截屏", "屏幕截图", "屏幕快照", "screenshot", "snip"}},
        {"video", "视频", "video",
            {"视频", "影片", "电影", "录像", "短片", "video", "movie", "film"}},
        {"music", "音乐", "music",
            {"音乐", "歌曲", "音频", "录音", "歌", "music", "song", "audio"}},
        {"tutorial", "教程", "tutorial",
            {"教程", "指南", "入门", "学习资料", "tutorial", "guide", "manual"}},
        {"paper", "论文", "paper",
            {"论文", "文献", "期刊", "学术", "研究", "paper", "thesis", "journal"}},
        {"novel", "小说", "novel",
            {"小说", "电子书", "阅读", "文学", "novel", "ebook"}},
        {"code", "代码", "code",
            {"代码", "源码", "源代码", "脚本", "程序", "code", "source", "script"}},
        {"config", "配置", "config",
            {"配置", "设置", "参数", "选项", "config", "configuration", "settings"}},
        {"backup", "备份", "backup",
            {"备份", "存档", "副本", "归档", "backup", "archive"}},
        {"driver", "驱动", "driver",
            {"驱动", "驱动程序", "固件", "设备", "driver", "firmware"}},
        {"certificate", "证书", "certificate",
            {"证书", "证件", "执照", "资质", "证明", "certificate", "license"}},
        {"passport", "护照", "passport",
            {"护照", "签证", "出入境", "passport", "visa"}},
        {"idcard", "身份证", "ID card",
            {"身份证", "身份证明", "id card"}},
        {"score", "成绩", "grades",
            {"成绩", "成绩单", "分数", "考试", "测验", "score", "grade", "exam"}},
        {"menu", "菜单", "menu",
            {"菜单", "菜谱", "食谱", "餐饮", "menu", "recipe"}},
        {"proposal", "方案", "proposal",
            {"方案", "提案", "策划", "计划书", "proposal"}},
        {"plan", "计划", "plan",
            {"计划", "规划", "安排", "日程", "排期", "plan", "schedule"}},
        {"summary", "总结", "summary",
            {"总结", "复盘", "小结", "summary", "review"}},
        {"notice", "通知", "notice",
            {"通知", "公告", "告示", "通告", "notice", "announcement"}},
        {"template", "模板", "template",
            {"模板", "范例", "样板", "template"}},
        {"finance", "财务", "finance",
            {"财务", "会计", "账本", "账单", "报表", "对账", "finance", "accounting"}},
        {"hr", "人事", "HR",
            {"人事", "招聘", "员工", "考勤", "薪酬", "工资", "payroll"}},
        {"project", "项目", "project",
            {"项目", "工程", "立项", "里程碑", "project"}},
        {"design", "设计", "design",
            {"设计", "图稿", "原型", "视觉", "design", "prototype"}},
        {"drawing", "图纸", "drawing",
            {"图纸", "施工图", "平面图", "cad", "drawing", "blueprint"}},
        {"manual", "说明书", "manual",
            {"说明书", "使用说明", "manual", "documentation"}},
        {"installer", "安装包", "installer",
            {"安装包", "安装程序", "安装文件", "客户端", "setup", "installer"}},
        {"data", "数据", "data",
            {"数据", "统计", "数据集", "data", "statistics", "dataset"}},
        {"sheet", "表格", "spreadsheet",
            {"表格", "工作表", "台账", "excel", "spreadsheet", "sheet"}},
        {"slides", "演示", "slides",
            {"演示", "幻灯片", "演示文稿", "汇报材料", "presentation", "slide"}},
        {"archive", "压缩包", "archive",
            {"压缩包", "压缩文件", "解压", "archive", "zip", "rar", "7z"}},
        {"download", "下载", "download",
            {"下载", "下载文件", "download"}},
        {"insurance", "保险", "insurance",
            {"保险", "保单", "投保", "理赔", "insurance"}},
        {"bank", "银行", "bank",
            {"银行", "流水", "转账", "对账单", "bank", "statement"}},
        {"loan", "贷款", "loan",
            {"房贷", "按揭", "贷款", "还款", "mortgage", "loan"}},
        {"rent", "租房", "rent",
            {"租房", "租赁", "房租", "房东", "lease", "rent"}},
        {"travel", "旅行", "travel",
            {"旅行", "旅游", "行程", "攻略", "travel", "trip", "itinerary"}},
        {"flight", "机票", "flight",
            {"机票", "航班", "登机牌", "flight", "ticket"}},
        {"hotel", "酒店", "hotel",
            {"酒店", "住宿", "预订", "hotel", "booking"}},
        {"study", "学习", "study",
            {"学习", "课程", "课件", "笔记", "作业", "study", "course", "notes"}},
        {"english", "英语", "English",
            {"英语", "英文", "单词", "雅思", "托福", "english", "ielts", "toefl"}},
        {"database", "数据库", "database",
            {"数据库", "表结构", "database"}},
        {"sql", "SQL 脚本", "SQL script",
            {"sql", "sql脚本", "数据库脚本", "sql文件", "建表语句", "查询语句"}},
        {"log", "日志", "log",
            {"日志", "log", "记录文件"}},
        {"receipt", "收据", "receipt",
            {"收据", "小票", "回执", "receipt"}},
        {"shipping", "快递", "shipping",
            {"快递", "物流", "运单", "单号", "shipping", "tracking"}},
        {"contact", "联系人", "contact",
            {"名片", "联系人", "通讯录", "contact", "business card"}},
        {"password", "密码", "password",
            {"密码", "账号", "口令", "password", "credential"}},
        {"key", "密钥", "key",
            {"密钥", "私钥", "公钥", "key"}},
        {"checklist", "清单", "checklist",
            {"清单", "列表", "checklist"}},
        {"report", "报告", "report",
            {"报告", "汇报材料", "report"}},
    };
    return all;
}

// Words that say nothing about which file: they are dropped before the
// remaining words are searched. Longer phrases first.
inline const std::vector<const char*>& stopwords()
{
    static const std::vector<const char*> words = {
        "帮我找一下", "帮我找", "帮忙找", "找一下", "找找", "搜索一下", "查一下", "看一下", "看看",
        "我想要", "我想找", "需要找", "有没有", "有没有关于", "关于", "有关", "相关",
        "那个", "这个", "那份", "这份", "那封", "这封", "那次", "这次",
        "一个", "一份", "一些", "所有", "全部", "本机", "电脑上", "电脑里", "里面", "内容",
        "文件名", "文件", "文件夹", "东西", "资料", "文档", "材料", "记录",
        "提到", "包含", "说的", "讲的", "写的是", "写", "叫", "名为",
        "请", "帮我", "给我", "一下", "之前", "以前", "时候", "时间",
        "的", "了", "吗", "呢", "啊", "吧", "和", "与", "或", "在", "上", "中", "里",
        "find", "search", "file", "files", "document", "documents", "please", "the", "a", "an", "of", "for", "about", "my", "me",
    };
    return words;
}

} // namespace finder
