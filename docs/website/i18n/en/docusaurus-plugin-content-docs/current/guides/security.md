# Security and Failure Boundaries

An embedded Agent can operate physical devices. Model text is not a trusted
control command. Bounded buffers and Policy reduce risk but do not replace
product authorization and device safety.

## Application Responsibilities

| Boundary | Requirement |
|---|---|
| HTTPS | Verify the certificate chain and hostname; fail without trust roots, never silently downgrade |
| Credentials | Application-owned; keep them out of logs, events and debug output |
| Tools | Default deny; validate business arguments and authorize trusted identities |
| Side effects | Idempotency, state queries, operation IDs; timeout can leave effects unknown |
| Documents | Trusted deployment for SOUL/Skills; user documents are data, not authority |
| Files | Application-authorized roots; never concatenate arbitrary model paths |
| Multiple users | Separate Store roots/bindings and serialized access, not just different Session IDs |
| Concurrency | One driver per instance; coordinate shared backends/external file changes |

READ_ONLY, groups, categories and Skill descriptions are metadata, not security
proofs. USER-role references do not eliminate prompt injection. Device access
still requires application-side authorization.

## Unsafe Recovery Patterns

- Silently truncate required rules after CAPACITY/CONTEXT_OVERFLOW.
- Assume TIMEOUT/CANCELLED proves a Tool had no effect.
- Assume failed Storage sync proves no record exists.
- Blindly retry Memory replacement after APPLIED/UNKNOWN.
- Disable certificate checks or log Authorization for diagnosis.
- Treat a no-symlink build mode as safe validation on a filesystem that supports symlinks.

## Release Validation

Measure Core/Provider/HTTP/TLS peaks and task stacks on target firmware. Test
certificates, deadlines, weak networks, reboot replay, full/read-only Flash,
power loss and device failures. Fake-SDK Host tests cover software contracts
only. Session is not a pre-action journal and does not provide exactly-once
physical actions.
