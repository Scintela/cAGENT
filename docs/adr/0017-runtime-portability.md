# ADR 0017: Runtime 最小平台服务与 Port 可移植性

- 状态：已采纳（目录边界）；首批官方 Port 待定
- 日期：2026-09-27

## 背景

cAgentV2 要在 ESP-IDF、openvela/NuttX、RT-Thread、STM32 裸机或 FreeRTOS，以及 Host 上运行。
这些环境的时钟、临界区、日志和 heap API 不同，但 Core 的 workspace、turn 和 Model 抽象不应依赖
任一系统头文件。

当前 `agent_runtime_t` 包含 optional allocator、必需的 `now_ms`、optional `cancel_sync` 与 optional
log callback。`src/runtime/runtime.c` 只验证和转发它们。平台实现从 `src/runtime/` 移出，避免空骨架
同时暗示错误的生命周期或网络职责。本 ADR 固定 Port 的目录与构建边界，不在此时冻结某一平台 API。

## 不变量

无论采用何种方案，以下边界不变：

- Runtime 不包含 HTTP、DNS、socket、TLS、Wi-Fi、文件、Flash/NVS 或 JSON；它们属于 Transport、
  Model Provider、Session Storage Provider 或应用。
- Core 不创建线程、RTOS task、timer 或网络 worker；一个 Agent 由应用的一个驱动上下文串行调用。
- Core caller-workspace 主路径不得需要 allocator；allocator 仅服务 `agent_create()` 和明确选择的
  heap 型 Provider。
- Runtime callback 的 context 均为 borrowed，应用必须让其寿命覆盖 Agent。
- Runtime 不返回 errno、FreeRTOS、NuttX 或厂商错误码；平台 Adapter 在自己的边界记录原生诊断。

## Runtime 最小能力

| 能力 | 必需性 | 建议语义 | 不承担的职责 |
|---|---:|---|---|
| 单调时钟 `now_ms` | 必需 | 返回不倒退的 `uint64_t` 毫秒，用于 deadline、timeout 与统计。 | 墙上时间、时区、RTC 校时。 |
| allocator | 可选，成对 | `alloc/free` 同时存在或同时为空；返回的指针至少满足 `agent_workspace_t` 对齐。 | Core scratch 扩容、Provider 隐式 heap。 |
| cancel sync | 可选，成对 | 仅保护极短的 cancel/active-turn 状态访问。 | 包围 Model、Tool、Storage 或 log callback。 |
| log | 可选 | 同步消费 callback-lifetime 文本；不得重入 Agent。 | 可靠日志队列、网络上报、格式化 heap。 |

`now_ms` 的 64 位单调语义尤其重要。直接把 32 位 `HAL_GetTick()` 或 RTOS tick 强制转换为
`uint64_t` 不会消除回绕；Adapter 或应用必须在板级层扩展 tick，或直接使用可用的 64 位单调计时器。
Core 不自行猜测 tick 频率、回绕周期或睡眠补偿策略。

`cancel_sync` 不等于通用 mutex。首版 `agent_cancel()` 不是 ISR-safe；临界区可以是禁中断、
spinlock 或短 mutex，但其保持时间必须只覆盖一个标志读取/写入。若产品需要 ISR cancel，应由 ISR
通知驱动任务，或另行定义具有明确生命周期和原子性约束的 Port 扩展。

## 方案比较

### 方案 A：应用直接构造 `agent_runtime_t`

应用将本系统的时钟、日志和可选同步 callback 直接填入 `agent_config_t.runtime`。

| 优点 | 代价 |
|---|---|
| Core 最小，无任何平台依赖；裸机、私有 RTOS 和 Host 都可使用。 | 每个产品都要写少量 glue code。 |
| 板级时钟回绕、日志和 allocator 可按产品精确选择。 | 示例之间可能不一致，需要文档模板。 |
| 最符合 caller-owned workspace 与不隐式 heap 的目标。 | 对新用户不如一键 adapter 方便。 |

这是所有平台必须支持的基线方案。

### 方案 B：Core 内置条件编译 Port

在 `src/runtime/` 内使用 `#ifdef ESP_PLATFORM`、`__NuttX__`、`RT_USING_*` 等自动选择 Runtime。

| 优点 | 代价 |
|---|---|
| 上手快，应用代码短。 | Core 源码、头文件和构建矩阵绑定平台；未选平台也更难保持独立构建。 |
| 可复用常见时钟和日志。 | 同一系统的板级时钟、critical section 与 heap 策略并不统一。 |

不建议。平台探测应由构建系统和可选组件完成，而不是由 Core 自动决定。

### 方案 C：独立的官方 Port 包

Core 只保留 Runtime contract；每个平台以独立组件提供一个 runtime builder，例如：

```text
ports/
  host/
  espidf/
  openvela/
  rtthread/
  stm32/
```

每个包仅在选中时编译对应 SDK 依赖，可提供类型化配置和 caller-owned port state。

| 优点 | 代价 |
|---|---|
| Core 保持纯 C99；平台依赖、Kconfig/CMake 和样例跟随 Port 包。 | 需要维护多个独立构建目标。 |
| 可提供易用默认值，同时允许应用覆盖 allocator/log。 | Port API 的版本兼容也需维护。 |
| 适合开源库和用户自定义 Adapter 共存。 | 不应承诺每个 MCU 都有官方完整实现。 |

这是已采纳的目录边界；仍需决定首批官方支持的 Port。

### 方案 D：只提供 Host Runtime

仅为测试提供 POSIX/Host convenience，其余系统均由应用注入 callback。

该方案实现成本最低，适合作为早期阶段；但 ESP-IDF/openvela/RT-Thread 用户仍需重复编写常见
glue code。它可作为方案 A 的第一步，也可与方案 C 共存。

## Port API 候选形态

不要让 Port `init()` 创建 task、打开网络或隐式启用 heap。候选 API 有两种：

```c
/* 简单系统：Port 只填充默认时钟、短同步和日志。 */
agent_error_t agent_port_espidf_runtime_init(agent_runtime_t* out);
```

```c
/* 时钟扩展、logger 或 critical section 需要 caller state 的系统。 */
agent_error_t agent_port_stm32_runtime_init(
    agent_runtime_t* out,
    agent_port_stm32_runtime_state_t* state,
    const agent_port_stm32_runtime_config_t* config);
```

后者对 STM32/RT-Thread 更稳健，因为 tick 扩展和临界区通常是板级策略；前者可用于已有 64 位
单调时钟的 Host、ESP-IDF 或 OpenVela。两种 API 都应位于 Port 自己的公开 include 目录，而不是
`include/agent/runtime.h`。

## 建议目录与构建边界

```text
include/agent/runtime.h                 # Core contract only
src/runtime/runtime.c                   # validation and callback dispatch

ports/espidf/include/agent_espidf_runtime.h
ports/espidf/runtime/src/runtime.c
ports/openvela/include/agent_openvela_runtime.h
ports/openvela/runtime/src/runtime.c
ports/rtthread/include/agent_rtthread_runtime.h
ports/rtthread/runtime/src/runtime.c
ports/stm32/include/agent_stm32_runtime.h
ports/stm32/runtime/src/runtime.c
ports/host/include/agent_host_runtime.h
ports/host/runtime/src/runtime.c
```

`src/runtime/port_*.c` 过渡骨架已删除。Core 静态库不编译任何 SDK 头；Kconfig、CMake preset 或
包管理器负责选择 Port，`agent_runtime_t` 不应包含 platform enum。Port 在具有真实实现前只保留
目录说明，不应导出会返回占位错误的伪初始化 API。

## 平台实现建议

| 平台 | 时钟 | cancel 同步 | 默认日志 | 注意事项 |
|---|---|---|---|---|
| Host | `clock_gettime(CLOCK_MONOTONIC)` 或等价 | mutex/原子实现 | stderr | 仅用于测试/开发，不作为 MCU 内存结论。 |
| ESP-IDF | `esp_timer_get_time()/1000` | 目前未注入；后续可选短 FreeRTOS critical section | 目前未注入 | 已有最小 Runtime builder；allocator 默认留空，避免隐式 heap。 |
| openvela/NuttX | `CLOCK_MONOTONIC` | 平台短 critical/mutex | syslog 或应用 logger | 不把 socket/mbedTLS 放入 Runtime。 |
| RT-Thread | 经回绕扩展的 tick 或板级时钟 | 短 critical section | `rt_kprintf` adapter | tick 单位、回绕和低功耗唤醒需要产品确认。 |
| STM32 | 板级维护的 64 位 tick | 应用定义 | 可选串口 logger | 不应假设 HAL tick 天然满足 64 位单调契约。 |

## 待决项

1. 首版是否只交付方案 A + Host，还是同时交付 ESP-IDF 官方 Port。
2. allocator 是否仅以文档承诺 `agent_workspace_t` 对齐，还是未来增加带 alignment 参数的扩展 API。
3. 是否保留 `agent_sync_t` 名称，或改为更窄的 `agent_cancel_sync_t` 以防被误当全局锁。
4. 是否在首版完全禁止 ISR 入口，还是设计独立的 ISR 通知扩展。

## 验证要求

- Core 仅链接 `src/runtime/runtime.c` 时可在无 SDK 的 C99 Host 上构建；
- 每个 Port 的 `now_ms` 覆盖 32 位 tick 回绕或声明不支持该来源；
- allocator 成对缺失、sync 成对缺失、空 clock 均被 `agent_init()` 拒绝；
- caller-workspace 路径验证没有 allocator 调用；
- cancel sync 不包围任何 Provider/Tool/log callback；
- 未选择 Port 时不会把对应 RTOS、网络或日志依赖带入最终固件。
