# Skill Interface

Registration, unregistration and bounded full-text projection are integrated
with Context and synchronous Run. Public header: `agent/skill.h`. On-demand
Skills, an official file loader and a `read_skill` Tool are not implemented.

## Definition and Registration

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

Registration copies the structure, not strings. Literals remain valid; loader
buffers must stay alive and immutable until unregistration/destruction. Do not
borrow Core Workspace text. Skills are application-authorized instructions,
not Tool execution permission.

| Field | Meaning and limits |
|---|---|
| name | Unique ASCII letters/digits/underscore/hyphen, bounded by AGENT_MAX_NAME_BYTES |
| description | Optional, bounded by AGENT_MAX_DESCRIPTION_BYTES; not automatically projected |
| content | UTF-8 instructions, may be empty; no embedded NUL; bounded by AGENT_MAX_CONTEXT_BYTES |
| priority | Higher first; equal priority preserves registration order |
| required | Reserve required content; include/skip optional content whole |

## Lifecycle and Errors

Register/unregister only in CONFIGURING/READY. ACTIVE or callback reentry returns
BUSY. Unregistration releases references, not application strings.

| Condition | Status |
|---|---|
| Success | AGENT_OK |
| Invalid parameters/name, embedded NUL, borrowed Workspace | INVALID |
| Invalid UTF-8 | PARSE |
| Field exceeds a build cap | LIMIT |
| Duplicate name | EXISTS |
| Slots exhausted | CAPACITY |
| Absent name on unregistration | NOT_FOUND |
| AGENT_MAX_SKILLS=0 | NOT_SUPPORTED |

## Projection

The application does not call private projection APIs. Context consumes sorted
full text under the shared budget, separated by two newlines. Skipped optional
Skills enter its private report.

Private `agent_skill_project()` reserves later required text/separators before
bounded sink projection. Required overflow fails before output with
CONTEXT_OVERFLOW. Sink failure can leave partial output, so its caller rolls
back. Full Context uses scratch checkpoints and publishes nothing on failure.

Build macros fix slots, with no runtime allocation; zero disables the registry.
See [Skill architecture](../arch/skill.md), [Context](context.md) and the
[Chinese development record](development/skill.md).
