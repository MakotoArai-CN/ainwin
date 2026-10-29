# 模型、上下文与多语言

## 档位

| 档位 | 模型 | 体积 / 维度 | 建议 |
|---|---|---|---|
| `auto` | 按内存和已装模型 | — | 默认 |
| `light` | bge-small-zh INT8 | 24 MB / 512 | 8–16 GB 内存、老机器、追求快 |
| `balanced` | bge-base-zh INT8 | 103 MB / 768 | 16 GB 以上、质量优先 |
| `large` | bge-large-zh INT8 | 327 MB / 1024 | 需自备，不随包 |
| `custom` | 自己的 BERT WordPiece ONNX | 任意 | 已有导出模型 |

`auto`：内存 ≥16 GB 且 base 存在时用 `balanced`，否则 `light`。

## 上下文

- 设置为 `0` 时自动：按查询长度增长，默认上限 256；
- 「自动扩容上限」可调到 512，设为 0 使用模型上限；
- 固定值范围 64–512；
- 上下文只影响向量重排读多少文字，不改变类型、大小、文件夹这些硬过滤。

## 多语言（实验，未随包）

用 `hf-mirror.com` 实测 `distiluse-base-multilingual-cased-v2` INT8（768 维，WordPiece）：

| 指标 | 日语 12 条烟测 |
|---|---:|
| Accuracy@1 | 0.583 |
| Recall@1 / @5 / @10 | 0.583 / 0.833 / 1.000 |
| MRR@10 / nDCG@10 | 0.696 / 0.768 |
| 加载 / 12 段 / 12 查询 / 总计 | 1101 / 1184 / 509 / 2796 ms |

数据集在 [bench/retrieval_ja.json](bench/retrieval_ja.json)。这只是烟测，不是正式基准。

multilingual MiniLM/e5 使用 Unigram，bge-m3 使用 SentencePiece，当前 tokenizer 只支持 WordPiece，不能直接换 ONNX。多语言要同时替换模型、tokenizer 和语言包。

## 基准

中文 24 段 / 30 查询的完整结果、测试机配置和复现命令见 [bench/README.md](bench/README.md)。
