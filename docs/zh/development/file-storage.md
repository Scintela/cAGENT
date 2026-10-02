# 文件存储与读取实施记录

日期：2026-10-02。依据 [ADR 0031](../adr/0031-prefabricated-platform-file-storage.md)。

## 边界与交付物

```text
providers/storage/files/
  include/agent_file_store.h        字节级 ops + 有界读取辅助
  src/file_store.c                  校验、分发、精确读与文本读取
providers/storage/jsonl/
  include/agent_session_jsonl_files.h
  src/file_store_bind.c             Session ID 编码、统计和清理
ports/posix/storage/
  include/agent_posix_file_store.h
  src/file_store.c                  物理文件 I/O
ports/{espidf,openvela}/storage/    平台装配入口，复用兼容 POSIX I/O
```

Core 仍只绑定 Session Storage；不直接持有文件实例。可选通用交付物集中在
`providers/`。文件读写可用于 USER、MEMORY、SOUL 和每日笔记，但这不等于已经
实现 Memory 检索、写入授权、Skill front-matter 解析或自动 Context 投影。

## API 与结构体

| 类型 / API | 职责 |
|---|---|
| `agent_file_store_ops_t` | size/read/visit/append/truncate/sync/remove/replace；缺失操作返回 NOT_SUPPORTED |
| `agent_file_store_t` | 借用不可变 ops 和 context；不自动拥有或释放后端 |
| `agent_file_read_exact` | 拼接短读；意外 EOF 返回 IO |
| `agent_file_read_all` | 按容量和 max_bytes 完整读取；检查可观察的增长/缩短，不静默截断 |
| `agent_file_read_text` | 输出 NUL 结尾的文本 view；拒绝内嵌 NUL，格式和 UTF-8 校验属于消费者 |
| `agent_posix_file_store_config_t` | 已挂载绝对目录、调用方路径缓冲、只读和目录同步选项 |
| `agent_posix_file_store_t` | 根目录、缓冲和临时文件序号；不常驻文件描述符 |
| `agent_session_jsonl_files_t` | 复制文件实例绑定，持有独立的 Session 文件名工作缓冲 |
| `agent_session_jsonl_files_init` | 校验 JSONL 必需文件能力，配置现有八个回调 |

操作同步，同一实例必须串行。ops/context、目录字符串和缓冲在消费者结束前
有效，配置对象只需在 init 时有效；回调中的文件名只在回调期间有效。
输入、输出、结果对象与后端工作缓冲不能重叠。SDK/文件系统内部可以分配资源，
文件辅助本身不申请 heap；不扩大全栈零分配承诺。

## 两条数据流

```text
应用读取 USER.md
  -> agent_file_read_text(store, name, buffer, capacity, max_bytes)
  -> size -> read_exact -> EOF 探测 -> 再检查 size -> 文本 view
  -> 应用持有快照，未来在 turn 边界贡献到 Context

Core Session
  -> agent_session_storage_t
  -> JSONL Provider（turn 编码 / 回放 / 尾部修复）
  -> Session 文件适配（ID -> session-<hex>.jsonl）
  -> agent_file_store_t -> Platform Port -> 已挂载文件系统
```

目录枚举顺序不保证。领域组件负责过滤和排序；通用文件层不提供清空目录。
Session clear_all 反复枚举一个匹配文件，关闭枚举再删除，避免回调重入，也
避免无界名称数组；错误时可能已有部分文件被删除。

## 保存与失败

追加失败可留下部分前缀，JSONL 继续负责回滚和尾部修复。替换采用同目录独占
临时文件、完整写入、文件同步、关闭、rename，不先清空旧文件。

| 结果 | 含义 |
|---|---|
| `AGENT_OK` / `published=true` | 新版本已发布，所请求的同步调用成功；不等于真机掉电验证完成 |
| 错误 / `published=false` | 新版本未发布，清理临时文件；清理失败仍返回 IO |
| 错误 / `published=true` | 新版本已发布，但目录同步失败；不得当作“未写入”盲目重试 |

`sync_directory` 显式选择：启用时 init 验证目录 fsync；未启用不承诺目录项
耐久性。文件系统不支持所需操作时返回错误。替换需要路径缓冲容纳两条完整
路径。掉电残留 `.cagent-*` 临时文件不会作为用户文件枚举，维护由应用负责。

首版依赖应用控制且访问期间稳定的根目录与祖先目录。拒绝叶子符号链接，
但不宣称抵御不可信并发目录替换；这种威胁模型需要另一个安全后端。

## 验证

新增测试覆盖短读与 EOF、容量与文件变化、只读能力、独立用户根、符号链接、
完整替换、JSONL 回放/残尾修复及清理隔离。GNU linker 失败注入覆盖短写、EINTR、
写入失败、文件同步/关闭/rename 失败、发布后的目录同步失败及资源清理。

可选构建与实际链接测试：

```sh
bash tests/providers/files/compile.sh
bash tests/ports/posix/file_store_compile.sh
bash tests/ports/posix/file_store_faults.sh
bash tests/build/file_store/compile.sh
```

`tests/ports/openvela/file_store_cross_compile.sh` 接受 `NUTTX_ROOT`、目标 `CC`
及 `CFLAGS`，用于已配置 NuttX 头与目标工具链的编译检查。CMake 平台装配测试使用
Host 代替 SDK 构建入口，分别验证纯文件组合、JSONL 组合和公共源码不重复编译。

Host 契约验证不等于 ESP-IDF/OpenVela 真机文件系统兼容性、重启或掉电验证。
快速开发阶段移除旧 Session 平台入口及其构建开关，不保留兼容别名；统一使用
文件接口及 JSONL bridge。Session 文件名和记录格式不变。原有边界用例迁移到
新链路测试，包括特殊 ID、空参数、路径/名称容量、多文件清理及 Markdown 隔离。
`agent_session_jsonl_file_ops_t` 继续作为领域契约支持应用直接注入回调。
