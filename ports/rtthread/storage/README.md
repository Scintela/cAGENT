# RT-Thread Byte-File Store

公共头 `agent_rtthread_file_store.h` 提供平台入口，实际复用 `ports/posix/storage`，
避免复制短 I/O、替换和失败清理逻辑。

```c
static agent_rtthread_file_store_t state;
static char path_scratch[512];
agent_file_store_t store;
agent_rtthread_file_store_config_t config = {
    mounted_trusted_directory, path_scratch, sizeof(path_scratch), false, false
};
agent_error_t error = agent_port_rtthread_file_store_init(&state, &config, &store);
```

根目录为借用的绝对路径，必须已存在、已挂载、受信任。应用负责挂载、格式化、
授权与并发串行化；Port 不自动创建目录或注册 Session/Memory。根字符串、state、
path scratch 在 Store 使用期间有效。

接口含 size/read(offset)/visit/append/truncate/sync/remove/replace；只读实例不
发布写能力。replace 使用同目录临时文件 + rename，published 区分已可见但后续
同步失败；不保证所有 Flash 文件系统均能掉电原子提交。

```text
Session -> JSONL Provider -> 文件命名桥 -> File Store -> DFS/POSIX
Memory  -> Markdown Provider ----------> File Store -> DFS/POSIX
```

JSONL naming scratch 与 Provider buffer 独立于 path scratch。Memory 可分别授权
SOUL/USER/MEMORY/每日笔记的 Store，不让 Core 知道路径或 Session 知道 Markdown。

BSP 必须提供所用的 POSIX 文件/目录调用。具体挂载的 ftruncate/rename/文件与目录
fsync 能力需板上验收，未实现则传播错误，不伪造成功。sync_directory=true 在
初始化就探测目录同步；默认不要求目录 fsync，不代表持久性保证。DFS 不等于每种
LittleFS/FAT/ROMFS 都能满足写入契约。

无 symlink 挂载才使用对应构建开关；否则保留 lstat/O_NOFOLLOW 防护。Backend
只处理安全平面文件名；枚举不排序，领域消费者自行排序。Host 测试覆盖实际文件、
Markdown 读取与 JSONL 桥，不代表 Flash 验收。
