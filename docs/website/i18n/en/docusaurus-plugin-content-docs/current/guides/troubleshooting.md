# Troubleshooting

Record execution, Session and delivery statuses plus the Turn summary before
deciding whether retry is safe. A nonzero code is not permission to rerun a request.

| Symptom/status | Check first | Action |
|---|---|---|
| INVALID from init | now_ms, paired allocator/sync hooks, max_steps | Correct configuration |
| Workspace compile assertion | Slots, scratch and CORE_WORKSPACE_BYTES share one profile | Adjust total budget and rebuild everything |
| CAPACITY | Registry slots, scratch, Provider buffers, JSON tokens | Identify the exhausted domain, not just Core |
| CONTEXT_OVERFLOW | Required declarations, Skills, separator overhead | Reduce contributions or raise text budget |
| LIMIT | Steps/Tool calls, object bytes, message descriptors | Separate behavior limits from admission bounds |
| BUSY | ACTIVE mutation/query or callback reentry | Wait for Run and serialize access |
| POLICY_DENIED | Missing Policy, CONFIRM, hidden/disabled Tool | Check explicit authorization; never allow by default |
| AUTH | Bearer value, remote permissions, TLS configuration | Check security configuration without logging credentials |
| MODEL_PARSE | Protocol, finish_reason, message fields | Reproduce with a sanitized Provider response |
| MODEL_RATE_LIMIT | Remote rate/quota | Bounded application backoff, not automatic side-effect replay |
| Delayed TIMEOUT/CANCELLED | SDK blocking in DNS/TLS/I/O | Tune SDK timeouts and keep state alive until return |
| Incomplete text | delivery_status, output_required | Enlarge future buffers; do not rerun executed actions for delivery |
| Missing history | max_history_turns defaults to 0, Storage, complete groups | Enable the window explicitly and check ID |
| Delete/replace error | published/change, directory sync capability | Read back facts before retrying |

See [errors](../api/error.md) and the [memory model](../arch/memory.md). There is
no complete structured public channel for model error-body diagnostics or exact
capacity sources. Log sanitized details at application Provider/Port boundaries
without changing public error semantics.

## Reproduce a Failure

1. Fix the source revision and shared build configuration.
2. Reduce the case to one request with deterministic Model/Transport implementations.
3. Keep necessary sizes, statuses, summary and configuration; remove secrets/private data.
4. Rerun the relevant [contract tests](../contributing/testing.md).
5. Include SDK/BSP, TLS, filesystem and stack details for platform failures.
