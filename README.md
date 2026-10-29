# 随心搜 · suixin

用一句自然语言找本机文件、文件夹、文件内容、聊天软件收到的文件，或者打开 Windows 设置页。

完全离线：本地索引 + 本地 ONNX 模型，不联网、不上传、不需要 API Key。

[![Release](https://img.shields.io/github/v/release/MakotoArai-CN/suixin)](https://github.com/MakotoArai-CN/suixin/releases)
[![License](https://img.shields.io/github/license/MakotoArai-CN/suixin)](LICENSE)
![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20x64-0078D4)

## 功能

- 按描述找文件：`找 去年报销的规定`、`找 上周的会议记录 PDF`
- 按文件夹找：`找 文档里面的安装包`、`f 下载的大于5M的文件`
- 按类型/时间/大小：`找 大于1M小于50M的数据库备份文件`
- 找聊天软件收到的文件：微信、QQ、企业微信、钉钉、飞书、WPS
- 找 Windows 设置：`找 环境变量`、`找 任务栏设置`
- 本地向量重排，支持 `auto / light / balanced / large / custom` 模型档位
- 结果带真实路径、修改时间、大小；支持打开、复制路径、在文件夹中显示

## 安装

1. 从 [Releases](https://github.com/MakotoArai-CN/suixin/releases) 下载 `suixin-*.zip`。
2. 解压到：

   ```text
   %LOCALAPPDATA%\WinShun\plugins\suixin
   ```

3. 重启 Win顺，在「设置 → 插件」启用 **随心搜**。
4. 首次查询允许 `winshun:search`、`winshun:open`、`files:read`。

需要 Windows 10/11 x64、Win顺 0.14 及以上。包内已带 ONNX Runtime 和中文模型，不需要 Python/CUDA。

> 从旧 `ainwin` 升级：插件 id 已改为 `suixin`，Win顺 按 id 保存启用状态，替换目录后要重新启用一次；旧 `plugins\ainwin` 可删除。

## 使用

默认关键字：`找`、`find`、`search`、`f`、`zf`。可在 **Win顺 设置 → 插件 → 随心搜 → 关键字** 里改成自己的。

```text
找 上周的会议记录 PDF
找 桌面上的合同
找 昨天微信收到的word和excel
找 微信和QQ的图片
找 大于1M小于50M的数据库备份文件
找 环境变量
```

| 操作 | 按键 |
|---|---|
| 打开 | Enter |
| 复制路径 | Ctrl+Enter |
| 在文件夹中显示 | Shift+Enter |
| 详细信息 | 右键菜单 |

结果第二行显示所在文件夹，行内带 `modified`、`size`，Win顺 面板可排序/筛选。右键「详细信息」可看完整路径、时间、大小和命中原因。

Win顺 0.15 的原生悬停是 NameTip，只在名称/文件夹被截断时出现；插件行是宿主里的应用行，不能复用原生文件悬停。详细说明见 [docs/limitations.md](docs/limitations.md)。

## 权限

| 权限 | 用途 |
|---|---|
| `winshun:search` | 调用 Win顺文件名/内容索引 |
| `winshun:open` | 打开文件、设置页，或定位结果 |
| `files:read` | 只读微信、QQ、企业微信、钉钉、飞书、WPS 的传输/接收目录，找聊天收到的文件 |

不申请 `network`、剪贴板、进程、`files:write`、`winshun:content`。插件不写用户目录；模型和 DLL 从插件目录只读加载。

## 构建

需要 Zig 0.17+：

```powershell
zig build
```

或用 CMake/MSVC：

```powershell
cmake -B build
cmake --build build --config Release
```

产物是 `suixin.exe`。模型和 `onnxruntime.dll` 不从源码构建。

## 文档

- [架构与目录](docs/architecture.md)
- [设置、文件夹与聊天文件](docs/settings.md)
- [模型、上下文与多语言](docs/models.md)
- [基准测试](docs/bench/README.md)
- [已知限制](docs/limitations.md)
- [宣传与社区](docs/promotion.md)

## 友情链接

- [linux.do](https://linux.do)
- [NodeSeek](https://www.nodeseek.com)
- [NodeLoc](https://www.nodeloc.com)

## 许可证

MIT，见 [LICENSE](LICENSE)。第三方组件见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) 和 `models/*/README.md`。
