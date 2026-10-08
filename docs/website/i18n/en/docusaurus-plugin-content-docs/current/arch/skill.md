# Skill Architecture

Skills are trusted application instructions. Core manages admission, fixed
slots, stable sorting and borrowed lifetimes. File reads, Markdown/front matter
and directory discovery belong to an optional loader.

```text
Application static text / optional loader
 -> agent_skill_t -> agent_register_skill()
 -> Skill registry in Workspace
 -> Context selection and budgets
 -> model_request.system_prompt
 -> Provider protocol encoding
```

## Files and State

| File | Responsibility |
|---|---|
| include/agent/skill.h | Borrowed definitions and register/unregister APIs |
| src/skill/skill_internal.h | Private fixed layout, size and projection entry |
| src/skill/skill_registry.c | Admission, stable insertion, removal, full-text projection |
| src/core/text_internal.h | JSON-independent UTF-8/identifier validation |
| src/core/lifecycle.c | Aligned registry layout and Workspace compile checks |

The registry shallow-copies definitions, not text. It sorts in fixed arrays with
no heap/index tree. Removal compacts entries while preserving stable ordering.

## Budgets and Ownership

Standalone Skill projection reserves required text/separators, then chooses
optional text. Joint Context also reserves fixed instructions, Memory and
dynamic sources using the same sorted metadata. Neither path truncates Skills.
Priority controls presentation; required controls space guarantees.

Registry state is persistent; selections belong to a Turn. Borrowed text lives
until unregistration, spanning multiple model calls. Context does not acquire
loader-buffer ownership.

## Scope and Verification

Full-text contribution is implemented. No directory auto-enable, on-demand
summary, Skill executor, implicit Tool registration or permission inheritance.
ADR 0028/0029 alternatives remain unimplemented.

Tests: `tests/skill/compile.sh` and joint `tests/context/compile.sh`.
See [Skill APIs](../api/skill.md).
