# 平台接入概览

Core 使用相同的应用契约，Port 负责平台服务。构建选择实现，初始化绑定实例；
库不会自动联网、挂载、格式化文件系统或选择可信证书。

| 平台 | Runtime | HTTP/TLS | 文件存储 | 接入 |
|---|---|---|---|---|
| Host/POSIX | 应用提供 | 应用提供，无官方 curl 后端 | 已实现 POSIX | [Host](host.md) |
| ESP-IDF | esp_timer 最小时钟 | esp_http_client，CA/bundle | VFS/POSIX 薄入口 | [ESP-IDF](espidf.md) |
| OpenVela/NuttX | 单调时钟 | webclient；应用提供认证 TLS | NuttX/POSIX 薄入口 | [OpenVela](openvela.md) |
| RT-Thread | 5.1+ tick 扩展与取消同步 | WebClient 2.3，非空 POST；应用认证 TLS | DFS/POSIX 薄入口 | [RT-Thread](rtthread.md) |

## 通用装配顺序

1. 初始化应用网络与已挂载文件系统。
2. 初始化 Runtime，提供可靠单调时钟。
3. 初始化 Transport 和 File Store，保持状态/缓冲长期有效。
4. 初始化 Model/Storage/Memory Provider。
5. init Agent，借用绑定与注册，start 后由应用工作任务 run。
6. 结束所有活动调用，再销毁 Agent、wrapper 和有清理要求的平台服务。

## 验证级别

仓库 Host 测试包括模拟 SDK、接口编译、故障注入及构建开关矩阵。
它们不证明任意 SDK 版本、BSP、文件系统或 TLS 配置已实机通过。
RT-Thread 另有真实头检查入口，但仍不是固件/网络验收。

新增平台应实现[Runtime](../api/runtime.md)、[Transport](../api/transport.md)或[File Store](../api/storage.md)
中的实际需求，不为目录对称强制新增 TLS、线程或文件系统大抽象。
