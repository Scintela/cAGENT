# Synchronous Run Architecture

`src/run/react_loop.c` drives execution; applications call `agent_run()`.

## Three Lifetimes

- Agent: registries/bindings spanning many Turns.
- Turn: one user request, effective limits, cancel token and current messages.
- Model iteration: one complete call returning final text or complete Tool calls.

Session is a conversation across Turns, not created by agent_start.

## Inputs and Facts

Prepare stable Skill/Memory snapshots each Turn. Refresh dynamic state, Tools
and messages for each model projection, then release that projection. Copy model
sink output before retaining it in the current Turn; never save Provider
decoded_buffer pointers as history.

Pair handler results with Tool Call IDs even on failure, preserving model-safe
relationships. Existence of a final response and successful delivery are distinct.

## Terminal Handling

Normal completion, model error, capacity exhaustion, cancellation and timeout
share finalization: consume Storage transaction, deliver available valid final
text, update stats/events, revoke active token and reset scratch. Even failed
finish leaves no publicly resumable active transaction.

Timeout/cancel cannot undo device actions. Core never automatically retries
side-effecting handlers.

## Tests

`tests/run/contract.c` covers deterministic Model/Tool, Storage faults, budgets,
delivery, cancel and reentry. `tests/run/openai.c` uses the real OpenAI Provider
with fake HTTP. These validate the software chain, not remote services/device safety.
