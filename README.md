# 意图找文件 · ainwin

开发者：**MakotoArai-CN** · MIT

**下载（含模型和 DLL）**：https://github.com/MakotoArai-CN/ainwin/releases/latest

源码仓库不提交模型和 `onnxruntime.dll`（大文件），克隆后请从 Releases 取完整包，或把 `models/`、`onnxruntime.dll` 放到本目录再构建。

> **从 0.3.1 升级**：插件 id 从 `panasonic.ainwin` 改成了 `ainwin`。Win顺 的启用状态按 id 保存，升级/替换目录后旧状态不会自动迁移，必须在「设置 → 插件」里重新启用 **意图找文件**；否则输入 `找 …` 不会调用插件，看起来就是“搜不到”。旧的 `plugins\panasonic.ainwin` 目录可以删除。

用一句中文（或英文）描述要找的文件，插件先拆出**主题概念、时间、文件类型、文件夹、大小和排除项**，再用 Win顺 自己的文件名/内容索引检索，最后用**设备本地 INT8 向量模型**把候选按意思重排。不联网、不调用云端 API、不上传任何文字。

```
找 去年报销的规定
找 上周的会议记录 PDF
找 桌面上的合同
找 大于 10M 的数据库文件
找 大于 1kb 的 JavaScript 文件
找 大于 1kb 的可执行文件
找 论文附录 *.tex
找 点云文件
```

- 打开结果：**Enter**（由 Win顺 打开，插件不碰文件）
- 复制路径：**Ctrl+Enter**
- 在文件夹中显示：**Shift+Enter**
- 全程键盘可完成；空输入时给出的示例行可回车填入。

## 关键字

`找`（备用 `zf`）。单独输入关键字会显示示例和键盘说明；输入 `找 ` 加空格后描述。

复杂句会拆成可核对的条件：

| 说法 | 拆出来的东西 |
|---|---|
| `上个月在D盘关于云南旅游的预算表，不要草稿` | 时间=上个月；位置=D:\；概念=旅行、预算；排除=草稿；残余词=云南 |
| `最近半年客户签字的租赁合同扫描件` | 时间=最近半年；概念=合同、租房；残余=客户签字、扫描件 |
| `包含差旅费的Excel表格` | 类型=Excel；概念=报销（差旅费）、表格 |
| `上周开会拍的现场照片` | 时间=上周；类型=图片；概念=会议；现场作为排序词 |
| `项目周报里提到服务器迁移的那份PPT` | 类型=PPT；概念=项目、汇报；内容词=服务器迁移 |

照片/视频没有可检索的文字时，不会硬凑“内容匹配”：会按**类型 + 时间/文件夹**列出，并在底部说明“按类型和时间列出”。

## 文件类型：内置目录 + 任意扩展名

内置类型目录覆盖常见的和各行业的格式，并持续用**扩展名**而不是“文件名里有没有这个词”来判断：

| 类别 | 例子（完整列表见 `planner.h` 的 `typeWords`） |
|---|---|
| 办公文档 | doc/docx/docm/dotx、xls/xlsx/xlsm/xlsb、ppt/pptx/ppsx、pdf、odt/ods/odp、rtf、wps/et/dps、pages/numbers/key、ofd/xps/oxps、hwp |
| 图片/摄影 | jpg/jpeg/png/gif/bmp/webp/heic/heif/avif/jxl/tif/tiff/ico/svg、psd/ai/eps、cr2/cr3/nef/arw/dng/orf/rw2/raf、exr/hdr/dds/ktx |
| 视频/音频 | mp4/mkv/avi/mov/wmv/flv/f4v/webm/m4v/mpg/mpeg/mxf/mts/m2ts/3gp/ogv/vob；mp3/wav/flac/aac/m4a/ogg/opus/wma/ape/aiff/alac/mid/caf/dsf/dff |
| 压缩/镜像 | zip/zipx/rar/7z/tar/gz/bz2/xz/zst/lz4/lzma/cab/arj；iso/img/vhd/vhdx/vmdk/qcow2/wim/dmg/ova/ovf |
| 可执行/安装 | exe/dll/sys/scr/cpl/ocx/com/msi/msix/appx/app、apk/ipa/jar/deb/rpm/pkg、bat/cmd/ps1/vbs |
| 数据库 | db/db3/sqlite/sqlite3/mdb/accdb/dbf/frm/ibd/myd/myi/mdf/ndf/ldf/duckdb/realm/parquet/avro/orc |
| 数据/统计 | csv/tsv/json/jsonl/ndjson/xml/yaml/parquet/feather/arrow/hdf5/h5/npy/npz/mat/sav/dta/sas7bdat/rds/rdata |
| 代码/语言 | c/cpp/h/hpp、cs、java/class、py/pyw/ipynb、js/mjs/cjs/jsx、ts/tsx、go、rs、swift、rb、php、lua、kt/kts、dart、scala、r、m、jl、tex、sh/bash、ps1、html/css/scss/less/vue、json/yaml/xml、sql/psql |
| 工程/CAD/BIM | dwg/dxf/dgn、stp/step/igs/iges、sldprt/sldasm/slddrw、ipt/iam/idw、catpart/catproduct、prt/asm、rvt/rfa/ifc、skp、3ds/fbx/blend/max/c4d、stl/obj/3mf/amf/gcode、pcb/sch/brd/gerber |
| 科研/医疗/生物 | dcm/dicom/nii、fasta/fa/fastq/fq/sam/bam/vcf/bed/gtf/gff、mol/mol2/sdf/smi/cdx/cif/pdb、fits、las/laz |
| 邮件/证书/抓包 | eml/msg/mbox/pst/ost、cer/crt/pem/der/pfx/p12/key/gpg/csr、pcap/pcapng/har |
| 字体/字幕/电子书/种子 | ttf/otf/woff/woff2/ttc、srt/ass/ssa/vtt/sub/idx/sup、epub/mobi/azw3/kfx/fb2/djvu/chm/cbz/cbr、torrent |
| 游戏/虚拟化/备份 | pak/uasset/umap/unitypackage/assetbundle/wad/vpk/bsa、vhd/vhdx/vmdk/vdi/qcow2、bak/backup/bkp |

另外三种兜底，保证“冷门类型”也能查：

- `找 *.xyz`、`找 *.tar.gz`：直接按扩展名；
- `找 ext:abc,def`：一次指定多个扩展名；
- 设置里的 **自定义类型**：`点云=las,laz;标本=svs,ndpi;注塑=mold,stp`，写多少行业都行，无需改代码。

类型是**硬过滤**，但仍会和“概念/文件名/内容”一起排序；`找 数据库脚本` 会收窄到 `.sql`，`找 数据库文件` 是 `.db/.sqlite/...`，两者不会混。格式不在时，文档类（PDF/Word/PPT/表格/图片等）会自动放宽类型并提示；代码/数据库这类格式（`.cpp`、`.sql`、`.db`）不会拿别的格式冒充。

## 系统设置也能找

“环境变量”不是文件，之前只搜文件索引，所以找不到；这不是权限问题。`winshun:search` 只读 Win顺 的文件/内容索引，插件没有枚举注册表或 Windows 设置页的权限。

0.5.1 补全了 **Windows 设置目录**，按中文/英文别名匹配；设置类描述已覆盖常见说法：

```
找 环境变量
找 设置里面的环境变量
找 屏幕亮度
找 关闭自动更新
找 任务栏放下面
找 登录选项
找 系统还原
找 打印机
找 代理设置
找 开机启动项在哪
找 蓝牙开关
找 默认应用
找 服务
找 注册表
```

命中后会在文件结果上方给出“编辑环境变量 / 代理设置 / 启动应用……”一行：

- **Enter** 打开对应的 Windows 设置页、`.cpl`/`.msc` 或系统属性；
- **Ctrl+Enter** 复制目标 URI/命令；
- 不申请新权限，仍然只用 `winshun:open` 打开结果。

“环境变量”打开的是 **系统属性 → 高级**，再点“环境变量”；Windows 没有稳定的 `ms-settings:` 直达页，所以不硬编一个可能失效的 URI。

目录覆盖的是常见设置，不是全部注册表开关；不在目录里的功能仍按文件名/内容搜索，不会假装能打开。

## 它怎么理解“意思”

1. **概念词库**：报销↔费用/差旅/发票，规定↔制度/管理办法，租房↔租赁，报账↔报销……约 60 组主题，中英双语。
2. **条件解析**：去年/上周/最近三天、2025 年 5 月、桌面/下载/D 盘、大于 10MB、不要发票、`*.dwg` 等。
3. **多路召回**：整句精确 → 逐词放宽 → 概念短语（如“报销规定”“费用管理办法”“租房合同”）→ 文件类型扩展名（`.db`、`.sql`、`.cpp`）→ 同义词（“租房”→“租赁”）。
4. **本地向量重排**：候选文件名+内容片段交给本地 bge-small-zh INT8，按余弦相似度重排；最多 18 个候选、每条 128 词元，默认 4 线程。
5. **证据与降噪**：同段命中、文件名命中、扩展名语义、主机相关度、修改时间综合；跳过系统目录、开发缓存和纯数字文件名；大小/时间查询只走带大小/时间的索引。

仍然不是“全库向量索引”：它重排的是检索到的候选，召回仍依赖词库、类型和同义词。需要任意改写也能召回时，可同时启用随 Win顺 的「按意思找」（`winshun.semantic`，需要 Ollama 或云密钥），两者不冲突。

## 本地模型与预置档位

设置里的 **模型档位**，换档后下一条查询生效：

| 档位 | 模型 | 体积/维度 | 适合 |
|---|---|---|---|
| `light` 轻量 | bge-small-zh-v1.5 INT8 | 24 MB / 512 | 默认、8–16 GB 内存、追求快 |
| `balanced` 均衡 | bge-base-zh-v1.5 INT8 | 103 MB / 768 | 16 GB 以上、能接受数秒等待 |
| `large` 高质量 | bge-large-zh-v1.5 INT8 | 327 MB / 1024 | 32 GB / 更多核心；未随包，需自备 |
| `custom` 自定义 | 自己的 BERT WordPiece ONNX | 任意 | 已有导出模型 |

`auto`（默认）：**内存 ≥ 16 GB 且 `bge-base` 文件夹存在**时用 `balanced`，否则用 `light`。测试机 8 GB，自动落在 `light`。

### 基准测试（不是主观感觉）

完整的测试机配置、数据集、指标定义、失败样例和复现命令在 **[bench/README.md](bench/README.md)**。摘要：

- 测试机：Panasonic CFXZ6-1，Intel i5-7300U（2 核 4 线程，2.6 GHz），8 GB DDR4，Intel HD 620 核显（未使用 GPU），Samsung 256 GB SATA SSD，Windows 10 22H2 19045，WinShun 0.14.0，ONNX Runtime 1.31.0 CPU，4 线程，每条 128 词元。
- 数据集：24 条中文段落、30 条查询，含同义改写和难负例；模型级检索，不含 Win顺 文件索引。
- 速度：每组 3 次取中位数。

| 模型 | Accuracy@1 | Recall@1 | MRR@10 | nDCG@10 | 加载 | 24 段编码 | 30 查询编码 | 总计（中位数） |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| small 512 | 0.933 | 0.900 | 0.967 | 0.973 | 314 ms | 426 ms | 313 ms | **1030 ms** |
| base 768 | 0.967 | 0.933 | 0.983 | 0.988 | 928 ms | 3096 ms | 1568 ms | **6258 ms** |
| large 1024 | 0.967 | 0.933 | 0.983 | 0.985 | 2123 ms | 8982 ms | 4145 ms | **15782 ms** |

端到端（Win顺 真实查询 `找 预算`，18 个候选重排，冷启动）：

| 档位 | 加载 | 18 条重排 | 总耗时 | 结果 |
|---|---:|---:|---:|---|
| light | 461 ms | 558 ms | **1153 ms** | 正常 |
| balanced | 1460 ms | 5364 ms | **7463 ms** | 正常，接近 10 s 超时 |
| large | 2567 ms | 11469 ms | **14283 ms** | 超过 10 s，结果被丢弃 |

数字说明：在这 30 条查询上，base 比 small 多对 1 条，nDCG 略高；large 与 base 的 Accuracy@1/Recall 相同，nDCG 还略低。**更大不自动更准，而且明确更慢。** 测试机是低功耗双核，换硬件结果会变；large 在这台机器上不适合端到端使用。

**关于独立显卡**：当前内核是 ONNX Runtime CPU 推理，不会自动用独显加速；换更大模型是“质量换时间”，不是“显卡加速”。没有硬塞 DirectML，是因为官方 DirectML 包最新只到 ONNX Runtime 1.24，而本插件用 1.31 C API，强行混用有崩溃风险；CUDA 版又要求特定 CUDA/cuDNN。需要 GPU 时可保留 `custom` 档，换成为你的显卡构建的 ORT + 模型。

**模型选择（评估过）**：

- `bge-base`：比 small 深 3 倍、768 维；测试集 Accuracy@1 0.967 vs 0.933，但端到端约 7.5 s，适合 16 GB 以上、能等数秒的机器。
- `bge-large`：327 MB、1024 维；测试集没有超过 base，测试机端到端超时；不随包，避免把插件做成 300 MB+。
- `Zhinao-ChineseModernBert-Embedding`：227M 参数，但 INT8 ONNX 约 230 MB，且用 BPE 词表；当前 tokenizer 读 `vocab.txt`（BERT WordPiece），需要先导出 ONNX 并换词表实现，未随包。
- `EmbeddingGemma-300M (INT8)`：约 300 MB，多语言，使用 SentencePiece/Unigram；同样需要额外 tokenizer，且 CPU 上更慢，未随包。
- `PicoQMD`：在 Hugging Face 上没有找到公开的权重/模型卡（搜索为空），无法作为可下载依赖，故未采用。

“放开限制”的部分：`sandbox: normal`（为了加载 DLL/模型）、`memoryMB: 1024`、`timeoutMs: 10000`、`idleExitSec: 0`（常驻不退出）、线程最多 8。没有放开网络、剪贴板、子进程或管理员权限。

模型资产合计约 **127 MB**（small 24 MB + base 103 MB），加上 ORT DLL 19 MB、exe 1 MB；large 的 327 MB 不随包。

## 权限

只申请两项：

| 权限 | 原因 |
|---|---|
| `winshun:search` | 调用 Win顺 的 `search_files` / `search_content`，读索引里的路径、时间、大小和文字片段；插件不遍历磁盘。 |
| `winshun:open` | 只在 **Shift+Enter** 时调用 `show_in_folder` 定位结果。 |

没有申请：`network`、`clipboard:read`、`clipboard:write`、`processes`、`files:write`、`files:read`、`winshun:content`、`ui`。模型和 DLL 是插件自己的资源，从插件目录读取；不读用户文件。

`normal` 沙箱不是强制隔离（Win顺 只能对 `winshun:*`、进程、剪贴板做作业限制）；插件代码本身不联网、不写用户目录、不启动进程。

## 数据位置

- **不写数据**：没有自建索引、缓存文件、日志或配置写入 `WS_DATA_DIR`；查询结果只在内存缓存 60 秒。
- 模型/词表在 `models/bge-small-zh-v1.5-int8/`，DLL 在插件根目录；它们只读。
- stderr 只写一行统计（字符数、概念数、调用数、命中数、耗时）和工具失败原因，不写正文/完整路径。

## 失败时

| 情况 | 表现 |
|---|---|
| 模型缺失/加载失败 | 自动退回“概念+词面”检索，仍可用；stderr 记一行原因，不弹假结果 |
| 内容索引没开 | 仍按文件名找；结果末尾提示；一个都没有时直接指路“设置 → 文件内容搜索” |
| 没有 `winshun:search` | 明确“没有搜索权限”，不返回伪造结果 |
| 工具报错/超时 | 保留已拿到的结果并说明“部分内容没有查完”；全失败显示错误原文 |
| 类型没命中 | 文档类自动放宽类型并提示；代码/数据库类不放宽，避免 `.db` 冒充 `.sql` |
| 没有匹配 | 空状态 + “用 Win顺 搜索这些词：…”（回车填入）；不编造 |
| 输入太短/只有时间 | 不发起搜索，提示补一句描述 |

查询阶段只做只读的 `search_files`/`search_content`：不打开文件、不启动程序、不改剪贴板、不删除/移动文件、不写盘。

## 编译（Zig 优先）

需要 **Zig 0.17+**（已用 0.17.0 验证）：

```powershell
zig build
```

`build.zig` 默认 `ReleaseFast`，把 `ainwin.exe` 直接写到插件目录；也可以：

```powershell
.\build.ps1
# 或
zig build -Doptimize=fast
```

有 MSVC/CMake 时：

```powershell
cmake -B build
cmake --build build --config Release
```

两种方式都需要把 `onnxruntime.dll` 和 `models\bge-small-zh-v1.5-int8\` 放在 `ainwin.exe` 旁边；`vendor\onnxruntime_c_api.h` 已随源码，不需要额外 SDK。

> 这是一个 C++ 常驻 exe，**不是** WinShun-plugins 社区库接受的脚本/文本/图片投稿。按 C++ 模板在自己的仓库或 `.wsplugin` 包中发布（模型约 127 MB、ORT DLL 约 19 MB、exe 约 1 MB）；不要把 `zig-out`、`.zig-cache`、日志或密钥打包进去。官方 JSON Schema 的 `runtime.program` 只枚举脚本入口，因此编辑器会对 `"ainwin.exe"` 这一行提示；其余字段仍按该 Schema 校验。

自检：

```powershell
.\ainwin.exe --plan "大于10M的数据库文件"
.\ainwin.exe --embed "报销的规定" "费用管理办法 差旅费用申请与报销规定" "租赁合同 房屋租赁合同"
```

`--plan` 打印解析结果；`--embed` 打印模型、维度、加载/查询/文档耗时和余弦值。

## 验证记录

发布包不包含测试脚本（`tests/` 已在打包前清理）。以下结果来自清理前的真实 JSON-RPC 往返和本地模型自检，保留作发布记录。

**测试机状态（2026-10-28，Win顺 0.14.0，已打开插件并确认 `winshun:search`/`winshun:open`）**：

| 输入 | 真实往返（`plugins.log`） | 结果 |
|---|---|---|
| `找 预算` | 2 次工具调用，202 ms | 12 条，副标题有“命中 预算” |
| `找 报销的规定` | 6 次调用，1.4 s（含 18 条向量重排 0.54 s） | 12 条，同段命中短语排前 |
| `找 视频` | 1 次 `ext:...`，36 ms | 12 条视频 |
| `找 桌面上的合同` | 2 次，60 ms | 1 条桌面合同 |
| `找 大于1M的文件` | 1 次 `size:>1mb`，52 ms | 10 条（跳过系统/开发缓存） |
| `找 大于10M的数据库文件` | `ext:db,... size:>10mb`，49 ms | 3 条 |
| `找 大于10kb的数据库脚本` | `ext:sql size:>10kb` 返回 0（测试机没有大 SQL） | 空状态，不拿 `.db` 冒充 |
| `找 大于1kb的JavaScript文件` | `ext:js,... size:>1kb`，75 条候选，33 ms | 12 条，包含 `app.js` |
| `找 大于1kb的可执行文件` | `ext:exe,... size:>1kb`，60 ms | 12 条，含 `VC_redist.x64.exe` |
| `找 最近半年客户签字的租赁合同扫描件` | 8 次调用，“租赁合同”短语命中，1.48 s | 12 条 |
| `找 上周开会拍的现场照片` | 5 次调用，最后按类型+日期列出图片，0.92 s | 12 条图片 + 说明 |\n| `找 数据库脚本文件` | `ext:sql`，100 条候选，551 ms | 12 条 |\n| `找 昨天微信文件夹的压缩包` | 名称未命中后按类型+日期列出压缩包，476 ms | 7 条压缩包 + 说明 |
| `找 `（空） | 无工具调用 | 示例与键盘说明 |

C++ 类型解析、`ext:cpp` 查询和 `server.cpp` 夹具均通过；测试机索引只返回了一个 Windows SDK 头文件，被“系统代码目录”规则过滤，因此该测试机查询为空——这不是解析失败，而是索引里没有用户的 C++ 源码。

仍未在测试机验证：内容索引关闭/权限被拒的界面文案（harness 覆盖）、其他 WinShun 版本的参数兼容、超大索引上的少数超时。

## 已知限制

- 仍是“词库/类型/同义词召回 + 本地向量重排”，没有全库语义索引；完全换一种说法且词库不认识的，可能召回不到。
- 只搜索 Win顺 已经索引的位置；未索引磁盘、网络盘、压缩包内部、图片里的文字（除非 Win顺 OCR 收录）不在范围。
- 大小/时间查询依赖索引返回的元数据；内容结果没有大小时，会改走文件名索引，避免把未知大小当成匹配。
- 代码类列表会排除 Windows Kits 头文件等系统源码；要查系统源码可在输入里写明 `*.h` 并关掉“系统代码”规则（当前无开关，代码里是 `systemCodePath`）。
- 一次最多 30 条（默认 12）；线程 4 通常最快，8 在测试机反而更慢（超订），可按机器试。
- 取消由 Win顺 按 `queryId` 丢弃过期结果；插件不写盘，迟到结果没有副作用。

## 许可证

MIT（见 [LICENSE](LICENSE)）。第三方：ONNX Runtime（MIT）、bge-small-zh-v1.5（MIT，BAAI），详见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) 和 `models/bge-small-zh-v1.5-int8/README.md`。
