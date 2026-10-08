# Context and Skills

Context assembles model input without owning every source. It calls domain
interfaces and builds a canonical projection in Core scratch. It neither reads
files directly nor generates OpenAI JSON.

## Input Sources

| Source | Registration/configuration | Read timing | Model placement |
|---|---|---|---|
| Fixed instructions | `config.system_prompt` | Turn preparation | system_prompt |
| Full-text Skill | `agent_register_skill()` | Turn selection | System instructions |
| SOUL | `agent_register_memory_context()` | Turn snapshot | System instructions |
| USER/MEMORY/daily note | Same | Turn snapshot | Temporary USER reference message |
| Dynamic state | `agent_register_context()` | Each model call | Explicit instructions or reference |
| Session | Storage + `max_history_turns` | Bounded history projection | Canonical message sequence |
| Tool Schema | Tool registry | Each model call | Separate tools[] |

Dynamic device state may change between model calls. Memory stays consistent
within a Turn. Reference messages are not appended to Session; Storage retains
the original history.

## Register a Skill

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

Registration shallow-copies the descriptor; literals have sufficient lifetime.
File-loaded content must remain alive until unregistration. Only full-text
registration/projection is implemented: no official directory loader or
on-demand `read_skill` Tool. Skills neither execute code nor authorize Tools.

## Budgets and Placement

- Required contributions reserve their declared bounds; insufficient room fails without truncation.
- Optional contributions are included or skipped as complete blocks, never partial instructions.
- Higher priority is presented first; equal priority preserves registration order. Required items are collected first.
- Dynamic sources and Memory selections share Context slots and a name namespace.
- Use INSTRUCTIONS for trusted rules and usually REFERENCE for sensor data, user documents and external material.
- USER role is not a security boundary; defend against prompt injection and privilege escalation.

Turn count, message size, text budget and scratch all limit history projection.
Increasing `max_history_turns` neither enlarges Workspace nor loads the whole
history file into RAM. See [Context](../api/context.md) and [Skill](../api/skill.md).
