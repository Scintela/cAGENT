# 上下文与 Skill

Context 统一组装模型输入，但不拥有所有原始数据。它调用各领域接口，在 Core scratch 中
生成本次模型调用的规范投影；不直接读取文件，不生成 OpenAI JSON。

## 输入从哪里来

| 来源 | 注册或配置 | 读取时机 | 模型位置 |
|---|---|---|---|
| 固定系统指令 | `config.system_prompt` | 每个 Turn 准备 | system_prompt |
| Skill 全文 | `agent_register_skill()` | 每个 Turn 选择 | 系统指令 |
| SOUL | `agent_register_memory_context()` | 每个 Turn 快照 | 系统指令 |
| USER/MEMORY/每日笔记 | 同上 | 每个 Turn 快照 | 临时 USER 参考消息 |
| 动态状态 | `agent_register_context()` | 每次模型调用 | 显式 instructions 或 reference |
| Session | Storage + `max_history_turns` | 有界历史投影 | 规范消息序列 |
| Tool Schema | Tool 注册表 | 每次模型调用 | 独立 tools[] |

动态设备状态每次模型调用可以更新，Memory 在一次 Turn 中保持快照一致。
参考消息不追加到 Session；历史仍由 Session Storage 保存。

## 注册 Skill

```c
#include <agent/skill.h>

agent_error_t install_home_skill(agent_t* agent)
{
    const agent_skill_t skill = {
        .name = AGENT_SV_LITERAL("home_safety"),
        .description = AGENT_SV_LITERAL("Home operation rules"),
        .content = AGENT_SV_LITERAL("Read device state before changing it."),
        .priority = 10,
        .required = true
    };
    return agent_register_skill(agent, &skill);
}
```

注册浅拷贝描述符，字面量寿命足够；若从文件加载内容，应用必须保留内容直到注销。
当前只有全文注册与投影，没有官方目录加载器或按需 `read_skill` 工具。
Skill 不执行代码、不赋予 Tool 权限。

## 预算与放置

- 必需贡献预留声明上界；装不下则失败，不静默截断。
- 可选贡献以完整块为单位跳过，不产生半段指令。
- 优先级越高越先呈现，同优先级保留注册顺序；收集时必需项优先。
- 动态贡献与 Memory 选择共用 Context 槽位和名字空间。
- 可信规则选择 INSTRUCTIONS；传感器、用户文档和外部资料通常选择 REFERENCE。
- USER 角色不是安全隔离机制，仍需防范提示注入和工具权限越权。

History 数量、单消息大小、文本预算与 scratch 都会限制投影。
增加 `max_history_turns` 不会扩容 Workspace，也不代表整个历史文件进入 RAM。

具体示例见[Context 参考](../api/context.md)、[Skill 参考](../api/skill.md)。
