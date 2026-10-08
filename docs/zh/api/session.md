# Session 接口

公共头：`agent/session.h`。Session 领域接口不包含 JSONL、文件路径或平台 SDK。

## Storage Ops

| 操作 | 契约 |
|---|---|
| begin | 按 Session ID 创建本轮事务 |
| append | 追加规范消息；调用期借用，不允许保存悬空视图 |
| finish | COMPLETE/ABORTED 结束；即使失败也消费事务 |
| recent | 最多 max_candidates 个完整模型安全组，最新在前 |
| clear | 清除一个会话，身份保留语义由后端定义 |
| clear_all | 批量清除，失败可能已清掉一部分 |
| remove | 删除一个会话及身份；失败不保证仍存在 |
| count | 后端已知会话身份数量 |

`agent_session_group_view_t` 是 messages + count，仅在 recent visitor 中有效。
历史后端不能将缺失 Tool result 的半组标成完整组。Core 将选中数据复制进自己的有界 scratch。

`agent_session_storage_t` 包含 Ops 值和 context。
`agent_set_session_storage(agent, &storage)` 空闲复制绑定；NULL 禁用历史存储。
外部 context/缓冲由应用持有，不随 Agent 自动释放。
当前绑定校验要求上述八个回调均非 NULL；能力不支持时实现回调返回 NOT_SUPPORTED，
不能套用 File Store 的“缺回调表示无能力”约定。

## 应用查询

`agent_session_clear()`、`clear_all()`、`remove()`、`count()` 转发所绑定后端。
在空闲驱动上下文访问，后端不支持能力时返回 NOT_SUPPORTED。
删除/清理错误不具备回滚保证。

Session 保存、投影和持久化限制见[使用指南](../guides/session.md)；
具体后端配置见[Storage 参考](storage.md)。
