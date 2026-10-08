# Storage Provider 与 File Store

这些交付物是可选包，不属于 Core 的必需依赖。应用提供所有状态、缓冲和挂载目录，
不能只启用构建开关就得到一个初始化完成的存储实例。

## RAM Session

公共头 `agent_session_ram.h`。

`agent_session_ram_config_t` 接收 turns/messages/calls 元数据数组、payload、
read_messages/read_calls 数组及各自容量。它们互不重叠，至少覆盖绑定寿命。
`agent_session_ram_init()` 建立空易失后端，`agent_session_ram_bind()` 导出 Storage。
然后用 `agent_set_session_storage()` 绑定 Core。

数组槽位和正文 payload 都可能限制存储，不按“保存几个 Turn”就能推导字节数。
没有文件 I/O、隐含 Heap 或重启持久化。

## JSONL Session

公共头 `agent_session_jsonl.h` 与 `agent_session_jsonl_files.h`。

| 配置 | 用途 |
|---|---|
| files / file_context | Session 专用文件操作表和 context |
| write_line | 当前 Turn 编码记录 |
| read_line | 单条历史记录读取 |
| decoded | 解码后的消息/调用字符串 |
| token_buffer / bytes | 有 int 对齐的私有 JSON token 存储 |
| read_messages / read_calls | 完整组的规范视图数组 |
| session_id | 活动事务 ID 的独立存储 |

所有容量由调用方配置，不重叠。单条 Turn 可能包含多次模型/Tool 结果，
write_line 应按最大完整组而非单条消息估算。

`agent_session_jsonl_files_init()` 将通用 File Store 映射为 Session 文件 Ops，
需要独立 name scratch；其命名/批量管理语义不下沉到共享字节接口。

装配顺序：

```text
平台 File Store init
 → session_jsonl_files_init(填 config.files/file_context)
 → 填齐 JSONL 工作数组与缓冲
 → session_jsonl_init
 → session_jsonl_bind
 → agent_set_session_storage
```

init 不打开会话文件；绑定不转移后端所有权。
完整版本化记录与失败边界见[Session 指南](../guides/session.md)。

## 通用字节文件接口

公共头 `agent_file_store.h`，绑定为 immutable ops pointer + context。

| 接口 | 行为 |
|---|---|
| agent_file_size | 查询长度 |
| agent_file_read | offset 读取，允许成功短读，0 为 EOF |
| agent_file_read_exact | 要求完整字节数，意外 EOF 为 IO |
| agent_file_read_all | 有界整文件读取，不保证并发写入下的快照 |
| agent_file_read_text | 整文读取并补 NUL，拒绝正文内 NUL；UTF-8 属于消费者 |
| agent_file_visit | 无序普通文件名枚举，不承诺排序 |
| agent_file_append / truncate / sync | 字节追加/截断/同步 |
| agent_file_remove | 删除；失败可能已删除 |
| agent_file_replace | 整文替换；published 表示发布事实 |

名字必须是单个安全组件，不含路径分隔符、NUL、点目录或内部 `.cagent-` 前缀。
缺回调表示能力不支持。根目录、状态、路径 scratch 与输入输出应满足不重叠约束。

`replace` 返回错误但 published=true 时不能假定旧文件仍在。
sync 不天然等于掉电安全；实际保证由后端和挂载文件系统决定。

## Markdown Memory

公共头 `agent_markdown_memory.h`。

`agent_markdown_memory_config_t` 逐项声明 soul/user/facts 的 Store、名字、正文上限，
及独立 notes Store。上限 0 禁用对应文档。
notes 使用合法 `YYYY-MM-DD` 逻辑标识，不扫描所有日期。

`agent_markdown_memory_init()` 校验映射并复制配置；
`agent_markdown_memory_bind()` 导出 Memory Ops，
再由 `agent_set_memory()` 绑定 Agent。无缓存、无隐含文件描述符和 Heap。

## POSIX 与平台 Store

`agent_posix_file_store_init()` 接收已存在的绝对受信任目录、独立 path buffer、
read_only 和 sync_directory。它不挂载、格式化或创建应用目录。

ESP-IDF、OpenVela、RT-Thread 预制入口复用该字节后端；
平台文件系统必须满足所选能力，目录同步不支持时不能伪装成功。
详见[平台指南](../platforms/index.md)。
