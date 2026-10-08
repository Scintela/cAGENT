# ESP-IDF 接入

Port 包位于 `ports/espidf/`，分 runtime、transport、storage。
Core 不包含 ESP-IDF SDK 头，应用使用各子包的 `agent_espidf_*.h`。

## 构建

将仓库作为名为 `cagent` 的 ESP-IDF component，
并将 `ports/espidf` 加入应用 component 搜索路径。
Port 的 REQUIRES 使用组件名 cagent，改名需同步修改构建依赖。

Kconfig 按需选择：

| 开关 | 作用 |
|---|---|
| CONFIG_AGENT_PROVIDER_OPENAI | Core component 内编译 OpenAI 与私有 JSON |
| CONFIG_AGENT_PORT_ESPIDF_RUNTIME | esp_timer Runtime |
| CONFIG_AGENT_PORT_ESPIDF_TRANSPORT | esp_http_client Adapter |
| CONFIG_AGENT_FILE_STORE / CONFIG_AGENT_POSIX_FILE_STORE | 通用字节契约/POSIX 后端 |
| CONFIG_AGENT_PORT_ESPIDF_FILE_STORE | ESP-IDF 文件薄入口 |
| CONFIG_AGENT_SESSION_JSONL / CONFIG_AGENT_MEMORY_MARKDOWN | 独立领域后端 |

Runtime/Transport 开关按需声明 SDK 依赖；
文件薄入口需要通用 File Store 与 POSIX 后端同时启用。
JSONL 与 Markdown 不因为启用文件 Port 自动绑定。

## Runtime 与 Transport

`agent_port_espidf_runtime_init(&runtime)` 填充 esp_timer 单调时钟。
最小 builder 不自动提供 Heap allocator 或跨任务取消锁，按产品需求追加服务。

`agent_port_espidf_transport_init(&binding, &state, &config)` 接受
Runtime、request_timeout_ms、URL/header 文本缓冲和响应头描述符数组。
HTTPS 选择借用 cert_pem 或 use_crt_bundle；bundle 依赖平台证书 bundle 配置。
不能同时乱配或在无可信配置时退化为不验证。

Model Provider 接收导出的 `agent_transport_t`，不接收 esp_http_client_handle_t。
SDK 的 HTTP/TLS 内部 Heap 不计入 Core Workspace；测量 SDK 峰值和任务栈。

## 文件系统

应用先挂载 VFS 并建立受信任 root，再调用
`agent_port_espidf_file_store_init()`。该入口配置与 POSIX 后端相同。
平台当前使用无 symlink 的后端构建模式，仅适用于真正不支持 symlink 的受信任挂载。

文件系统未提供 truncate/rename/目录同步等能力时不能宣称 JSONL 或整文替换具备对应保证。
mount、格式化、分区、空间管理与掉电测试均由应用/BSP 负责。

## 验证

```sh
bash tests/ports/espidf/compile.sh
bash tests/build/file_store/compile.sh
```

这些是 Host 假 SDK/构建验证。真实 SDK release、联网、证书和文件系统耐久性需另行验收。
