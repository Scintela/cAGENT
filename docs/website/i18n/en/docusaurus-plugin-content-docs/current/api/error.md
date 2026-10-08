# Error Codes

Public header: `agent/error.h`. The flat enum separates shared failures from
domain-specific semantics. Only AGENT_OK succeeds; errors are negative.

| Name | Value | Meaning |
|---|---:|---|
| AGENT_OK | 0 | Success |
| AGENT_ERROR | -1 | External failure without a more specific classification |
| AGENT_ERROR_NOMEM | -2 | Allocator/Provider heap allocation failed |
| AGENT_ERROR_INVALID | -3 | Invalid argument, configuration or local data |
| AGENT_ERROR_STATE | -4 | Lifecycle state forbids the operation |
| AGENT_ERROR_BUSY | -5 | ACTIVE state or callback reentry prevents the operation |
| AGENT_ERROR_LIMIT | -6 | Behavior or bounded-object limit |
| AGENT_ERROR_TIMEOUT | -7 | Effective deadline expired |
| AGENT_ERROR_CANCELLED | -8 | Cooperative cancellation observed |
| AGENT_ERROR_NOT_FOUND | -9 | Object absent |
| AGENT_ERROR_EXISTS | -10 | Duplicate name |
| AGENT_ERROR_NOT_SUPPORTED | -11 | Unsupported by this build/backend |
| AGENT_ERROR_IO | -12 | I/O did not complete successfully |
| AGENT_ERROR_AUTH | -13 | Remote credential/authorization rejection |
| AGENT_ERROR_TRUNCATED | -14 | Incomplete final-text delivery |
| AGENT_ERROR_PARSE | -15 | Non-model-specific input/format parsing failed |
| AGENT_ERROR_CAPACITY | -16 | Fixed Workspace, pool or scratch exhausted |
| AGENT_ERROR_CONTEXT_OVERFLOW | -32 | Context text exceeds its bound |
| AGENT_ERROR_MODEL_FAILED | -48 | Model failure without a more specific classification |
| AGENT_ERROR_MODEL_PARSE | -49 | Unparseable model response |
| AGENT_ERROR_MODEL_RATE_LIMIT | -50 | Model rate/quota rejection |
| AGENT_ERROR_MODEL_UNAVAILABLE | -51 | Model service temporarily unavailable |
| AGENT_ERROR_POLICY_DENIED | -64 | Authorization/confirmation denied execution |
| AGENT_ERROR_TOOL_ARGUMENT | -80 | Invalid model Tool arguments |
| AGENT_ERROR_TOOL_FAILED | -81 | Tool failure without a more specific classification |

`agent_error_str(code)` returns a static diagnostic name, never NULL; do not
free it. Compare with `status == AGENT_OK` or `status != AGENT_OK`; there are no
additional success/failure helpers.

Codes have no separate source field or exhaustive DNS/TLS/HTTP stage breakdown.
Log sanitized details at application Provider/Port boundaries. Also inspect
delivery_status, session_status or Memory change: status alone cannot prove that
an external action/publication did not occur. See [troubleshooting](../guides/troubleshooting.md).
