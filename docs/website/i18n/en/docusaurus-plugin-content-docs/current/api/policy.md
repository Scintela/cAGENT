# Policy Interface

Public header: `agent/policy.h`. The implementation is one application callback,
not a generic multi-policy framework.

| Type/value | Meaning |
|---|---|
| agent_policy_request_t | Registered Tool, execution context and MODEL source; read-only callback view |
| AGENT_POLICY_DENY | Deny; zero-initialized default |
| AGENT_POLICY_ALLOW | Allow, without bypassing the Tool confirmation flag |
| AGENT_POLICY_CONFIRM | Fail closed synchronously; no pending state |

Set `agent_set_policy_callback(agent, callback, user_data)` only while idle.
NULL/invalid decisions deny model Tool calls. user_data is borrowed until
replacement/destruction. The callback must not cause device effects or reenter
Agent.

Names, Schema, group/category and READ_ONLY are not authorization. Use trusted
request_user_data identity and product permissions. See [Tools](../guides/tools.md).
