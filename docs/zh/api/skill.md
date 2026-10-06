# Skill 接口

状态：2026-10-06 已实现注册、注销和有界全文投影；Run 尚未接入。
公共头为 `agent/skill.h`。文件加载器、按需 Skill 和 `read_skill` Tool 不在当前实现内。

## 定义与注册

```c
#include <agent/skill.h>

agent_error_t register_home_skill(agent_t* agent)
{
    const agent_skill_t skill = {
        .name = AGENT_SV_LITERAL("home-control"),
        .description = AGENT_SV_LITERAL("Home device instructions"),
        .content = AGENT_SV_LITERAL("Check device state before changing settings."),
        .priority = 10,
        .required = true
    };
    return agent_register_skill(agent, &skill);
}
```

注册复制结构体，字符串仍由应用持有。示例中的字符串字面量可长期借用；若文本来自加载器，
加载缓冲必须保持有效且不可修改，直到注销或 Agent 销毁。不得借用 Core workspace 中的文本。
Skill 属于应用授权的指令材料，注册成功不产生任何 Tool 执行权限。

| 字段 | 作用与限制 |
|---|---|
| `name` | 唯一 ASCII 标识，仅字母、数字、下划线和连字符；受 `AGENT_MAX_NAME_BYTES` 限制 |
| `description` | 可选说明，受 `AGENT_MAX_DESCRIPTION_BYTES` 限制；全文投影不自动输出此字段 |
| `content` | UTF-8 指令文本，可空；不允许嵌入 NUL，受 `AGENT_MAX_CONTEXT_BYTES` 限制 |
| `priority` | 数值越大越靠前；同优先级保持注册顺序 |
| `required` | 必需内容保留空间；可选内容只整段纳入或跳过 |

## 生命周期与错误

`agent_register_skill()`、`agent_unregister_skill()` 只允许在 CONFIGURING/READY 调用。
ACTIVE 或回调重入返回 `AGENT_ERROR_BUSY`。注销释放库中的引用，不释放应用文本。

| 情况 | 返回值 |
|---|---|
| 成功 | `AGENT_OK` |
| 空参数、非法名字、嵌入 NUL、借用 workspace | `AGENT_ERROR_INVALID` |
| 非法 UTF-8 | `AGENT_ERROR_PARSE` |
| 单字段超过构建上限 | `AGENT_ERROR_LIMIT` |
| 同名重复注册 | `AGENT_ERROR_EXISTS` |
| 固定槽位耗尽 | `AGENT_ERROR_CAPACITY` |
| 注销不存在的名字 | `AGENT_ERROR_NOT_FOUND` |
| `AGENT_MAX_SKILLS=0` | `AGENT_ERROR_NOT_SUPPORTED` |

## 投影

应用不调用私有投影接口。Context 从已排序注册表取得全文，纳入统一预算；相邻指令段之间
使用两个换行符。未选择的可选 Skill 进入 Context 的私有跳过报告。

独立 `agent_skill_project()` 用于有界 sink 投影，并预留后续必需 Skill 的文本和分隔符空间。
必需文本溢出在输出前报告 `AGENT_ERROR_CONTEXT_OVERFLOW`；sink 中途失败可能留下部分输出，
因此拥有目标缓冲的调用者负责回滚。完整 Context 构建通过 scratch checkpoint 保证失败不发布模型输入。

容量来自构建宏，不增加运行期槽位分配。`AGENT_MAX_SKILLS=0` 时不分配 Skill 注册表。
参见 [Skill 架构](../arch/skill.md)、[Context 接口](context.md)和[开发日志](../development/skill.md)。
