# 快速开始

本教程在 Linux/POSIX Host 上运行一次同步对话，不需要 API Key、网络、Kconfig 或平台 SDK。
示例使用应用实现的确定性 Model，验证生命周期和输出交付；它不是仓库中的官方 Mock Provider。

## 1. 准备环境

安装 Git、C99 编译器和 CMake 3.16 或更新版本，克隆仓库：

```sh
git clone https://github.com/Scintela/cAGENT.git
```

创建以下应用目录。这里 `cAGENT/` 就是克隆的仓库：

```text
my-agent/
├── cAGENT/
├── CMakeLists.txt
└── main.c
```

## 2. 加入构建

```cmake title="CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
project(my_agent C)
add_subdirectory(cAGENT)
add_executable(my_agent main.c)
target_link_libraries(my_agent PRIVATE cagent::core)
```

链接目标会传播头文件路径和生成的容量配置，不需要手工复制公共头文件。

## 3. 编写完整应用

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

示例将两块 Workspace 放在静态存储中，避免将默认 32 KiB Core Workspace 放到任务栈上。
时钟必须单调且持续可用；不能用会被校时调整的墙上时间代替。

## 4. 编译并运行

在 `my-agent/` 中执行：

```sh
cmake -S . -B build
cmake --build build
./build/my_agent
```

运行成功时输出文本包含 `Hello from cAgentV2.`，执行和交付状态均为成功。
诊断名称的具体字符串以 `agent_error_str()` 为准。

## 5. 理解刚才发生的事情

1. `agent_init()` 使用调用方 Workspace，进入 CONFIGURING。
2. `agent_model_init()` 创建模型包装器；`agent_set_model()` 借用绑定。
3. `agent_start()` 检查生命周期状态并进入 READY，不创建 Session、不发起网络请求。
4. `agent_run()` 开始一个 Turn，组装规范输入，调用 Model，再交付最终文本。
5. 返回后 Agent 回到 READY；销毁 Agent 不会释放调用方 Workspace 或借用 Model。

尚未绑定 Session Storage，因此示例不保留历史。接下来可[连接 OpenAI](../guides/openai.md)
或[注册 Tool](../guides/tools.md)。先阅读[所有权与并发](../guides/lifecycle.md)，
再把示例迁移到多任务设备。
