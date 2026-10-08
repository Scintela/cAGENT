# OpenVela / NuttX 接入

Port 包位于 `ports/openvela/`。公共入口为
`agent_openvela_runtime.h`、`agent_openvela_transport.h`、`agent_openvela_file_store.h`。

## 构建接入

由 NuttX 应用构建引入仓库 Core、Provider 和 Port；
在应用 Kconfig source 根 Kconfig 及 Port Kconfig，再在 NuttX CMake 环境加入
`ports/openvela`。Port 使用 nuttx_add_library，不是普通 Host CMake 的替代入口。

| 开关 | 前置条件 |
|---|---|
| CONFIG_AGENT_PORT_OPENVELA_RUNTIME | 单调时钟支持 |
| CONFIG_AGENT_PORT_OPENVELA_TRANSPORT | CONFIG_NETUTILS_WEBCLIENT |
| CONFIG_AGENT_PORT_OPENVELA_FILE_STORE | CONFIG_AGENT_FILE_STORE、已挂载 POSIX 文件系统 |

应用还要保证所有目标使用相同构建配置，并链接实际 Core/Provider 与网络库。
Port 可复用已有 File Store/POSIX target，不应重复编译一份公共后端。

## 服务绑定

`agent_port_openvela_runtime_init()` 提供 clock-only Runtime。
需要跨任务 cancel、allocator 或日志时由应用补齐。

Transport 配置包括 Runtime、URL/请求头缓冲、请求头数组、
webclient I/O 缓冲以及响应头数组/正文存储。
`agent_port_openvela_transport_init()` 校验并导出统一 HTTP Ops，不建立连接。

HTTPS 必须提供实现认证的 `webclient_tls_ops` 和 tls_context，
验证证书链与主机名。已有 webclient 不代表 TLS 已配置，更不能复制宽松握手来“兼容”。
request_timeout_ms 主要是每次 I/O 空闲上限，整体 deadline 是协作检查，
不能保证抢占 SDK 阻塞 DNS、握手或读写。

## 文件 Store

`agent_port_openvela_file_store_init()` 绑定应用已挂载的受信任 root，
复用通用字节 I/O；不要求 JSONL。JSONL/Markdown 各自消费该契约，
不把会话格式写到 Port 中。

## 验证

```sh
bash tests/ports/openvela/compile.sh
bash tests/build/file_store/compile.sh
```

Host 假 SDK 不是完整 OpenVela 固件构建。目标工具链、TLS 组件、文件系统能力、
重启回放和掉电测试是独立验收项。
