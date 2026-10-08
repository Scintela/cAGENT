# RT-Thread Integration

Runtime targets RT-Thread 5.1+; Transport targets WebClient 2.3. Platform public
headers may expose SDK types without leaking them into generic Core.

## SCons and Kconfig

Include root Kconfig and `ports/rtthread/Kconfig` in the BSP. The application
SConscript calls the Port SConscript and merges its groups. That script already
builds Core/selected Providers; do not add Core sources again.

Enable required AGENT_PORT_RTTHREAD_RUNTIME, TRANSPORT and FILE_STORE options.
Transport needs WebClient/RT_USING_HEAP. Disable WEBCLIENT_DEBUG, which can print
credentials. Storage requires DFS/POSIX.

SCons publishes `AGENT_BUILD_CONFIG_HEADER="rtconfig.h"`; application and library
must see the same configuration.

## CMake

An application with an existing BSP SDK target can use:

```cmake
set(AGENT_RTTHREAD_RUNTIME ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_TRANSPORT ON CACHE BOOL "" FORCE)
set(AGENT_RTTHREAD_SDK_TARGET my_rtthread_sdk CACHE STRING "" FORCE)
add_subdirectory(path/to/cAGENT cagent)
add_subdirectory(path/to/cAGENT/ports/rtthread cagent_rtthread)
target_link_libraries(app PRIVATE
    cagent::rtthread_runtime cagent::rtthread_transport)
```

my_rtthread_sdk must supply actual BSP rtconfig.h, SDK includes and link
dependencies. Core capacities come from generated agent_build_config.h, not an
automatic translation of rtconfig.h.

For files, enable AGENT_BUILD_FILE_STORE and AGENT_BUILD_POSIX_FILE_STORE, then
AGENT_RTTHREAD_FILE_STORE and link `cagent::rtthread_file_store`.

## Runtime Lifetime

Zero-initialize persistent `agent_rtthread_runtime_t`; call
`agent_port_rtthread_runtime_init(&runtime, &state, use_heap_allocator)`. It
installs a static tick-sampling timer, extended clock, short cancel sync and
optional rt_malloc/rt_free.

Stop requests and destroy all consumers before deinit. SMP/all-soft-timer BSPs
must externally drain sample dispatch before detach; detach is not join.
Tickless BSPs must compensate ticks correctly; Host tests do not prove target
clock correctness.

## HTTP and TLS

Transport init accepts Runtime, SDK header bound, URL/I/O buffers and response
header descriptors. Only nonempty POST is supported, suitable for current OpenAI;
GET, empty POST and redirects are unsupported.

authenticated_tls is the application's assertion that chain/hostname checks
are configured, not a Port switch that establishes trust. No SDK TLS returns
NOT_SUPPORTED; missing authentication is rejected, not bypassed.

WebClient uses heap. Blocking DNS/TLS/I/O may delay cancellation. SDK send/header
behavior remains an integration risk requiring actual tests.

## Files and Acceptance

`agent_port_rtthread_file_store_init()` reuses POSIX. BSP chooses mounts, trusted
roots, directory-sync capability and appropriate no-symlink profile.

```sh
bash tests/ports/rtthread/compile.sh
bash tests/ports/rtthread/build.sh
```

Real-header/extra checks are described in the [Chinese development record](development/rtthread.md).
Host success is not acceptance of BSP firmware, HTTPS or Flash power-loss behavior.
