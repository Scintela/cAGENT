# Configuration and Capacities

Public header: `agent/config.h`; default-limit macros live in `agent/types.h`.
Use one shared configuration for application and library; see the
[build guide](../getting-started/build.md).

## Instance Configuration and Storage

`agent_config_default()` returns zero-initialized configuration with default
limits. Supply Runtime monotonic now_ms explicitly; no platform is auto-selected.

| Type/field | Meaning |
|---|---|
| agent_config_t.system_prompt | Borrowed fixed instructions |
| agent_config_t.limits | Default instance behavior budgets |
| agent_config_t.runtime | Copied service table with borrowed contexts |
| agent_workspace_t | Conservatively aligned private Core byte storage |
| agent_model_workspace_t | Separate Model wrapper storage, excluding Provider buffers |

Workspace is an init argument, not part of config. No public planning or runtime
capacity-growth API exists.

## Physical Capacity Macros

CMake/Kconfig use corresponding `CONFIG_AGENT_*` names.

| Macro | Default | Purpose |
|---|---:|---|
| AGENT_MAX_TOOLS | 12 | Tool slots; zero trims the registry |
| AGENT_MAX_SKILLS | 8 | Skill slots; zero trims the registry |
| AGENT_MAX_CONTEXTS | 8 | Shared dynamic Context/Memory-selection slots |
| AGENT_SCRATCH_BYTES | 12288 | Shared temporary Turn region |
| AGENT_MAX_PROJECTED_MESSAGES | 32 | Model message descriptors |
| AGENT_MAX_INPUT_BYTES | 1024 | Applicable user/reference input bodies |
| AGENT_MAX_CONTEXT_BYTES | 4096 | Context text budget |
| AGENT_MAX_SCHEMA_BYTES | 2048 | One Tool Schema |
| AGENT_MAX_ARGUMENTS_BYTES | 1024 | One Tool argument object |
| AGENT_MAX_TOOL_OUTPUT_BYTES | 1024 | One Tool result |
| AGENT_MAX_MODEL_OUTPUT_BYTES | 2048 | Model text output |
| AGENT_MAX_MODEL_TOOL_CALLS | 4 | Complete calls in one model response |
| AGENT_MAX_NAME_BYTES | 64 | Name body |
| AGENT_MAX_DESCRIPTION_BYTES | 256 | Description body |
| AGENT_MAX_IDENTIFIER_BYTES | 64 | Session/Trace/Call IDs |
| AGENT_MAX_JSON_DEPTH | 16 | Core Tool JSON depth, valid 1..32 |
| AGENT_CORE_WORKSPACE_BYTES | 32768 | Total Core Workspace, compile-checked |
| AGENT_MODEL_WORKSPACE_BYTES | 128 | Model wrapper Workspace |

`AGENT_MAX_SESSIONS=4`, `AGENT_SESSION_EVENT_CAPACITY=96` and
`AGENT_SESSION_PAYLOAD_BYTES=8192` remain configuration entries but are not used
to allocate history pools in current Core/Storage. RAM/JSONL capacities instead
come from caller arrays/buffers in their respective configs.

Generated workspace size is a published configuration value, not an automatic
formula over every macro. CMake generates the header; internal assertions check
the actual layout fits. Recheck the total after increasing slots or scratch.

## Default Behavior Macros

| Macro | Default |
|---|---:|
| AGENT_DEFAULT_MAX_STEPS | 8 |
| AGENT_DEFAULT_TIMEOUT_MS | 30000 |
| AGENT_DEFAULT_MODEL_TIMEOUT_MS | 15000 |
| AGENT_DEFAULT_TOOL_TIMEOUT_MS | 3000 |
| AGENT_DEFAULT_MAX_TOOL_CALLS | 4 |
| AGENT_DEFAULT_MAX_OUTPUT_TOKENS | 512 |
| AGENT_DEFAULT_MAX_HISTORY_TURNS | 0 |

These initialize limits; idle set_limits or complete request overrides can change
them. They neither enlarge physical storage nor enforce immutable product safety policy.
