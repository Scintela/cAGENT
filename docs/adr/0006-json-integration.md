# ADR 0006: cJSON 集成边界

- 状态：已接受
- 日期：2026-09-22

## 背景

cAgentV2 需要在后续阶段支持：

- OpenAI-compatible HTTP 请求的 JSON 序列化；
- 模型响应与流式 SSE `data` payload 的 JSON 解析；
- Tool schema 到模型可见 JSON Schema 的序列化；
- `tool_calls[].function.arguments` 的解析和验证；
- 可选 JSONL Session Storage 的编解码。

Core 的定位仍是跨 ESP-IDF、openvela、RT-Thread 与 Host 的可移植 Agent 内核。Core
不能为了这些协议需求直接暴露 JSON 库类型，或把厂商 JSON wire format 写入公共 API。

## 决定

第一版选择 cJSON 作为 JSON 实现。cJSON 只在 Provider、Storage 和内部 schema 模块中
使用；Core 和 `include/agent/*.h` 保持 JSON 无关。

允许的依赖方向：

```text
Core / Public API
    <- no cJSON dependency

OpenAI Model Provider / JSONL Storage / Tool Schema Codec
    -> cJSON
    -> Transport / Runtime Port
```

不允许以下行为：

- 在公共结构体、回调或函数签名中暴露 `cJSON *`；
- 让 Session 内存事实直接保存 OpenAI JSON；
- 用原始 JSON Schema 字符串作为 Tool 的唯一真相来源；
- 在 Agent 生命周期中按实例反复调用 `cJSON_InitHooks()`。

## Tool Schema 与 Tool Arguments

Tool 的规范来源是受限的类型化 descriptor，至少表达字段名、类型、required、enum、
数值范围和是否允许未知字段。`tool_schema` 使用 cJSON 将该 descriptor 序列化为模型
可见的 JSON Schema。

cJSON 只负责 JSON 读写，不承担完整 JSON Schema 规范的验证。Tool pipeline 在解析
arguments 后依据 descriptor 执行语义校验；模型返回的未知字段、缺失字段、类型错误、
越界值和无效 enum 必须产生明确的 Tool 参数错误。

OpenAI 的 `arguments` 是 JSON 字符串，解析流程为：

```text
outer model response JSON
-> extract arguments string
-> decode JSON string
-> parse inner arguments JSON
-> validate against Tool descriptor
```

## 内存与分配策略

Core 保持如下规则：`agent_start()` 成功后，Core 主链路不执行不可预测的 heap 分配。
`agent_init()` 的 caller-provided workspace、Session pool 和 per-turn scratch 仍是 Core
状态的资源来源。

cJSON 的 DOM 解析和构造允许在 OpenAI Model Provider、JSONL Storage 等外围模块使用
平台统一 heap。这些分配是 Provider 的外部 transient memory，不计入第一版 Core
workspace 的精确大小；Profile 必须为其声明并测试峰值预算。

第一版不为每个 Agent 设置独立 cJSON allocator hook。cJSON 的 allocator hook 是进程
级全局设置，按 Agent 或按 turn 重新设置会破坏多个 Agent 与并发调用的隔离性。若后续
必须将 cJSON 迁移到静态 pool，只能采用对全部实例一致、并经并发验证的全局 allocator，
或替换 JSON 实现；不能隐式复用某个 Agent 的 scratch。

每个 Profile 至少限制：

- 最大 OpenAI 请求 body 字节数；
- 最大响应 body 和单个 SSE `data` payload 字节数；
- 最大 Tool schema 字节数；
- 最大 Tool arguments 字节数；
- 最大允许 JSON 嵌套深度；
- 单个 turn 可同时保留的 JSON DOM 数量。

## cJSON 使用规则

1. 仅内部 `.c` 文件包含 `cJSON.h`；不得从公共头传递 cJSON 类型。
2. 使用长度感知的解析入口，并根据 `return_parse_end` 实施明确的尾随数据策略。
3. 所有解析后的 root 都在当前函数或统一 cleanup 块中调用 `cJSON_Delete()`；错误路径
   与取消路径同样适用。
4. 请求 body 输出使用 `cJSON_PrintPreallocated()` 和调用方提供的固定 buffer。禁止
   在 Agent 主链路使用 `cJSON_Print()` 或 `cJSON_PrintBuffered()`。
5. 不使用 `cJSON_GetErrorPtr()` 作为跨线程错误通道；使用长度解析入口返回的结束位置
   生成本地错误信息。
6. JSON 解析前先检查 Transport 提供的 body 长度，超过 Profile 上限直接失败。
7. SSE framing 属于 Transport/OpenAI Provider。cJSON 只解析每个已完整提取的 `data`
   payload，不承担 HTTP chunk 或 SSE 边界处理。

## 影响

正面影响：

- 第一版无需维护自有通用 JSON codec，即可覆盖 OpenAI 请求、响应、Tool schema 和
  Tool arguments 的完整路径；
- Tool 与 Model 实现可使用成熟 DOM API，缩短行为测试和联调时间；
- cJSON 不进入公共 API，未来可替换为 coreJSON、jsmn 或 yyjson，而不破坏应用代码。

代价：

- OpenAI/Storage Provider 的 transient heap 峰值不能仅由 Core workspace 精确描述；
- 必须建立 JSON DOM 的释放、取消和错误路径测试；
- 多 Agent 并发时不得依赖每实例 allocator hook；
- 极小 RAM Profile 未来可能需要切换为无 DOM 的 JSON 实现。

## 被拒绝的替代方案

### jsmn + 自有 codec

适合 Tiny Profile，但 jsmn 只提供 tokenizer。实现请求序列化、字符串转义、Unicode
处理、结构化遍历和 Tool 参数校验仍需维护大量自有代码，不适合作为第一版默认方案。

### coreJSON + 自有 codec

适合低内存和高保证环境，但它是读取/验证库，不覆盖 OpenAI 请求和 schema 写入。若
引入它仍需完成有界 writer 和协议映射层，适合后续 Tiny 或安全关键 Profile 评估。

### yyjson

具备强大的读写与 allocator 选项，但第一版的体积与 scratch 复杂度高于当前需求。
可在 Host 或资源充裕的 Profile 中后续评估。

## 验证要求

- 截断、畸形和超限 JSON；
- 控制字符、转义、Unicode 和嵌套 `arguments`；
- OpenAI 成功响应、Tool call 响应、错误响应和 SSE payload；
- 请求输出 buffer 不足；
- Tool descriptor 的 required、类型、enum、范围和未知字段规则；
- 每个 JSON root 的成功、失败、取消路径释放；
- 多 Agent 串行及并发调用下不修改 cJSON 全局 hook；
- Host 下 AddressSanitizer、UndefinedBehaviorSanitizer 和 LeakSanitizer 覆盖。
