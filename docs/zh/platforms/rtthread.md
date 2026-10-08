# RT-Thread 接入

当前 Runtime 针对 RT-Thread 5.1+，Transport 针对 WebClient 2.3 API。
Public Port 头允许包含 SDK 类型，这是具体平台包的边界，不影响通用 Core。

## SCons 与 Kconfig

BSP Kconfig 引入根 `Kconfig` 与 `ports/rtthread/Kconfig`。
应用 SConscript 调用 Port 的 SConscript 并合并返回 group；
它已编译 Core 和选中的 Provider，不要另外重复加入 Core 源文件。

启用所需 AGENT_PORT_RTTHREAD_RUNTIME、TRANSPORT、FILE_STORE。
Transport 需要 WebClient 与 RT_USING_HEAP，禁用会打印凭证的 WEBCLIENT_DEBUG；
Storage 需要 DFS/POSIX。

SCons 发布 `AGENT_BUILD_CONFIG_HEADER="rtconfig.h"`，应用与库必须看到同一配置。

## CMake

已有 BSP SDK target 的应用可使用：

```cmake
set(AGENT_RTTHREAD_RUNTIME ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_TRANSPORT ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_SDK_TARGET my_rtthread_sdk CACHE STRING "" FORCE)
add_subdirectory(path/to/cAGENT cagent)
add_subdirectory(path/to/cAGENT/ports/rtthread cagent_rtthread)
target_link_libraries(app PRIVATE
    cagent::rtthread_runtime cagent::rtthread_transport)
```

my_rtthread_sdk 必须由应用提供真实 BSP rtconfig.h、SDK include 与链接依赖。
CMake 的 Core 容量来自生成的 agent_build_config.h，不自动从 rtconfig.h 翻译。

需要文件时，先启用 AGENT_BUILD_FILE_STORE、AGENT_BUILD_POSIX_FILE_STORE，
再启用 AGENT_RTTHREAD_FILE_STORE 并链接 `cagent::rtthread_file_store`。

## Runtime 生命周期

零初始化并固定存放 `agent_rtthread_runtime_t`，
调用 `agent_port_rtthread_runtime_init(&runtime, &state, use_heap_allocator)`。
它安装静态 tick 采样 timer、扩展时钟和短 cancel 同步；可选 rt_malloc/rt_free。

先停止活动请求并销毁所有消费者，再 deinit。
SMP 或 all-soft timer BSP 必须在 detach 前外部排空采样分发；
detach 不是 join。tickless BSP 要正确补偿 tick，不能只根据 Host 测试推断时间正确。

## HTTP 与 TLS

`agent_port_rtthread_transport_init()` 接收 Runtime、SDK header 上限、
URL/I/O 缓冲和响应头描述符。当前仅支持非空 POST，适用于当前 OpenAI 请求；
GET、空 POST 和重定向不支持。

HTTPS 的 authenticated_tls 是应用对“证书链及主机名校验已配置”的声明，
不是由 Port 开关自动建立信任。SDK 无 TLS 时返回 NOT_SUPPORTED；
缺认证声明时拒绝，而不是跳过证书验证。

WebClient 使用内部 Heap，部分阻塞 DNS/TLS/读写无法及时取消。
SDK 版本自身的发送与头解析行为仍是集成风险，要实际测试。

## 文件与验收

文件入口 `agent_port_rtthread_file_store_init()` 复用 POSIX 后端，
挂载、可信 root、目录同步能力和无 symlink Profile 由 BSP 决定。

```sh
bash tests/ports/rtthread/compile.sh
bash tests/ports/rtthread/build.sh
```

SDK 头检查与额外验证入口见[开发记录](../development/rtthread.md)。
现有 Host 通过不等于 BSP 固件、HTTPS 和 Flash 掉电验收。
