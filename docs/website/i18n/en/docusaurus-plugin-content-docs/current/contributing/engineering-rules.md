# Engineering Rules

Global engineering discipline lives in the repository `rules/` directory rather than on this site — it is a contract for contributors and AI pairing sessions, kept next to its checks.

- `rules/engineering.md` — global rules (embedded / cross-platform / lightweight / stability, each with a decidable check)
- `rules/exceptions.md` — exception registry (reason and revocation condition)
- `rules/checks.sh` — red-line checks: `bash rules/checks.sh`, exit code 0 means pass

Read the relevant rules before changing code; changing rules is as serious as changing code. See [Testing and Contributions](./testing.md) for test conventions.
