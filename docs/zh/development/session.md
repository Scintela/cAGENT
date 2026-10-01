# Session 模块开发日志

- 日期：2026-09-30
- 范围：Storage 契约、当前 turn 校验、历史投影和可选 RAM 后端
- 状态：模块契约可单独测试；`agent_run()` 尚未接入，文件系统持久化尚未实现

## 设计边界

| 所有者 | 职责 | 内存 |
|---|---|---|
| Core Session (`src/session/session_manager.c`) | 维护当前 turn 的 user/assistant/Tool 因果顺序；复制本轮消息；选择并校验历史 turn | Core turn scratch |
| Storage 契约 (`include/agent/session.h`) | 定义同步写入、完成/中止、按界读取和清理操作 | 不拥有具体数据 |
| RAM 后端 (`providers/storage/ram/`) | 在应用给定的数组与字节缓冲中保存完整和中止的 turn | 独立于 Core workspace；掉电即失 |
| Model Provider | 将 `agent_message_view_t[]` 编码为具体模型协议 | Provider 自有缓冲 |

`src/session/session_codec.c` 的“二进制快照 + CRC”只是旧骨架，现已移出构建并删除。
JSONL/NVS 的记录格式、校验与恢复应由对应 Storage 后端实现；通用 JSON 语法能力继续由
`codecs/json/` 提供，不进入 Session Core。

## 公开 API 与结构

`agent_session_storage_t` 包含复制到 Agent 的 `ops` 表及借用的 `context` 指针。
`agent_set_session_storage()` 只允许空闲时绑定或替换；`NULL` 解绑。应用负责维持后端状态、
索引和 payload 缓冲的生命周期。未绑定 Storage 时仍可构造一个当前 turn，但请求历史
`max_history_turns > 0` 会返回 `AGENT_ERROR_NOT_SUPPORTED`。

| Ops | 输入/输出契约 |
|---|---|
| `begin(session_id, &transaction)` | 打开一个 turn；失败不得留下调用方需要释放的句柄。 |
| `append(transaction, message)` | 接收一个规范化消息；借用的文本与 Tool call 只在本次调用有效。失败不得部分追加。 |
| `finish(transaction, outcome)` | 结束为 `COMPLETE` 或 `ABORTED`；无论结果如何均消费句柄。只有成功完成的组可被 `recent` 投影。 |
| `recent(session_id, max_candidates, visit)` | 至多回调指定数量的**完整**候选组，按最新到最旧；组内消息按时间正序。回调 view 仅在回调内有效。 |
| `clear` / `clear_all` / `remove` / `count` | 空闲时的会话管理；身份保留语义由后端声明。 |

`agent_session_group_view_t` 是一组完整的 `agent_message_view_t[]`，不是 JSON。
`agent_session_turn_outcome_t` 区分完成和中止；中止组可供后端审计，但不直接送给模型。
现有 `agent_session_clear()` 等公共函数调用绑定后端，不再只清理 `agent_t` 内的数组。
具体后端的耐久性必须单独声明：**同步调用返回成功不自动等于数据已经落盘**。

RAM 后端的 `agent_session_ram_config_t` 由应用提供 turn/message/Tool-call 描述符数组、
payload 字节缓冲和一次读取用的 view 数组。`agent_session_ram_t` 只保存这些指针及使用量。
它支持多个 session ID，按追加顺序保存；容量满返回 `AGENT_ERROR_CAPACITY`，**不自动淘汰**。
`clear` 与 `remove` 在该后端都删除该 ID 的所有记录；由于它不保留空身份，两者效果相同。
无 Tool 的产品可把 `calls/read_calls` 设为 `NULL`，对应容量设为 0。
RAM 后端一次只允许一个活动事务，内部不加锁；若多个 Agent 共用该实例，应用必须串行化
访问，或给每个 Agent 使用独立实例。

内部 `session_internal.h` 的调用顺序是 `agent_session_turn_open()`、若干次
`agent_session_append()`、每次模型请求前的 `agent_session_project()`，最后
`agent_session_turn_finish()`。这些不是应用 API；`react_loop.c` 接入时必须持有一个
scratch mark，在模型调用后回退单次投影，turn 结束后回退整个 turn。`finish()` 返回
Storage 的提交结果，不再以 `void` 吞掉写入失败。

RAM 后端的应用侧初始化形式：

```c
static agent_session_ram_t ram;
static agent_session_ram_turn_t turns[8];
static agent_session_ram_message_t messages[32];
static agent_session_ram_call_t calls[8];
static agent_message_view_t read_messages[16];
static agent_tool_call_view_t read_calls[8];
static char payload[4096];

agent_session_ram_config_t buffers = {
    .turns = turns, .turn_capacity = 8,
    .messages = messages, .message_capacity = 32,
    .calls = calls, .call_capacity = 8,
    .payload = payload, .payload_capacity = sizeof(payload),
    .read_messages = read_messages, .read_message_capacity = 16,
    .read_calls = read_calls, .read_call_capacity = 8,
};
agent_session_storage_t storage;
agent_session_ram_init(&ram, &buffers);
agent_session_ram_bind(&ram, &storage);
agent_set_session_storage(agent, &storage);
```

RAM 总预算约为各描述符数组的 `sizeof * capacity` 之和，加上 payload 大小和
`agent_session_ram_t` 自身。Core 的当前 turn 与历史投影仍消耗独立的
`agent_workspace_t` scratch；在一次模型请求中，历史消息文本与 descriptor 只复制
到满足 `AGENT_MAX_PROJECTED_MESSAGES` 和 scratch 剩余量的范围内。

## 数据流

```text
Application: 配置 Core workspace + RAM Storage workspace，绑定 Storage
Core turn_open: 复制 session_id/user 到 scratch → Storage begin + append(user)
每个模型步骤: Core 校验 assistant Tool call → Storage append → Tool 执行
             Core 校验匹配的 Tool result → Storage append → 再调用模型
模型输入: Storage recent(最近完整组) → Core 校验、复制到 scratch
          → 历史组按时间正序 + 当前 turn → agent_model_request_t.messages
结束: Core finish(COMPLETE/ABORTED) → Storage finish → scratch 回退
```

Core 检查初始 user、assistant 的 Tool call ID 唯一性、Tool result 与未回答 call 的匹配、
重复结果以及 assistant final 必须在全部 Tool result 之后。历史组会重新验证同样的不变量。
投影最多读取 `min(max_history_turns, 剩余消息描述符容量)` 个候选组；单组装不下时整组
跳过，不修改 Storage，也不会留下孤立 Tool result。最终消息视图只在调用方保持 scratch
有效时可用；`AGENT_MAX_CONTEXT_BYTES` 约束 system/context 文本，不作为 Session 消息总量。

RAM 后端当前不自动做长期保留与淘汰，因此容量满时产品必须清理或提供更大独立缓冲。
此行为刻意区别于 V1 因一次请求的 JSON buffer 不够便永久删除历史的行为。

## 失败与耐久性

- `INVALID` / `EXISTS` / `STATE`：消息顺序、重复 ID 或结束状态有误；失败不修改当前 turn。
- `LIMIT`：单对象或消息数超出配置硬上限；`CAPACITY`：scratch 或 RAM 后端缓冲不足。
- `PARSE`：Storage 返回了破坏 turn/Tool 配对的不可信历史组；底层 I/O 错误原样传播。
- `ABORTED` 组不会被正常历史投影。它可保留已提交的 Tool call 等审计事实，但 RAM 后端
  掉电即失，不能证明设备动作可在重启后追溯。

若产品要求 Tool 副作用具备掉电追溯，未来持久化后端必须在执行 Tool 之前确认调用意图
已经稳定写入，并在结束时写入可恢复的完成/中止标记。损坏尾部、重放幂等性、实际介质
flush 语义、文件系统原子性及磨损控制均不由当前 RAM 后端保证。`agent_run()` 尚未实现，
因此上述执行顺序是集成契约，不是已经贯通的产品流程。

## 构建与验证

Host CMake 可设置 `AGENT_BUILD_SESSION_RAM=ON` 构建 `cagent::session_ram`；Core 始终
编译 Session 契约。ESP-IDF 可通过 `CONFIG_AGENT_SESSION_RAM` 选择同一后端。

```sh
bash tests/session/compile.sh
```

契约测试覆盖深拷贝、跨模型步骤重复 Tool ID、Tool result 配对、完整/中止组、历史顺序、
`max_history_turns=0`、多会话隔离、清理后的紧凑化，以及缓冲容量失败。

## 下一阶段

1. 在 ReAct Loop 接入 `turn_open/append/project/finish`，使运行时真正消费此契约。
2. 基于目标平台的文件/Flash 能力实现 JSONL 或分块日志后端；增加截断尾部、掉电恢复、
   提交失败和副作用 Tool 的重启测试。
3. 根据实际存储后端验证是否需要公共耐久等级、用户身份隔离及保留策略 API；不提前把
   文件系统或 JSONL 类型写入 Core 接口。
