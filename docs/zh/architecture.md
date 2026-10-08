# 总体架构

cAgentV2 将执行机制、领域协议和平台实现分开。应用决定依赖、预算与信任，
Core 决定一次 Turn 内的执行顺序和不变量。

## 分层

```text
Application
  配置 / Workspace / 身份权限 / 任务队列 / 挂载与联网
  │
  ├── Core (src/)
  │   lifecycle / arena / event / cancel
  │   run → context → model contract
  │       → tool → policy
  │       → session contract
  │       → memory contract
  │   skill 与 context 注册表
  │
  ├── 可选 Provider (providers/)
  │   model/openai      规范请求 ↔ Chat Completions
  │   storage/ram       易失 Session
  │   storage/jsonl     Turn ↔ JSONL
  │   storage/files     字节文件契约与有界帮助函数
  │   memory/markdown   逻辑文档 ↔ Markdown 文件
  │
  ├── 私有 Codec (codecs/json/)
  │   严格 JSON reader / 有界 writer / 内嵌 jsmn
  │
  └── 平台 Port (ports/)
      espidf / openvela / rtthread
        runtime / transport / storage
      posix/storage     可复用物理字节 I/O
```

Model 的 vendor wire JSON 不进入 Core，文件路径不进入 Session/Memory 领域接口，
平台 SDK 不进入通用公共头。可选包的启用不隐含资源初始化。

## 执行数据流

```text
agent_run(request)
 → 准入、有效 limits 与 deadline
 → Session 当前 Turn / Storage 事务
 → Context 准备 Skill 与 Memory 快照
 → Context 投影 system_prompt / messages[] / tools[]
 → Model.complete
      → OpenAI JSON → HTTP Transport → JSON 响应 → Model sink
 → Core 复制模型结果
      ├── final：结束
      └── tool_calls：参数 / Policy / handler / 配对结果 → 再次投影
 → Session finish
 → 文本交付、统计、TURN_END
 → scratch rewind，回到 READY
```

这是已经实现的同步主链路。公开接口没有 begin/step/resume/end、
Plugin mount/unmount、后台 Agent 线程或自动记忆提取。

## 领域职责

| 领域 | 拥有什么 | 不做什么 |
|---|---|---|
| Run | 本轮流程、预算、取消、终态 | 产品调度、UI、网络重连 |
| Context | 本次模型输入的有界投影 | 持久化、厂商序列化 |
| Tool/Policy | 注册、验证、授权与同步执行 | 自动设备回滚或通用沙箱 |
| Skill | 可信全文指令的注册与选择 | 文件扫描、代码执行 |
| Session | 当前 Turn 配对与历史组投影 | 强制 JSONL/NVS 存储 |
| Memory | 逻辑长期文档管理与变化语义 | 自动摘要、向量检索 |
| Provider | 协议、格式与独立工作缓冲 | 平台 HTTP/TLS 实现 |
| Port | SDK 服务、网络和字节 I/O | JSONL/Markdown 领域规则 |

## 生命周期与内存

实例为 CONFIGURING → READY → ACTIVE → READY。
Core Workspace 存长期状态及注册表，外加复用 Turn scratch；
Model wrapper、Provider、Storage、HTTP/TLS 和栈独立预算。

Core 固定存储初始化与执行路径不申请 Heap；可选 create 包装和平台 SDK 可使用 allocator。
只读视图消除不必要复制，但 Session 选择、回调输出和长期快照仍有必要复制，
不能将零 Heap 等同于零拷贝。

进一步阅读[模块地图](arch/module-map.md)、[运行架构](arch/run.md)、
[内存模型](arch/memory.md)、[Tool](arch/tool.md)、[Skill](arch/skill.md)与[Context](arch/context.md)。
设计动机保留在[ADR](adr/index.md)，其中历史候选接口不代表当前实现。
