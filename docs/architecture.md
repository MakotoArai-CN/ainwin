# 架构与目录

## 目录

```text
src/
  core/
    common.h      UTF-8、字符串、路径工具
    winshun.h     Win顺 C++ SDK / JSON-RPC
  search/
    planner.h     一句话 → 概念、时间、类型、文件夹、大小、排除项
    lexicon.h     概念词库
    settings.h    Windows 设置目录
    appfiles.h    聊天/办公软件传输文件扫描
    embedder.h    本地 ONNX 模型与 WordPiece
  app/
    main.cpp      插件组合根：协议、查询编排、结果渲染
docs/
  architecture.md
  settings.md
  models.md
  limitations.md
  promotion.md
  bench/          可复现的检索基准和日语烟测集
models/           随包的 bge-small/base（large 需自备）
vendor/           ONNX Runtime C API 头
```

依赖方向固定为：

```text
core ← search ← app
```

`search` 不接触 stdio、插件协议或 Win顺 宿主；`app` 只做组合和 I/O。这样以后换宿主或拆出独立程序时，解析、词库、模型可以整体复用。

## 构建

```powershell
zig build
```

或：

```powershell
cmake -B build
cmake --build build --config Release
```

Zig 的产物是 `suixin.exe`。CMake 目标名也是 `suixin`。
