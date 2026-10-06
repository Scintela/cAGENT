# Context 开发日志

日期：2026-10-06。前置 Skill 机制先实现并提交，再完成 Context 联合投影。

## 从骨架到实现

原 `agent_context_build()` 仅描述动态文本 sink，不能代表完整模型输入。
本次私有接口改为 `agent_context_prepare()`、`agent_context_project()`、`agent_context_release()`，
覆盖 turn 快照、单次模型投影和后进先出回收。没有新增面向应用的通用构建器。

公共接口保留动态注册/注销，新增 `agent_register_memory_context()` 选择逻辑文档。
贡献描述符增加 `max_bytes` 和 `placement`；处于开发期，直接完善契约，不保留旧私有入口。
结构体、公共示例及返回值见 [API](../api/context.md)，数据流与寿命见[架构](../arch/context.md)。

## 跨模块修改

- Core 初始化 Context 注册表，workspace 编译期检查同时覆盖 Tool、Skill、Context 和对齐。
- Context 与 Memory 选择共用固定槽位，Kconfig 允许 Skill/Context 配置为零。
- Session 新增调用者提供消息描述符的私有投影入口，保留独立投影包装函数供原有使用者调用。
- Session 历史回调加入轮询，投影前验证 Agent arena、session id 和当前输入的一致性。
- Tool 投影复用现有可见性规则，不提前调用有副作用的授权或执行回调。
- 所有通过 `lifecycle.c` 的直接编译测试补齐注册表链接依赖，CMake 加入新的投影文件。

## 实现中明确的取舍

必需来源按声明上界预留，实际内容更短不作为跳过预检的理由。可选 Memory 会为后续调用级投影
保守保留空间，因此可能在理论上“还能挤入更多文本”时选择跳过，行为是确定且可诊断的。
动态回调执行时必需优先，最终输出仍保持同类优先级顺序，应用不能依赖回调执行副作用。

SOUL/Skill 属于应用授权指令；USER/FACTS/NOTE 使用临时参考消息，不污染持久化 Session。
没有把角色选择宣传成提示注入防护，也没有把 byte budget 宣传成 token budget。
未使用普通 Memory 读写 API 在 Context 回调中重入 Agent。

## 验证记录

```sh
bash tests/skill/compile.sh
bash tests/context/compile.sh
bash tests/build/context/compile.sh
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' bash tests/context/compile.sh
```

以上均通过；同时通过公共头、Core、Tool、Session/RAM/JSONL、Memory/Markdown、OpenAI、
JSON、Transport、平台 Port、File Store，以及 Tool/File Store 构建矩阵回归。
模拟 SDK/Host 测试不代替真实 ESP-IDF/OpenVela 的栈、峰值内存及存储时延验证。

## Run 接入注意事项

当前 `agent_run()` 仍返回 NOT_SUPPORTED。后续 Run 必须计算整体 deadline、发布取消 token、
预留响应区域、驱动 Context/Model/Tool、维护 Session 事务及事件，不能只调用一次 projection
就声称 ReAct 已完成。Model 回调只借用投影视图；release 后不得保留这些指针。
参考资料和动态设备状态不作为用户发言写入持久化 Session。
