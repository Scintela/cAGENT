# Build and Configuration

cAgentV2 is primarily built together with application source. C configuration
macros are the foundation; ordinary CMake generates a shared configuration
header. Kconfig is optional platform integration, not a mandatory Core dependency.

## Ordinary CMake

Build only Core:

```sh
cmake -S . -B build
cmake --build build
```

Use `add_subdirectory()` and link the required targets. Set options before adding
the repository, or pass them on the CMake command line. Do not combine libraries
built with different capacity configurations.

| Build option | Exported target | Dependency |
|---|---|---|
| Core, enabled by default | `cagent::core` | JSON reader when Tool capacity is nonzero |
| `AGENT_BUILD_JSON_CODEC` | `cagent::json_jsmn` | Private codec, not an application JSON API |
| `AGENT_BUILD_OPENAI_PROVIDER` | `cagent::provider_openai` | Full JSON codec |
| `AGENT_BUILD_SESSION_RAM` | `cagent::session_ram` | Core |
| `AGENT_BUILD_FILE_STORE` | `cagent::file_store` | Public byte-file contract |
| `AGENT_BUILD_SESSION_JSONL` | `cagent::session_jsonl` | Full JSON codec |
| JSONL + File Store | `cagent::session_jsonl_files` | Session filename bridge |
| `AGENT_BUILD_MARKDOWN_MEMORY` | `cagent::memory_markdown` | File Store |
| `AGENT_BUILD_POSIX_FILE_STORE` | `cagent::posix_file_store` | File Store |

`AGENT_BUILD_MOCK_PROVIDER` has a build entry but no delivered official Mock
implementation. The quickstart implements its own Model. Anthropic and STM32
directories also do not imply usable components.

For example, enable OpenAI and JSONL:

```sh
cmake -S . -B build \
  -DAGENT_BUILD_JSON_CODEC=ON \
  -DAGENT_BUILD_OPENAI_PROVIDER=ON \
  -DAGENT_BUILD_SESSION_JSONL=ON \
  -DAGENT_BUILD_FILE_STORE=ON \
  -DAGENT_BUILD_POSIX_FILE_STORE=ON
cmake --build build
```

The ordinary top-level build does not automatically enable platform Ports. See
the [platform guides](../platforms/index.md) for SDK dependencies and switches.

## Three Configuration Stages

| Stage | Configuration | Effect |
|---|---|---|
| Build | Source selection, physical capacities, default limits | ROM, layout and upper bounds |
| Initialization/idle | Runtime, Model, Storage, Memory, registrations, limits | Instance dependencies and behavior |
| Execution | Request, optional limits override, Session ID, user_data | Current Turn |

`agent_config_t` contains only the system prompt, default limits and Runtime.
Transport, Model, paths and Workspace are supplied through their own APIs.

## Consistent Capacities

Ordinary CMake accepts `CONFIG_AGENT_*` variables and generates
`agent_build_config.h`. Targets propagate `AGENT_BUILD_CONFIG_HEADER` so the
application and library see the same Workspace layout.

```sh
cmake -S . -B build \
  -DCONFIG_AGENT_MAX_TOOLS=16 \
  -DCONFIG_AGENT_DEFAULT_MAX_STEPS=6
```

Other build systems can supply a shared `AGENT_BUILD_CONFIG_HEADER` or consistent
project-wide definitions. Precedence is explicit `AGENT_*`, then `CONFIG_AGENT_*`
from the configuration header, then built-in defaults. Redefining capacities in
one application source file does not rebuild the library and breaks consistency.

## Trimming the Build

- No Tools: set `CONFIG_AGENT_MAX_TOOLS=0`; Core no longer needs JSON for Tools.
- No Skills or Context: set the corresponding slot count to zero; registration returns NOT_SUPPORTED.
- No persistence: omit Storage Providers and do not bind Storage.
- JSONL and OpenAI independently require the full codec; disabling Tools does not remove those dependencies.
- Markdown-only access does not require JSON.

See the [configuration reference](../api/config.md) for defaults and budgets.
