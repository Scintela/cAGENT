---
slug: /
title: cAgentV2 Developer Manual
---

# cAgentV2 Developer Manual

cAgentV2 is a C99 library for synchronous embedded agents. The application owns
platform services, budgets, models and tools; Core coordinates context projection,
model iterations, guarded tool execution and session finalization.

The complete maintained manual is currently in Chinese. Untranslated pages in
this locale fall back to Chinese; this is not a complete English translation.

## Start Here

- [Run a deterministic Host application](getting-started/quickstart.md).
- [Configure and build with the application](getting-started/build.md).
- [Connect the non-streaming OpenAI Provider](guides/openai.md).
- [Integrate a platform](platforms/index.md).
- [Look up current APIs](api/public-api.md).
- [Understand the architecture](architecture.md).

The synchronous Core/Model/Tool/Session chain is implemented. Optional RAM/JSONL
Session, Markdown Memory and platform adapters have Host contract tests.
Real-device TLS, memory, reboot and power-loss validation remain platform-specific.

Core execution avoids heap allocation; HTTP/TLS SDKs may allocate. Streaming
models, confirmation resume, automatic memory extraction and an official Skill
file loader are not implemented. Build application and library from the same
revision and configuration; binary ABI stability is not promised.

See the [source repository](https://github.com/Scintela/cAGENT) and
[MIT license](https://github.com/Scintela/cAGENT/blob/main/LICENSE).
