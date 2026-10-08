# Quickstart

Run one synchronous conversation on Linux/POSIX without an API key, network,
Kconfig or platform SDK. This example implements a deterministic application
Model to verify lifecycle and output delivery; it is not the official Mock Provider.

## 1. Prepare the Environment

Install Git, a C99 compiler and CMake 3.16 or newer, then clone the repository:

```sh
git clone https://github.com/Scintela/cAGENT.git
```

Create this application directory. `cAGENT/` is the cloned repository:

```text
my-agent/
├── cAGENT/
├── CMakeLists.txt
└── main.c
```

## 2. Add the Build

```cmake title="CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
project(my_agent C)
add_subdirectory(cAGENT)
add_executable(my_agent main.c)
target_link_libraries(my_agent PRIVATE cagent::core)
```

The target propagates include paths and generated capacity configuration. Do not
copy public headers manually.

## 3. Write the Application

```c title="main.c"
#define _POSIX_C_SOURCE 200809L
#include <agent.h>
#include <agent/model.h>
#include <stdio.h>
#include <time.h>

static agent_workspace_t core_workspace;
static agent_model_workspace_t model_workspace;

static uint64_t monotonic_ms(void* context)
{
    struct timespec now;
    (void)context;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        /* A production clock adapter must report a dependable monotonic clock. */
        return 0u;
    }
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static agent_error_t complete(void* context,
    const agent_model_request_t* request, const agent_model_sink_t* sink)
{
    static const char reply[] = "Hello from cAgentV2.";
    (void)context;
    (void)request;
    return sink->text(sink->context,
        agent_string_view(reply, sizeof(reply) - 1u));
}

int main(void)
{
    const agent_model_ops_t ops = { .complete = complete };
    const agent_request_t request = { .input = AGENT_SV_LITERAL("Hello") };
    agent_config_t config = agent_config_default();
    agent_t* agent = NULL;
    agent_model_t* model = NULL;
    char output[128];
    agent_response_t response = {
        .output = output,
        .output_size = sizeof(output)
    };
    agent_error_t status;

    config.runtime.now_ms = monotonic_ms;
    status = agent_init(&agent, &core_workspace, &config);
    if (status == AGENT_OK)
        status = agent_model_init(&model, &model_workspace, &ops, NULL);
    if (status == AGENT_OK)
        status = agent_set_model(agent, model);
    if (status == AGENT_OK)
        status = agent_start(agent);
    if (status == AGENT_OK) {
        status = agent_run(agent, &request, &response);
        printf("run=%s, delivery=%s, text=%.*s\n",
            agent_error_str(status), agent_error_str(response.delivery_status),
            (int)response.output_written, output);
    }
    /* Borrowed Model: destroy Agent first, then its wrapper. */
    agent_destroy(agent);
    agent_model_destroy(model);
    if (status != AGENT_OK)
        fprintf(stderr, "error=%s\n", agent_error_str(status));
    return status == AGENT_OK ? 0 : 1;
}
```

Both Workspaces use static storage rather than placing the default 32 KiB Core
Workspace on the task stack. The clock must remain available and monotonic; do
not substitute wall time that can change during synchronization.

## 4. Build and Run

From `my-agent/`:

```sh
cmake -S . -B build
cmake --build build
./build/my_agent
```

Successful output includes `Hello from cAgentV2.` with successful execution and
delivery statuses. Diagnostic spellings are defined by `agent_error_str()`.

## 5. Understand the Flow

1. `agent_init()` uses caller-owned Workspace and enters CONFIGURING.
2. `agent_model_init()` initializes the wrapper; `agent_set_model()` borrows it.
3. `agent_start()` enters READY without creating a Session or making a network request.
4. `agent_run()` starts a Turn, builds canonical input, calls the Model and delivers final text.
5. The Agent returns to READY. Destroying it does not free caller Workspace or the borrowed Model.

No Session Storage is bound, so this example does not retain history. Next,
[connect OpenAI](../guides/openai.md) or [register a Tool](../guides/tools.md).
Read [ownership and concurrency](../guides/lifecycle.md) before moving to a
multitasking device.
