---
slug: /
title: cAgentV2 Developer Manual
---

# cAgentV2 Developer Manual

cAgentV2 is a C99 library for synchronous embedded agents. The application owns
platform services, budgets, models and tools; Core coordinates context projection,
model iterations, guarded tool execution and session finalization.

This manual describes the current source contracts, not future API proposals.
Build the application and library from the same revision and configuration;
cross-version binary ABI stability is not promised.

## Start Here

| Your goal | Start here |
|---|---|
| Run an Agent on your computer | [Quickstart](getting-started/quickstart.md) |
| Add the library to an application | [Build and configuration](getting-started/build.md) |
| Connect an LLM | [OpenAI integration](guides/openai.md) |
| Expose device capabilities | [Tools and authorization](guides/tools.md) |
| Configure instructions, state and history | [Context and Skills](guides/context.md) |
| Save conversations and read long-term memory | [Session](guides/session.md), [Memory](guides/memory.md) |
| Integrate a target system | [Platforms](platforms/index.md) |
| Look up functions and failure semantics | [API overview](api/public-api.md) |
| Understand modules and memory | [Architecture](architecture.md), [memory model](arch/memory.md) |

## Supported Scope

| Capability | Current scope |
|---|---|
| Execution | One instance, one driver task, synchronous bounded ReAct; cooperative cancellation and deadlines |
| Models | Canonical messages and Tool views; non-streaming OpenAI Chat Completions |
| Context | System instructions, full-text Skills, Memory snapshots, dynamic sources and complete historical Turns |
| Tools | Syntax and application validation, default-deny Policy, bounded output |
| Sessions | No history storage, optional RAM storage or JSONL files |
| Memory | Whole-document operations and explicit projection of SOUL, USER, MEMORY and daily notes |
| Platforms | ESP-IDF, OpenVela and RT-Thread adapters; POSIX file backend |
| Not included | Voice/UI, device drivers, reconnect logic, a scheduler, automatic memory extraction, streaming models or confirmation resume |

Core execution avoids heap allocation; HTTP/TLS SDKs may allocate. Host contract
tests do not replace real-device TLS, stack, Flash, reboot or power-loss validation.
See the [platform limitations](platforms/index.md).

## Learning Path

Complete the quickstart, then add OpenAI, one read-only Tool, Session and Memory.
Verify return values and ownership at each step before increasing budgets or
adding side effects.

The developer manual is available in English. Historical ADRs and development
logs remain in Chinese and are outside the English translation; their pages
fall back to the original text. These records explain decisions, not the current calling API.
When historical content differs, follow the current headers and API reference.

## Project Resources

- [Source repository](https://github.com/Scintela/cAGENT)
- [Testing and contributions](contributing/testing.md)
- [Documentation maintenance](contributing/documentation.md)
- [Architecture decision index](adr/index.md)

The project uses the [MIT license](https://github.com/Scintela/cAGENT/blob/main/LICENSE).
