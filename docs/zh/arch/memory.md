# 内存模型

应用选择物理预算，库在既定域内分配。不是整个工程只有一块 Arena，
也不是所有数据都在 Core 中保存。

```text
Application
├── Core Workspace
│   ├── 长期区：agent_t、注册表、绑定与统计
│   └── Turn scratch：快照、消息、视图、模型输出与工具结果
├── Model wrapper Workspace
├── Model Provider 工作区：request / response / decode / token
├── Session Storage 工作区：RAM payload 或 JSONL 编解码缓冲
├── File Store 路径 scratch
├── HTTP/TLS SDK 状态与缓冲
└── 应用任务栈及设备资源
```

## 四类容量概念

| 类别 | 示例 | 是否直接预留正文空间 |
|---|---|---|
| Persistent capacity | Tool/Skill/Context 槽位 | 注册表元数据预留；正文通常借用 |
| Turn scratch budget | AGENT_SCRATCH_BYTES | 一块每轮复用的临时空间 |
| Per-object admission cap | Schema/arguments/输出上限 | 准入上限，不等于每对象独立数组 |
| Workspace total | AGENT_CORE_WORKSPACE_BYTES | 调用方总存储，内部布局编译校验 |

Core scratch 按存活期分配和 LIFO 回收，并非通用碎片化 Heap。
一次 Turn 的输出/事实仍活着时不能复用其字节；可释放的模型投影才可回收。
“Context 暂时变大”只能利用剩余 scratch，不能突破单项上限或占用长期区。

## 三个容易混淆的概念

- 零 Heap：不调用动态 allocator；固定 Workspace 内仍会分配、复制与回收。
- 零拷贝视图：读取借用数据；不能延长原缓冲寿命。
- PSRAM：应用选择存储位置；Core 不自动判断 SRAM/PSRAM 或 DMA/TLS 能力。

应用可以把适合的 Provider/历史缓冲放到 PSRAM，但要检查平台对齐、访问与 SDK 限制。
不要把大 Workspace 放在默认小栈上。

## 调整方法

先测 summary.scratch_peak_bytes，再定位 Provider/Storage/SDK 的独立峰值。
增大 Core 总 Workspace 不会自动增大 scratch 或 OpenAI response_buffer；
增大单对象 cap 也不会自动提供足够总预算。

保守地测量完整历史窗口、最大 Tool Schema、迭代次数、失败路径和 JSON 深度栈。
当前没有全栈零 Heap 保证或统一 Heap 高水位统计。
配置默认值见[配置参考](../api/config.md)。
