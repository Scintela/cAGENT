# ADR 0022: 私有有界 JSON codec 的第一阶段

- 状态：已采纳（codec 层）；Provider 接入待实现
- 日期：2026-09-28
- 取代：[ADR 0006](0006-json-integration.md) 的默认 cJSON 选择；
  [ADR 0021](0021-platform-cjson-reuse.md) 的平台 cJSON 默认方案

## 决定

首版 JSON codec 使用固定版本的 jsmn tokenizer 加私有读写层，不要求应用
启用平台 cJSON。`codecs/json` 是可选静态构建目标 `cagent_json_jsmn`，不加入
`cagent_core`。公共头文件与 `agent_config_t` 不暴露 jsmn token 或 JSON ops。
当前 Host CMake 使用 `AGENT_BUILD_JSON_CODEC=ON` 构建该目标；ESP-IDF 等
平台的 Provider 打包尚未完成，不能据此宣称设备已可接入模型。

```text
cagent_core                        -> no JSON dependency
codecs/json (private, optional)    -> vendored jsmn + bounded reader/writer
OpenAI Provider (future)           -> codec + Transport contract
JSONL Storage (future)             -> codec if needed
```

第一阶段提供：

- 对整个输入进行严格 JSON 语法、UTF-8、Unicode 转义/代理对校验，然后由
  jsmn 在调用方 token 数组中生成结构视图；token 不拥有输入字节。
- 按对象键、数组序号及组合路径提取值；被查询的对象键重复时拒绝歧义。
- 将 JSON 字符串解码到调用方缓冲，或借用已验证文档的原始值片段。
- 在调用方输出缓冲中编码字符串、`uint32_t`、布尔值和受信任的 JSON
  字面量；缓冲不足或输入无效时锁存错误，最终输出不可用。

jsmn 不等于完整 JSON 校验器。私有读层在 tokenization 前额外校验数值语法、
分隔符、完整根值、控制字符、UTF-8 和 `\u` 代理对。解析深度硬上限为 32，
调用方还可以设置更小的上限；token 和输出缓冲耗尽均明确失败，不静默截断。
`agent_json_writer_literal()` 只允许写入实现自身控制的语法片段；动态内容必须
通过字符串编码或已验证文档的 raw-value 接口写入。
jsmn 实现只在 `reader.c` 的翻译单元内以 `JSMN_STATIC` 编译，不向应用导出
`jsmn_init` 或 `jsmn_parse`，允许应用另行链接自己的 jsmn 实现。

## 尚未解决的契约

`agent_tool_t.input_schema_json` 目前是原始 JSON Schema 字符串，并非类型化
descriptor。第一阶段只提供 JSON 语法和顶层结构检查的基础能力，不实现完整
JSON Schema、required/enum/范围校验，也不修改 `tool.h` 或 `model.h`。
Tool 参数的模型协议解析、schema 投影和具体错误映射归后续 Provider 接入；
Core 的 `src/tool/tool_schema.c` 不因此引入 jsmn 依赖。

当前只有 jsmn 实现，因此**没有** `AGENT_JSON_BACKEND=jsmn|cjson` 选择。
待 cJSON 后端确有需求、且两端有同等协议测试时，再增加构建期选择；
不要预先公开运行时 JSON VTable。平台 cJSON 仍可作为后续可选后端评估。

## 内存与失败边界

codec 自身不调用 heap allocator；输入、token 数组、解码目标和输出缓冲均由
调用方持有。token 对输入的引用与解码输出不得越过调用方指定的生命周期。
这不代表 Model Provider、Transport、TLS 或整个 Agent 执行链已经零 heap。
一个响应可能同时占用输入缓冲、token 数组、解码缓冲和请求输出缓冲，必须
测量组合峰值，而非只计入某一个数组。

私有 codec 使用 `AGENT_ERROR_PARSE`、`AGENT_ERROR_CAPACITY` 和
`AGENT_ERROR_LIMIT` 等现有错误码；未来 Provider 在协议边界映射为
`AGENT_ERROR_MODEL_PARSE` 或 `AGENT_ERROR_TOOL_ARGUMENT`。不因 JSON 后端
选择新增公共错误码。

## 验证

`bash tests/json/compile.sh` 覆盖合法/畸形 JSON、Unicode、嵌套、路径、重复键、
token 耗尽、writer 溢出、确定性随机输入，以及应用另带全局 jsmn 的链接测试。
Host 下还需以 ASan/UBSan 运行。
后续 Provider 阶段必须增加 OpenAI 请求、响应、SSE 与 Tool arguments 的
协议级测试，并在 ESP-IDF、openvela、RT-Thread 目标上测栈、峰值 RAM 与固件增量。
