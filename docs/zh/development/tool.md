# Tool 模块开发记录

日期：2026-10-06。代码提交：`965c93f`。
接口与字段见[Tool API](../api/tool.md)，数据流与模块责任见[Tool 架构](../arch/tool.md)。

## 本次完成

- 将 Tool/Policy 空骨架替换为正式的同步注册、投影、准入和安全执行机制。
- Core workspace 初始化时分配固定 registry，编译期检查容量与对齐。
- 注册值复制、字符串借用，空闲时维护槽位；枚举顺序稳定且禁止重入。
- schema/arguments 完整对象准入，拒绝解码重复键/NUL 键；不声称自动执行 JSON Schema。
- 默认拒绝、纯 validator、确认拒绝、调用预算检查和有效工具 deadline。
- handler 单次调用、sticky 有界输出，分别保留调用事实、handler 状态与输出状态。
- 输出空间和别名前检在设备操作前完成；不自动重试、不掩盖部分效果。
- Tool 零容量裁剪，reader/writer 构建分离，Provider 与 Core 共用单份 reader。

## 验证

```sh
bash tests/tool/compile.sh
CC=clang bash tests/tool/compile.sh
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' bash tests/tool/compile.sh
bash tests/build/tool/compile.sh
```

Tool 契约验证零/单/默认槽位；名称、字段/flag、UTF-8、schema、重复键、注册原子性、
满表错误、启停与隐藏、视图容量、查询、枚举顺序/首错、回调重入、默认拒绝和确认。
执行验证 malformed/超限参数、validator 错误、调用限额、预分配不足、取消、验证/Policy/
handler 超时、deadline 更紧者与溢出饱和、ignored sink error、非法输出、跨块 UTF-8、
空输出、精确上限、partial cancel、执行事实与别名保护，以及 scratch 中的目标缓冲。
另有 4096 次确定性畸形输入 smoke 检查，不等于完成了长期 fuzz 安全审计。

构建契约覆盖 Host/模拟 ESP-IDF、Tool 0/12 槽位、完整 codec/OpenAI 开关的八种组合，
实际链接并调用 Tool API/私有执行测试；启用 OpenAI 时也引用其 ops 以触发 Provider 链接。
检查 reader 编译一次，Tool-only 不引入 writer，零槽位无 Provider 时不编译 JSON。
不足的 workspace 构建必须失败，不等到启动期才发现槽位装不下。

同时回归公共头 C99/C++11、Core、JSON/jsmn 链接隔离、Session/RAM/JSONL、Memory/
Markdown、共享文件/失败注入、Transport、ESP-IDF/OpenVela 模拟适配和文件装配。
这些 Host 测试不能替代真实 SDK 固件构建、MCU 栈/时延/峰值 RAM 与设备效果测试。

## 下一阶段的接入责任

`agent_run()` 仍返回 NOT_SUPPORTED，不能以 Tool 私有契约通过代替端到端执行。
Run 接入时须完成以下职责，不复制另一套 Tool guard：

1. 复制 Model sink 的短寿命调用数据，并拒绝本轮重复 call ID。
2. 投影可见工具、按 turn 维护有效 limits、cancel 和整体 deadline。
3. 在执行前预留结果与 Session 失败收尾资源；传入剩余 handler 额度。
4. 依据 handler_called 扣减额度并保留失败/取消后的调用事实。
5. 形成完整 assistant Tool-call/result 配对；明确错误结果、终止与持久化行为。
6. 维护 Event 配对、统计和 scratch rewind，不让 handler/Policy自行写历史。

未纳入本次范围：内置业务 Tool、暂停确认、并行执行、自动重试、完整 JSON Schema
验证、共享 Provider 线程安全和自动模型摘要。阻塞 handler 必须由设备层自行支持
有界等待/协作取消，框架不能保证瞬时中止。
