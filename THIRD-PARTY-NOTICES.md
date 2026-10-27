# 第三方组件

本仓库的插件源码（`main.cpp`、`planner.h`、`lexicon.h`、`embedder.h` 等）为 MIT。

| 组件 | 版本 | 许可证 | 位置 | 说明 |
|---|---|---|---|---|
| ONNX Runtime | 1.31.0 | MIT | `onnxruntime.dll`、`vendor/onnxruntime_c_api.h` | 本机 CPU 推理；运行时 `LoadLibrary`，不链接 import lib |
| bge-small-zh-v1.5 | Xenova ONNX INT8 | MIT | `models/bge-small-zh-v1.5-int8/` | 24M 参数中文向量模型（轻量档） |
| bge-base-zh-v1.5 | Xenova ONNX INT8 | MIT | `models/bge-base-zh-v1.5-int8/` | 102M 参数中文向量模型（均衡档） |
| Win顺 插件 SDK | 随 Win顺 提供 | 见 Win顺 仓库 | `winshun.h` | 单头文件协议封装 |

ONNX Runtime 版权：Copyright (c) Microsoft Corporation. All rights reserved.

bge-small-zh-v1.5 / bge-base-zh-v1.5 版权：Copyright (c) 2023 BAAI（北京智源人工智能研究院），MIT License。

这些组件只在本机运行；插件不包含模型服务、API 客户端或联网代码。
