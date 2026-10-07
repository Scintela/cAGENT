# RT-Thread Port

可选的平台包，应用显式构建、初始化并注入，Core 不包含 RT-Thread SDK。

| 子包 | 接口 | 依赖与边界 |
|------|------|------------|
| [runtime](runtime/README.md) | `agent_port_rtthread_runtime_init/deinit` | RT-Thread 5.1+，32-bit tick、静态 timer、spinlock；可选 `rt_malloc/rt_free`。 |
| [transport](transport/README.md) | `agent_port_rtthread_transport_init` | WebClient 2.3 API；同步非空 POST，HTTPS 信任策略由应用配置，SDK 使用 heap。 |
| [storage](storage/README.md) | `agent_port_rtthread_file_store_init` | 已挂载的 DFS/POSIX 目录；复用共享字节后端，不依赖 JSONL。 |

```text
ports/rtthread/
  Kconfig / SConscript / CMakeLists.txt
  runtime/   include/agent_rtthread_runtime.h     src/runtime.c
  transport/ include/agent_rtthread_transport.h   src/transport.c
  storage/   include/agent_rtthread_file_store.h  src/file_store.c
```

## SCons / Kconfig

在 BSP Kconfig 分别引入仓库根 `Kconfig` 与本目录 `Kconfig`，路径按应用目录确定。
选择容量 Profile，再按需启用 `AGENT_PORT_RTTHREAD_RUNTIME`、
`AGENT_PORT_RTTHREAD_TRANSPORT`、`AGENT_PORT_RTTHREAD_FILE_STORE`。

Transport 依赖 `PKG_USING_WEBCLIENT`、`RT_USING_HEAP`，禁用可能打印凭证的
`WEBCLIENT_DEBUG`。Storage 依赖 `RT_USING_DFS`、`RT_USING_POSIX_FS`，选中后
引入通用 File Store 与 POSIX 后端；Markdown Memory、JSONL、OpenAI 单独选择。

应用 `SConscript` 显式调用本目录 `SConscript` 并合并返回的 group。它编译 Core
与选中的交付物，**不要再单独加入另一份 Core 源文件**。RT-Thread `DefineGroup`
的公共 `CPPDEFINES` 发布 `AGENT_BUILD_CONFIG_HEADER=\"rtconfig.h\"`，让应用、
Core、Provider 使用同一份 `rtconfig.h` 容量定义。不要混用默认容量与自定义布局。

## CMake

Kconfig 不是强制依赖。已有 RT-Thread CMake 工程可创建 SDK target，携带
BSP/RT-Thread/WebClient include、编译设置和链接依赖，再使用：

```cmake
# my_rtthread_sdk 由应用创建，必须含真实 BSP 的 rtconfig.h 和 SDK 依赖。
set(AGENT_BUILD_FILE_STORE ON CACHE BOOL "" FORCE)
set(AGENT_BUILD_POSIX_FILE_STORE ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_RUNTIME ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_TRANSPORT ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_FILE_STORE ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_SDK_TARGET my_rtthread_sdk CACHE STRING "" FORCE)
add_subdirectory(path/to/cAgentV2 cagent)
add_subdirectory(path/to/cAgentV2/ports/rtthread cagent_rtthread)
target_link_libraries(app PRIVATE cagent::rtthread_runtime
    cagent::rtthread_transport cagent::rtthread_file_store)
```

CMake 使用 Core 生成的 `agent_build_config.h`，所有消费者通过 target 继承相同
容量；不会从 `rtconfig.h` 自动翻译容量。构建选项独立且默认关闭。BSP 必须提供
实际实现与库。Core 最低 C99，SDK 可以要求更高标准。

无符号链接的受信任 DFS 挂载可显式选 `AGENT_PORT_RTTHREAD_NO_SYMLINKS`
（SCons）或 `AGENT_RTTHREAD_NO_SYMLINKS=ON`（CMake）。它改变共享 POSIX
后端的构建配置，不是每个 Store 的独立开关，不可用于支持 symlink 的挂载。

## 验证

```sh
bash tests/ports/rtthread/compile.sh
SANITIZE=1 bash tests/ports/rtthread/compile.sh
bash tests/ports/rtthread/build.sh
# 可选：以真实 SCons 引擎编译 Host 假 SDK，无需完整 BSP。
SCONS=/path/to/scons bash tests/ports/rtthread/build.sh
# 可选：该 Python 环境需要安装 Kconfiglib。
KCONFIG_PYTHON=/path/to/python bash tests/ports/rtthread/build.sh
# 可选：本地真实 SDK 头文件，不自动下载。
RTTHREAD_SDK_ROOT=/path/to/rt-thread WEBCLIENT_SDK_ROOT=/path/to/webclient \
  bash tests/ports/rtthread/upstream_headers.sh
```

Host 假 SDK 验证逻辑、清理和构建选项；真实头检查只验证最小 UP 配置接口编译，
不是 BSP 固件构建，更不是 HTTPS 或掉电验收。见
[RT-Thread 开发记录](../../docs/zh/development/rtthread.md)。
