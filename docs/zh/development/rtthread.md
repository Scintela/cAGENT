# RT-Thread Port 开发记录

日期：2026-10-07。范围：Runtime、WebClient POST Transport、DFS/POSIX 文件绑定
和可裁剪构建。不修改 Core 公共 Ops，不向通用 API 引入平台类型。

## 结构与接口

| 模块 | 结构 / API | 生命周期 |
|------|------------|----------|
| Runtime | `agent_rtthread_runtime_t`；`agent_port_rtthread_runtime_init/deinit` | caller-owned tick 扩展、两个短锁、静态周期 timer；销毁消费者后 detach。 |
| HTTP | `agent_port_rtthread_transport_config_t`、`agent_port_rtthread_transport_t`；`agent_port_rtthread_transport_init` | config 复制，时钟 context/buffers 借用；request 创建并关闭 SDK session。 |
| Files | `agent_rtthread_file_store_config_t`、`agent_rtthread_file_store_t`；`agent_port_rtthread_file_store_init` | 复用 POSIX 配置与状态，无长期文件句柄。 |
| 构建 | `Kconfig`、`SConscript`、`CMakeLists.txt` | 三类 Adapter 独立选择；SCons 用 rtconfig.h，CMake 用 Core 生成配置。 |

应用先 init Runtime，注入 Core/Model；init HTTP binding 后传入 OpenAI Provider
的 transport。文件 Store 独立存在，经 JSONL/Markdown 绑定为领域 Ops。Agent
destroy 不销毁借用资源。

```text
Application 选择容量、SDK 依赖、buffer、可信挂载、TLS 策略
  ├─ Runtime builder -> Core / OpenAI 使用同一单调时基
  ├─ HTTP builder -> OpenAI -> JSON codec -> HTTP Ops -> WebClient -> SAL/TLS/socket
  └─ File builder -> byte-file Ops <- JSONL Session / Markdown Memory

agent_run -> Context -> Model -> HTTP -> 规范响应 -> Tool -> Session
           文件格式不进入 Core，SDK 状态不进入 Core workspace
```

## 实现取舍

1. 不创建工作线程；静态 sampler 观察 tick 回绕，扩展到 64-bit 毫秒，转换溢出
   饱和处理。取消锁不包裹 I/O。
2. 非空 POST 支持现有非流式 Chat Completions；拒绝重定向，HTTP 状态透传，
   传输失败与 SDK timeout 使用扁平错误码。
3. 手动请求头及 SDK 拼接预预算；显式 Host，防止 SDK 把 value 中的 Host 字符串
   误认作字段。Accept/User-Agent 名字规范化。
4. Header/body 按预算准入；校验 framing、长度和编码；首个 sink 错误停止后续
   投递；成功/失败均关闭 session，不自动重试副作用不确定的请求。
5. HTTPS 默认失败关闭：TLS 缺失 NOT_SUPPORTED，无安全确认 AUTH。确认仅声明
   产品 TLS 配置已验证证书链和主机名，不等于本库进行了证书检查。
6. 文件复用共享 POSIX 后端；JSONL 记录与文件命名留在 Provider，Memory 保留独立
   领域契约，不复制 Session-specific 平台 API。

## 验证状态

| 检查 | 结果 / 边界 |
|------|-------------|
| Runtime Host 契约 | 100/1000/1024 Hz、heap on/off、idle 多次回绕、锁配对、启动回滚与 detach 错误。 |
| HTTP Host 契约 | TLS on/off、POST、拒绝 GET/空 POST/3xx、injection/alias、401/429/503 透传、已知/未知/chunk 正文、截断与容量、取消/deadline、sink 首错、关闭错误、重入。 |
| 文件实际 Host I/O | 默认与无 symlink 两种 Profile；有界文本、replace、只读、Markdown Memory 和 JSONL 桥。 |
| 构建 | CMake off/runtime/transport/files/all 目标及链接；SCons 源文件/依赖契约和真实引擎对 Host 假 SDK 的编译链接、统一 rtconfig.h 检查；Kconfiglib 验证依赖/安全默认值/Profile。未运行完整 BSP 构建。 |
| 编译/内存检查 | 严格 C99、三份 Port 头 C++11、ASan/UBSan、真实 SDK 头 UP 检查；无 firmware 链接。 |
| 板上验收 | 未完成：DNS/TLS/证书拒绝、真实 LLM、SDK heap 峰值、栈、SMP、tickless、DFS 挂载、重启/掉电。 |

真实头核对：RT-Thread v5.1.0（`4f7940167d172858e37373bc330c7093e8765d5b`）与
commit `c3e94f7b5fd533763a8e577fea0dfbc982eb20e5`、
WebClient commit `d30f86d39b2c36952c8ba8739504cebe4fc9aafb`（2.3.0 API）。
依赖未 vendored，测试不会联网下载。最低 RT-Thread 5.1+ 声明不等于逐 BSP 验证。
上游接口见 [RT-Thread](https://github.com/RT-Thread/rt-thread) 和
[WebClient](https://github.com/RT-Thread-packages/webclient)。

## 产品风险边界

- SDK 阻塞无统一中止句柄，本层取消/deadline 是协作检查，不能保证立即打断
  connect。硬截止需要换后端或产品提供验证过的网络中止机制。
- 上游 POST 忽略部分 write 错误，header/chunk 解析先于本层；有界后验检查不能
  修补所有上游问题。需审计固定版本，假 SDK 测试不是网络 parser 安全证明。
- SMP/all-soft deinit 前需保证 timer 派发静止，detach 不提供 join；tick 重设、
  未补偿睡眠的 BSP 不能保证墙钟 deadline。
- 文件共享 scratch 需串行；no-symlinks 开关需挂载符合承诺；rename/fsync 成功
  不能自动推导掉电原子性。
- HTTP/TLS/DFS 平台 heap 和缓存单独预算；Core no-heap 不扩展到 SDK。

接入步骤与各子包配置见仓库 `ports/rtthread/README.md` 及子包 README。
