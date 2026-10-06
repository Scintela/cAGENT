# Context 接口

状态：2026-10-06 已实现统一有界投影，联合使用 Session、Tool、Skill、Memory；
`agent_run()` 尚未接入，以下注册能力不能单独构成端到端对话。

## 动态贡献

公共头为 `agent/context.h`。回调在驱动任务上同步执行，每次模型调用重新读取；不创建后台线程。

```c
#include <agent.h>
#include <agent/context.h>

static agent_error_t build_state(void* data, const agent_context_request_t* request,
                                  const agent_text_sink_t* output)
{
    (void)data;
    if (agent_cancel_token_is_set(request->cancel)) return AGENT_ERROR_CANCELLED;
    return output->write(output->context, agent_string_view("lamp=off", 8u));
}

agent_error_t register_state(agent_t* agent)
{
    const agent_context_provider_t source = {
        .name = AGENT_SV_LITERAL("device-state"),
        .priority = 10,
        .required = true,
        .build = build_state,
        .user_data = NULL,
        .max_bytes = 128u,
        .placement = AGENT_CONTEXT_REFERENCE
    };
    return agent_register_context(agent, &source);
}
```

示例回调不阻塞。真实设备或网络查询需要自行轮询取消与 deadline；Core 只能在回调前后及
sink 写入时检查，不能抢占一个阻塞的同步回调。`agent_cancel_token_is_set()` 由 `agent.h` 声明。

| 字段 | 契约 |
|---|---|
| `name` | 唯一 ASCII 标识，规则与 Skill 名字一致 |
| `priority` | 同类贡献的呈现优先级；同优先级保持注册顺序 |
| `required` | 失败终止构建；声明的上界参与必需预算预留 |
| `build` | 必需同步回调，文本经 sink 复制，不得保存 sink 或请求指针 |
| `user_data` | 应用拥有，至少覆盖注册寿命 |
| `max_bytes` | 单次正文上界；0 使用 Profile 上界，REFERENCE 同时受输入消息上界约束 |
| `placement` | INSTRUCTIONS 加入系统指令；REFERENCE 生成临时 USER 消息 |

建议显式设置符合业务的 `max_bytes`。准入按声明上界判断，不因预计实际输出较短而绕过预算。
例如 Profile 文本上限为 4096 时，一个声明 4096 字节的必需贡献无法再与非空固定指令共存。

回调执行按“必需贡献优先、可选贡献其次”；各组内按优先级。呈现时恢复注册表的优先级顺序。
回调应只读，不依赖其他回调的执行副作用。各块可以拆开一个 UTF-8 字符，完整贡献结束后统一校验。
sink 首错保持，忽略 sink 返回值也不能让失败输出被发布。

## 选择 Memory

先 `agent_set_memory()` 绑定 Provider，再选择逻辑文档；注册本身不进行 I/O：

```c
agent_error_t register_facts(agent_t* agent)
{
    const agent_memory_context_t facts = {
        .name = AGENT_SV_LITERAL("preferences"),
        .key = { .kind = AGENT_MEMORY_FACTS },
        .priority = 0,
        .required = false,
        .max_bytes = 512u
    };
    return agent_register_memory_context(agent, &facts);
}
```

SOUL 投影为应用授权的指令；USER、FACTS 和 NOTE 投影为临时 USER 角色参考消息。
NOTE 需要非空逻辑 `key.id`，具体日期/文件映射由 Memory Provider 决定。
不会自动读取全部笔记或扫描目录，选择项也不赋予模型修改文档或执行工具的权限。
USER 角色与内容分区不是提示注入防护保证；SOUL 的可信来源由应用保证。

动态来源和 Memory 选择共用 `AGENT_MAX_CONTEXTS` 槽位及名字空间。
用 `agent_unregister_context()` 删除任一种；注册项描述符复制，名字、NOTE id 和用户状态借用。
配置阶段不要求 Memory 已绑定；构建时未绑定会产生 NOT_SUPPORTED，按必需性处理。

## 错误、预算与寿命

注册/注销仅允许 CONFIGURING/READY；ACTIVE 或回调重入为 BUSY。非法参数为 INVALID，
非法 UTF-8 为 PARSE，声明超限为 LIMIT，重名为 EXISTS，槽位不足为 CAPACITY，零槽位为 NOT_SUPPORTED。

构建期规则：

- 必需来源失败：返回原错误，整次构建不发布模型输入。
- 可选来源失败：丢弃整段，并在私有 report 记录名字和错误；不发明公共事件类型。
- CANCELLED/TIMEOUT 始终终止构建，不因来源可选而降级。
- 文本预算不足为 CONTEXT_OVERFLOW，scratch 不足为 CAPACITY，消息描述符不足为 LIMIT。
- 文本预算覆盖固定指令、Skill、Memory 和动态贡献，每个非空贡献保守计入两个分隔字节，
  第一段免除此开销。历史消息另受单消息、描述符和剩余 scratch 约束；这不是 token 预算。

Memory 快照一次 turn 内稳定；动态数据逐次更新。公共 Memory 读写接口只允许空闲期，
不能在 Context 回调内重入；Memory 文档选择由驱动调用私有投影完成。
参考资料不追加到持久化 Session。更多内容见 [Context 架构](../arch/context.md)。
