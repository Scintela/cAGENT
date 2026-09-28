# cAgentV2 公共 API 设计与收敛建议

> 状态：基于 V1 实际应用复核的修订草案；同步运行 MVP 边界以 ADR 0020 为准。
> 本文定义建议的公开范围、行为和所有权，不表示这些接口已实现或 ABI 已稳定。
> `include/` 已补齐首批公共声明；部分 Core/Port 代码可运行，但 `agent_run()` 执行链尚未实现。
> 下文保留设计评审语境，最新声明快照见 §11.1；声明不等于已接受或通过运行验证。

## 1. 目标与评审依据

目标是在 ESP-IDF、openvela、RT-Thread 和 Host 上，用同一套源码级 API 开发智能家居、
语音助手和设备控制应用。兼容目标是相同配置下的运行语义，不承诺跨平台二进制 ABI。
Core 不创建应用线程，不管理 UI、语音、网络连接恢复或硬件驱动。

轻量化的衡量对象是常驻 RAM、运行峰值、栈深度、阻塞时间、依赖和集成复杂度。
仅减少函数数量、改为静态数组或增加 `Tiny` 名称，都不能证明实现更轻量。

本次核对的 V1 位于 `/home/arongw/openvela/packages/cAGENT`，真实目录为
`/home/arongw/openvela/contest2026_031_niudanxianqianchong/packages/cAGENT`。
同时参考同一工程的 `demos/smart_home` 与 `packages/cagent_addons`。以下为源码观察，
不是本次运行测试或真机验证的结论：

| V1 证据 | 对 V2 的约束 |
|---------|-------------|
| Model ops，Tool/Context/Policy 回调 | 保留类型化能力注入，不要求应用先学习 Plugin 框架 |
| `smart_home_agent.c` 装配 Context、Skill、Tool、Policy、Model | 保留直接装配；成组注册与失败回滚有实际价值 |
| smart_home 枚举工具、调整权限和模型配置 | 枚举与空闲时重配置不是纯粹的预留需求 |
| addons 的 `remote_tool.c` 注册、排空并注销远程工具 | 支持 turn 之间的目录更新，区别于运行中热卸载 |
| 本地与远程 Tool 均使用 `input_schema_json` | 不强制所有远程 schema 转换成受限 C descriptor |
| `agent_t` 内嵌 Session/消息/Tool-call 最大数组 | 保留编译期容量 Profile，但使用共享 payload/descriptor pool，避免多层最大容量相乘 |
| `agent_config_tiny()` 只调整 limits；每轮另分配 arena | Tiny/Default/ReAct 改为 build Profile；运行 limits 与编译期容量分离，Core 不再每轮分配 arena |
| Tool 执行后模型失败，loop 保留提示但丢弃未完成 turn | 区分运行失败与动作未执行，保留部分执行事实 |
| `memory_store.c` 返回 NOTSUP，`llm_router.c` 为占位 | 不因 V1 有声明就承诺 V2 已有对应能力 |

本次所读 V1 `session.h` 没有原表格声称的 `agent_session_set_store()`；本文不再把
完整 Storage API 标为已经验证的 V1 迁移项。

## 2. 公开边界与首批范围

| 类别 | 使用者 | 范围 |
|------|--------|------|
| 应用 API | 产品开发者 | 初始化、运行、工具注册、会话、策略、事件、统计 |
| 扩展 API | Provider/Port/addon 作者 | Model ops/sink、Transport、Runtime、可选能力包 |
| 实现私有 | 库维护者 | arena、注册表布局、状态机内部字段、缓存、Storage Provider 内部索引与 payload |

首批闭环：一个 Agent 同时处理一次同步运行，Model + Tool + 有界 Session + Context/Skill 注入，
配合 Policy、取消与事件。可暂停的人工确认延期；Skill 和 Context 可关闭，不作为启动必需项。
MCP、远程 Node、业务设备 Tool Pack 继续作为独立 addon。

`include/agent.h` 聚合常用应用 API；扩展作者按需包含其他头，不自动引入厂商、OS 或
第三方 JSON 类型。下表是规划，未创建的头不代表可用功能。

| 头文件 | 范围 |
|--------|------|
| `agent.h` | 生命周期、同步运行与取消；聚合基础类型、配置、Tool、Policy、Event、Session |
| `agent/types.h`、`error.h`、`config.h` | 最小公共值类型、错误、编译期容量与默认配置 |
| `agent/tool.h`、`policy.h` | 设备工具与授权契约 |
| `agent/event.h`、`session.h` | 观测与有界会话管理 |
| `agent/model.h`、`runtime.h`、`transport.h` | 能力绑定与扩展接口；按需包含 |
| `agent/context.h`、`skill.h` | 可选上下文贡献；文件 loader 在外围 |
| `agent/model_mock.h`、`model_openai.h`、`transport_http.h` | 具体 Provider，独立构建与包含 |
| `agent/runtime_<platform>.h` | 平台 Runtime 工厂；TLS 配置属于 Transport 头 |
| `agent/plugin.h`、`storage.h`、`storage_jsonl.h`、`memory.h` | 延后稳定，见 §10；最小应用不依赖 |

### 2.1 公共值类型与所有权

| 类型 | 公开内容及约束 |
|------|----------------|
| `agent_t` | 不透明句柄，不允许应用依赖内部布局 |
| `agent_string_view_t` | data + size；不假定 NUL 终止，NULL data 只允许 size 为 0 |
| `agent_config_t` | 系统提示、默认限制、Runtime；不包含编译期容量、厂商配置或完整插件管理状态 |
| `agent_request_t` | 输入、session/trace、limits、user_data；不保存厂商请求 JSON |
| `agent_response_t` | 有界输出、执行状态与本轮摘要；具体布局见 §4 |
| `agent_message_view_t`、`agent_tool_call_view_t`、`agent_tool_view_t` | Model 扩展所需的规范视图，数组有数量，字符串有长度 |
| `agent_tool_t`、`agent_context_provider_t`、`agent_skill_t` | 贡献定义、callback/context 与元数据，不公开注册表条目 |
| `agent_limits_t` | 每 Agent/turn 的运行限制；不得突破 build Profile 的物理容量 |
| `agent_stats_t`、`agent_event_t` | 统计快照与临时观测负载，不作为可修改内部状态 |
| `agent_workspace_t` | 当前 build Profile 决定大小与对齐的 Core caller-storage；不包含 Provider/TLS/HTTP 内存 |
| `agent_model_workspace_t` | 固定大小的 Model wrapper caller-storage；Provider 状态仍在 Core 外部 |

统一使用 BORROWED（外部持有）、COPIED（库复制）、TRANSFERRED（成功才移交所有权）、
VIEW（特定期限内只读）标注。每个 view 必须明确有效到哪次操作，不能只写“临时有效”。
复制包含指针的结构体只复制指针值，不意味着复制指向的数据。init 复制 config 值，
其中提示文本、Runtime context 等借用对象须覆盖 Agent 使用期；后续各节细化其他寿命。

## 3. 配置、内存与生命周期

### 3.1 基本入口

建议普通初始化直接接收 `agent_config_t`，不强制经过 `agent_composition_t`：

```c
agent_config_t agent_config_default(void);

agent_error_t agent_init(agent_t **out, agent_workspace_t *workspace,
                         const agent_config_t *config);
agent_t *agent_create(const agent_config_t *config);
agent_error_t agent_start(agent_t *agent);
void agent_destroy(agent_t *agent);
```

- `agent_init()` 绑定编译期 Profile 对应的 `agent_workspace_t`，初始化固定 pool 并进入
  CONFIGURING；合法 out 参数在失败时置 NULL。失败不释放 caller workspace，也不申请 Core heap。
- `agent_start()` 校验 Core 配置和固定 pool 状态并进入 READY，不隐式发起模型请求。首版允许
  无 Model 的 READY；需要模型的 run 在使用点检查 binding。失败仍在 CONFIGURING，允许
  修正配置或销毁。
- `agent_create()` 是可选 heap 包装；Agent 记录 workspace 来源与配对 allocator。
  NULL 表示失败，需要详细错误的调用者使用 init。
- `agent_destroy()` 适用 init/create：清理内部及明确 owned 的资源，只有 create 路径
  释放 workspace。NULL 安全；禁止与运行、回调或 Provider 调用并发。
- 首版不同时增加 `deinit()`，也暂不公开语义混杂的 `stop()/reset()`；清会话使用
  Session API，取消使用 cancel。今后按明确的重新配置或统计重置需求独立设计。

生命周期建议为 CONFIGURING -> READY -> ACTIVE -> READY。同步 `agent_run()` 返回时
不留下可恢复的活动 turn；运行期间禁止修改注册对象。

### 3.2 编译期容量与 workspace

Tool、Context、Skill、scratch、输入、schema、arguments、Tool/模型输出与 JSON 深度的容量均由
build Profile 的 `CONFIG_AGENT_*` 宏决定。Kconfig/CMake 同时决定模块
是否编译；未启用模块不进入依赖图和 Core workspace。普通应用应选择 Tiny、Default、Device
ReAct 或产品 Profile，而不是逐项设置全部容量宏；细项只在 Custom/Advanced Profile 中覆盖。

应用使用 `agent_workspace_t` 取得正确大小与对齐，不猜测 `unsigned char[]` 的长度。Core
workspace 包含 Agent、固定 registry、session binding/cursor、turn 状态与可复用 scratch；不包含
完整 Session 历史、借用字符串、Model Provider、cJSON DOM、TLS/HTTP 缓冲或应用线程栈。后者
必须作为产品外部内存预算单独测量。

Core 在 init 时先预留 registry、session binding/cursor 等跨 turn 状态，再建立一个 turn scratch arena。
context、消息/Tool view 数组、arguments 和临时 Tool result 在同一轮内按需共享该 arena，turn
结束后整体复位。因此 context 可以使用本轮未被其他临时内容使用的空间；长期状态不得侵占
scratch，单项内容仍受 Profile 的保护上限约束。Tool schema、名称和描述是 borrowed view，
其字节上限主要用于注册/解析准入，不意味着 Core 为每项预留固定 buffer；Provider 序列化所需
JSON buffer 属于外部 Provider workspace。仅有一个 `AGENT_CORE_WORKSPACE_BYTES` 无法表达这些
隔离规则，不能作为唯一容量配置项。

`agent_init()` 只绑定固定 workspace、初始化 pool 并验证运行期配置，不在主路径申请 heap。
运行时复用 scratch/容量槽，不再每轮创建 request arena。该规则不承诺 Provider/codec/Transport
全链路零分配，cJSON 例外见 §6.2。

Tiny/Default/Device ReAct 是构建 Profile，不是 `agent_config_tiny()` 之类运行期容量切换。
同一静态库构建只对应一组 Core 容量；不同产品容量需要重建 Profile。

`agent/config.h` 按 `AGENT_*` 直接宏、生成的 `CONFIG_AGENT_*` 宏、内建 Default 值的顺序
选择容量。`AGENT_CORE_WORKSPACE_BYTES` 是当前 Profile 的 Core 总 caller-storage；Core 实现必须
在编译时验证其足以容纳内部布局。Model wrapper 不占用该空间，需要 caller-storage 时使用
`agent_model_workspace_t`，其大小由 `AGENT_MODEL_WORKSPACE_BYTES` 决定。

### 3.3 默认限制与请求覆盖

保留 `agent_set_limits()`，只在 CONFIGURING/READY 调用，修改后续运行默认值。
每个 turn 复制有效 limits，不临时改写 Agent 默认配置。

请求 `limits == NULL` 表示继承；非 NULL 建议完整覆盖，不以零值表示逐字段继承。
时间限制为 0 表示无该项运行限制；`max_tool_calls == 0` 建议表示禁止工具调用；
`max_steps` 必须非零；`max_output_tokens == 0` 表示未指定模型 token 预算。
这些计数规则仍需在头文件定稿时统一，不沿用未定义的 V1 行为。

编译期字节/数量上限不能被请求覆盖。产品若有不可放宽的步骤、时间或调用上限，应以 build
Profile 或独立 admission 检查拒绝越界请求；不能把可修改默认 limits 同时当作产品硬限制。
Token 预算不能替代输出字节上限，也不能假定 provider/model 一定遵守请求参数。

## 4. 同步运行与结果

MVP 只公开 `agent_run(agent, request, response)` 作为一次请求的同步入口。`agent.h`
同时声明 `agent_cancel(agent)` 和 `agent_cancel_token_is_set(token)`；不公开 turn 句柄、
step 或 resume。同步调用返回后不留下活动运行。完整执行链仍在实现中。

input/session/trace view 与 user_data 由调用方保持有效直到 `agent_run()` 返回；
指向的文本在运行期间不可修改。Model/Tool 可以同步阻塞，应用需要非阻塞 UI 时应在
自己的工作任务中运行 Agent；取消是协作式检查，不保证打断阻塞中的 SDK。

### 4.1 结果与部分执行

- `agent_response_t` 使用 caller buffer；`output_written` 不含终止符，非零容量须以
  NUL 终止。`output_required`、`output_truncated` 和 `delivery_status` 区分输出交付
  与 `status` 所表示的执行结果。
- 输出交付失败不重新执行 Model/Tool；本轮 `summary` 记录工具是否执行、成功/失败
  次数和 scratch 峰值。累计 stats 不能替代本轮摘要。
- 超时或取消不意味着已执行的设备动作被撤销。具体设备结果查询、幂等性和未知执行
  状态由 Tool/addon 处理；Core 不自动重试副作用工具。
- Event/sink 的 view 仅在回调期间有效；流式文本交付如需向应用公开，应另行定义
  有界 sink，不复用事件回调。

### 4.2 确认失败关闭

MVP 没有暂停或恢复能力。`AGENT_POLICY_CONFIRM` 或
`AGENT_TOOL_REQUIRES_CONFIRM` 必须阻止 handler 执行，不得自动批准或留下待恢复
的隐藏 turn。Tool guard 和完整 run 尚未实现，此处是后续实现的强制规则；
需要异步人工确认的产品应在应用层管理该流程，未来再单独评审公开 resume API。

## 5. Model、Runtime 与 Transport

### 5.1 Model

Model ops 公开给 Provider 作者。输入为 system prompt、规范消息/工具视图、trace/session、
有效 timeout/token 预算和 cancel token；不携带整段 OpenAI messages/tools 请求 JSON。
通用 `arguments_json/input_schema_json` view 仍允许存在。

Model sink 由 Core 提供，有界追加文本与完整 Tool call，在返回前复制借用数据。
sink 返回容量/取消错误后 Provider 必须停止生产；complete 失败时，已追加的临时数据
不当作成功 final。同步 complete 返回后不得继续调用 sink。

| API/能力 | 收敛建议 |
|----------|----------|
| `agent_model_ops_t`、request、sink | 扩展 API 公开，保留类型化 callback |
| `agent_set_model()`、`agent_set_model_owned()` | 保留 borrowed/owned 绑定，V1 有 owned 用例；仅 CONFIGURING/READY |
| `agent_get_model()` | 借用句柄，用于产品空闲时更新配置；不能绕过单写者规则 |
| `agent_model_create()/destroy()` | 可选 heap 路径；明确包装对象与 provider state 各自的 allocator/销毁责任 |
| 静态 Model 绑定 | 首版应有 ops + context 的无额外 heap 包装路径；值类型 binding 或 caller storage 的签名待定 |
| `agent_model_complete()/cancel()` | 默认内部调度；确有 router/decorator 用例再作为扩展 API 公开 |
| `model_openai_*` 配置更新 | 保留实际产品需求；无活动调用时完整校验并原子替换，不留下部分更新 |
| Mock Provider | 独立头与构建目标，不放进通用 Model 头 |

owned 绑定成功才转移所有权，失败由调用者清理；替换失败保留旧绑定。相同句柄重复
绑定、borrowed/owned 切换及跨 Agent 共享需明确，不能重新 set 时先销毁同一个对象。
首版不承诺同一可变 Provider 跨 Agent 并发安全。

### 5.2 Runtime 与 Transport

Runtime 负责 allocator、单调时钟、日志及按需同步。callback 成对验证，不能为缺失
项混入不匹配的默认 allocator/锁。单写者路径不强制创建 mutex；静态 Core 不必要求
heap allocator，使用 create 或分配型 Provider 时才要求相应能力。

HTTP Transport 是 Provider 的依赖，建议经 Model 配置注入。`agent_set_transport/_owned`
移出首批 API，避免给不联网的 Model 强加 Agent 级网络服务。Transport 可以复用平台
组件，独立约定缓冲和销毁；不公开平台句柄。

Transport request/sink 是扩展公开面，可以包含 HTTP method、地址、headers、body。
“不公开厂商 wire format”不意味着禁止适配作者看到 HTTP 字段。URL 或 host/path/port
选择一种权威表示，不允许两套地址互相矛盾。body chunk 不等于完整 JSON；HTTP 分帧
归 Transport，模型 SSE 事件归 Provider。

平台 Adapter、构建裁剪、同步 callback、借用生命周期和通用 destroy 边界的候选方案见
[ADR 0007](../adr/0007-http-transport-adapters.md)。该 ADR 仍是提案，不改变当前头文件。

## 6. Tool 注册、schema 与执行

### 6.1 注册接口

| 接口 | 行为 |
|------|------|
| `agent_register_tool()` | 复制定义值，检查名称冲突、容量和字段上限；失败不产生半注册项 |
| `agent_unregister_tool()` | CONFIGURING/READY 中移除；不负责断开外部连接或释放 addon 对象 |
| `agent_tool_set_enabled()/is_enabled()` | 控制调用及模型可见列表，变更使 schema 缓存失效 |
| `agent_tool_enumerate()` | 支持设置页与诊断；回调不得修改注册表或保留 view |
| `agent_tool_execute()` | 首版内部；独立调用需来源、授权、预算、结果生命周期，不能直接开放内部 handler 路径 |

注册定义中的名称、说明、schema、user_data 默认 BORROWED，保持有效直到注销返回或
Agent 销毁；描述数据在注册期间不可修改，user_data 指向的业务状态由应用同步维护。
允许静态字符串留在只读存储，动态 schema 由 addon 持有。首版不隐式深拷贝；如需
copied 注册，必须显式纳入持久化字节池预算。

`flags == 0` 表示启用且对模型可见，不表示已授权。DISABLED 禁止调用且不参与投影；
HIDDEN 不进入投影，也拒绝模型路径按名调用，但不自动定义其他来源的授权。
REQUIRES_CONFIRM 必须由流水线处理，不能因无 Policy 而跳过。READ_ONLY、SIDE_EFFECT
是实现声明，其可信度由产品 Policy 判断。顺序执行的首版暂不公开 PARALLEL_SAFE。

group/category 可保留给分类和来源展示，但不能仅凭可重复数字 group 实现资源所有权
或卸载；需要 owner/scope 时独立定义，避免能力包互相注销对方贡献。

### 6.2 Schema 与参数校验

本次建议保留显式长度的 `input_schema_json` 为首版注册输入，兼容 V1 本地与远程 Tool。
类型化 descriptor 可作为生成辅助，不要求把所有远程 schema 无损转换成受限 C 类型。
两种来源同时存在时必须指定唯一权威输入，不能生成不同的定义。

JSON codec 检查完整文档、类型、字节/深度限制；这些检查不等于 JSON Schema 语义验证。
Tool 自身或显式 validator 必须验证 required、范围、设备约束等执行前置条件。声明
支持的 schema 子集需有测试，不支持规则不能被声称已验证。重复键、嵌入 NUL、未知
字段策略须统一，避免 Policy 与 Tool 对同一输入产生不同解释。

继续采用 cJSON 作为首版 codec 私有依赖，不暴露 `cJSON *`。准确边界是：Core 不绑定
JSON 库类型、不构造厂商请求；公共接口可携带通用 JSON view。若 Tool 参数 codec
在运行时用 cJSON，其临时分配同样是需测量的外部内存，不能宣称整个执行链零 heap。

此项是对 [ADR 0006](../adr/0006-json-integration.md) 的修订提案，尚未同步其中
“descriptor 是唯一来源”的已接受决定。实现 schema API 前必须统一两份文档，不能
同时执行冲突要求；本提案不改变选择 cJSON 的决定。

### 6.3 Tool 输出

Tool callback 应获得有效 deadline、cancel token 和有界输出能力。优先采用 caller
buffer 或 sink，签名待定。不能仅返回未定义寿命的栈内字符串。若保留 result view，
必须约定 Core 复制时点及提供者可重用缓冲的时点。
Tool 结果不强制为 JSON，文本也可由 Model Provider 序列化；执行状态和诊断分开保存。
确认/校验失败不得调用 handler。

## 7. Policy、Context、Skill 与观测

| 能力 | 第一版建议 |
|------|------------|
| Policy | 保留单个 `agent_set_policy_callback()`，由应用组合产品规则；注册式策略链暂缓 |
| Policy request | Tool 信息、参数、调用来源、session/trace、有效限制；仅回调期间有效 |
| Context | 保留 register/unregister、优先级、有界 build；区分必需和允许跳过内容 |
| Skill | 保留 register/unregister 和有界 context 文本，兼容 V1；loader/文件格式在外围 |
| Event | 保留单个 `agent_set_event_callback()`，用于观测，不隐式修改授权或执行控制 |
| Stats | 保留 `agent_get_stats()`；累计统计与本轮摘要分开，由驱动任务读取或外部同步 |
| Error | 采用单一负值 `agent_error_t` enum，分通用错误与少量模块错误；调用边界、Event kind 和日志表达来源，字符串不承担结构化状态协议。Tool effect 仍是独立事实。详见 [ADR 0008](../adr/0008-error-contract.md)。 |

Context/Skill 使用与 Tool 类似的借用/注销生命周期；注册不自动拥有外部资源。
贡献数量、动态输出和最终上下文均有预算，不以无提示截断替代容量错误。
Policy/Context/Event 回调在驱动任务同步执行，不重入 Agent；慢日志、UI、网络工作由
应用复制必要信息后入队。

## 8. Session 与失败事实

Session 的权威历史属于可替换的 Session Storage Provider；Core 仅维护当前 turn 的事务、完整
turn/Tool 配对不变量和本次投影。`agent_session_clear()`、`clear_all()`、`count()` 与未来
`remove()` 应委托 Provider 实现；具体是否存在默认 session、列举范围和删除语义由稳定的
Storage 契约定义。当前头文件仍是过渡草案，不应据此推断 Session 历史保存在 Core workspace。

Session 保持 Tool call/result 配对，按完整 turn 记录、淘汰与投影。失败不能仅删除已执行工具的
事实并表现为从未发生；至少保留 abort/部分执行记录，并定义其后续模型投影，避免孤立 Tool
result。Provider 追加前应预留失败收尾所需空间，不能直到溢出才尝试记录 abort。具体日志格式、
缓存、索引与持久化介质保持 Provider 私有。

`max_history_turns` 是每次模型请求投影多少个此前完整 turn group 的运行期窗口；它不是历史
保留策略，也不等同于 `max_steps`。每轮还必须受 build Profile 的
`AGENT_MAX_PROJECTED_MESSAGES`、Context 和 scratch 预算限制。保留、JSONL/Flash/NVS 编码、
PSRAM 热缓存、checkpoint 和恢复策略都属于 Session Storage Provider。权威规则见 ADR 0016。

## 9. 并发、动态目录与能力包

一个 Agent 一个驱动任务；不同 Agent 独立运行，共享 Provider/设备资源由应用同步。
所有入口仍须检查非法状态，外部串行化不代替状态验证。

跨任务 cancel 必须有 Port 提供的同步保证；缺少该能力时只能使用串行调用配置，不能
用 volatile 代替同步或仍宣称跨任务安全。取消与销毁的并发仍由应用禁止。

| 操作 | CONFIGURING | READY | ACTIVE（同步运行中） |
|------|-------------|-------|---------------------|
| Model 绑定、默认 limits 等可变配置 | 允许 | 空闲重配置允许 | BUSY |
| Tool/Context/Skill 注册、注销、启停 | 允许 | 预分配容量内允许 | BUSY |
| Policy/Event 替换、Session 修改 | 允许 | 允许 | BUSY |
| run | 不允许 | 允许 | BUSY |
| cancel | 无操作 | 无操作 | 有同步保证的跨任务请求 |
| destroy | 允许 | 允许 | 禁止，等待 run 返回 |

Runtime、workspace 和编译期容量在 init 后不变；表中的重配置不包含这些字段。
网络 reader 只更新 addon 连接状态/入队，注册表 mutation 由驱动任务在 turn 之间执行。
远程对象离线先阻止新调用，等待引用排空，再注销释放。Core 不管理连接代次，但拒绝
ACTIVE 中的目录变更。

静态能力包先用普通装配函数贡献多项注册，失败逆序撤销自己的成功项，并保持外部对象
寿命覆盖注册期。需要通用批量事务时使用有界存储实现；暂不稳定公开
`scope_begin/add/commit/abort` 和可变 Composition builder 两套并行装配协议。
不支持运行中热卸载二进制，不妨碍空闲时更新工具目录。

`agent_cancel_from_isr()` 暂缓成为所有 Port 必需 API；默认由 ISR 通知任务再取消。
直接 ISR 取消需明确 Port 原子/临界区能力与句柄寿命。普通 cancel 也不能因为置位
部分是原子的，就把包含 Provider callback 的整个实现标为 ISR-safe。

## 10. 延后公开的能力

| 原草案项目 | 处理与重新引入条件 |
|------------|--------------------|
| `agent_composition_t`、init/add_plugin | Profile 可先组织静态配置；通用 builder 明确容量、所有权、回滚后再公开 |
| Plugin descriptor/scope/依赖管理 | 保留扩展边界；能力包出现重复生命周期需求后稳定 API，不作为最小应用必经路径 |
| Storage set/open/append/checkpoint/sync | 可选后端；先明确恢复读取/遍历、版本、损坏尾部、错误传播与 Flash 写入策略 |
| 无参数 `storage_required_buffer_size()` | 不保留固定需求假设；缓冲需求由后端及配置决定 |
| `memory_snapshot/restore/free`、Memory ops | V1 未实现；先与 Session 持久化、长期检索划清边界 |
| Policy 注册链 | 多个独立能力包需贡献策略时再加，定义 DENY 优先、确认合并及移除规则 |
| `agent_stop/reset`、ISR cancel | 按实际需求和状态契约评审，不靠名字预留能力 |
| `agent_turn_begin/step/resume/end` | 需跨回调持有状态、确认 nonce 和可恢复资源；MVP 不公开，见 ADR 0020 |
| 公共 Loop ops、router 管理器 | 第二种编排/路由实现出现前不公开；Model wrapper 可在扩展侧探索 |
| 全异步 Model/Transport | 先定义完成通知、取消后排空、buffer 寿命与背压，再设计 API |

`*_simple` 不逐项复刻，也不因名字统一一律禁止。能减少常见误用且不引入第二套执行
逻辑的包装才值得保留；首版先提供默认配置和统一结构体入口。

## 11. 头文件落地前的验收与待决事项

先以应用和失败路径验证契约，再冻结签名：

- Mock + caller workspace 跑通 input -> final，Model binding 不要求额外 heap。
- Tool -> result -> final、确认要求拒绝/取消、动作成功后模型失败；输出交付失败不隐式
  重执行。验证无 handler 副作用、view 寿命与 deadline。
- Profile 布局与 `agent_init()` 使用同一内部计算，覆盖对齐、恰好足够、少一字节、乘加溢出和
  初始化失败清理；实际布局超出 `agent_workspace_t` 时必须在构建期失败。
- 远程目录上下线/重连、ACTIVE 拒绝 mutation、注销后才能释放 user_data。
- borrowed/owned 替换失败、相同句柄重绑、schema/输出超限及每条清理路径。
- 最小构建不链接网络/Storage/Memory/Plugin；公共头独立 C99 编译，各 Port 运行相同
  行为测试，另报 RAM/ROM/栈峰值。承诺 C++ 可包含时还需验证头文件兼容性。

定稿前必须完成以下决定，不能由实现者分别猜测：

1. 同步 [总体架构](../architecture.md)、[模块地图](../arch/README.md) 与 ADR 0006：
   生命周期/Composition 收敛、schema 输入、JSON 依赖边界及首批范围。
2. 静态 Model binding 的结构/签名、owned 销毁责任、平台大缓冲 allocator 注入。
3. 同步 response 的执行/交付状态、Tool 有界输出签名、取消与确认失败关闭；
   step/resume API 延后独立评审。
4. 限制零值、硬上限布局、schema validator 支持范围及不支持规则的处理方式。
5. config/ops 是否采用 `struct_size`/版本字段及兼容规则。字段本身不自动提供 ABI
   兼容；初期以同版本源码构建为基线，不给所有值类型机械添加版本开销。

验收重点是保留 V1 已使用的扩展能力，让普通应用直接完成装配，同时使存储、所有权
和运行失败行为适合有界 MCU 环境。以上是后续交付要求，不是当前骨架已通过的测试。

### 11.1 头文件声明快照（2026-09-22）

已将首批接口落为可编译声明，仍为接口草案，而非实现或稳定 ABI：

- `agent.h` 聚合应用接口；新增 `context.h`、`transport.h` 补齐已有模块的扩展契约。
  Runtime 不含网络/TLS，HTTP Transport 经 Model Provider 配置注入，不增加
  `agent_set_transport()`。具体 Model Provider 工厂头留待实现时定义。
- Model 保持不透明句柄，使用类型化 `agent_model_workspace_t` 的 `agent_model_init()` 支持
  caller-storage；`agent_model_create()` 使用显式 allocator。wrapper 是否调用 provider 的 destroy
  与 Agent 是否拥有 wrapper 是两个层次，成功才移交相应清理责任。
- 同步 response 分开记录 execution status 与 delivery status，输出不足不得触发
  工具重执行；step/确认 nonce 不属于 MVP 公开接口。
- Tool/Context 使用同步有界 sink。请求限制非 NULL 时完整覆盖默认值；时间限制 0
  表示不增加该项限制，工具调用上限 0 禁用工具，max_steps 必须大于 0。产品不可放宽的
  硬限制暂由应用 admission 检查，未机械增加另一套未验证的 Core 限制结构。
- Core 与 turn 使用单驱动者生命周期。跨任务取消依赖 Runtime 的配对同步回调，
  不承诺 ISR 安全，不公开 `agent_cancel_from_isr()`。Session、注册表为内部状态，
  共享 Provider/设备/连接可由应用持有，不强制统一挂载生命周期。
- 长期 Memory、通用 Plugin 和完整 Storage 不为填满头文件而新增接口；保留明确的
  延后说明。内部结构和协作函数不属于应用 API。
- Tool 头暂按本文 raw JSON Schema 提案表达，并标注与 ADR 0006 的冲突；本次没有
  修改已接受 ADR，也没有实现 codec。实现前仍须统一 schema 来源与验证规则。
- `AGENT_SV_LITERAL()` 明确为字符串字面量的聚合初始化器，可用于 C99/C++11 静态
  对象；赋值和函数实参使用 `agent_string_view()`，避免依赖 C 的扩展静态初始化行为。

`bash tests/headers/compile.sh` 检查所有头的独立/重复/组合包含与用法片段；这些检查
不链接未实现的函数，不验证 plan 布局、所有权释放、并发、JSON 或真机运行行为。
