# Host / POSIX

先运行[快速开始](../getting-started/quickstart.md)。Host 不依赖 Kconfig，
使用 CMake 和应用自定义 Runtime/Model/Transport。

## 文件存储

启用 `AGENT_BUILD_FILE_STORE=ON` 与 `AGENT_BUILD_POSIX_FILE_STORE=ON`，
链接 `cagent::posix_file_store`。

```c
#include <agent_posix_file_store.h>

static agent_posix_file_store_t state;
static char path[512];
static agent_file_store_t store;

agent_error_t open_documents(const char* trusted_directory)
{
    const agent_posix_file_store_config_t config = {
        .directory = trusted_directory,
        .path_buffer = path,
        .path_capacity = sizeof(path),
        .read_only = false,
        .sync_directory = true
    };
    return agent_posix_file_store_init(&state, &config, &store);
}
```

目录必须是已存在的绝对受信任 root，字符串也必须持续存活。
Store 不持有长期文件描述符；共享状态和路径缓冲要外部串行化。
目录 fsync 在初始化时探测，失败应处理，不能假设所有挂载都支持。

JSONL 通过领域 bridge 绑定同一字节接口；Memory 的用户 root 和 notes root 明确隔离。
读写、替换发布与错误语义见[Storage 参考](../api/storage.md)。

## 网络

目前没有官方 Host Runtime builder 或 libcurl Adapter。
应用可以实现同步 Transport Ops，遵守 headers/body/预算/取消/TLS 契约。
测试中的 fake HTTP 是确定性测试设施，不是生产网络后端。
