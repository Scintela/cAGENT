---
slug: /
title: cAgentV2 开发者手册
---

# cAgentV2 开发者手册

cAgentV2 是面向嵌入式设备与 Host 的跨平台 C99 Agent 库。应用提供平台服务、模型和工具，
库负责一次请求内的上下文组装、模型迭代、工具安全调度和会话记录。

本手册描述当前源码的使用契约，而不是未来接口草案。公共 API 仍在演进，不承诺跨版本二进制 ABI；
应用与库应使用同一提交、同一构建配置一起编译。

## 从这里开始

| 你的目标 | 阅读入口 |
|---|---|
| 在电脑上运行第一个 Agent | [快速开始](getting-started/quickstart.md) |
| 将库加入现有应用 | [构建与配置](getting-started/build.md) |
| 连接真实 LLM | [OpenAI 接入](guides/openai.md) |
| 让模型调用设备能力 | [Tool 与授权](guides/tools.md) |
| 配置指令、实时状态和历史 | [上下文与 Skill](guides/context.md) |
| 保存对话与读取长期记忆 | [Session](guides/session.md)、[Memory](guides/memory.md) |
| 接入目标系统 | [平台概览](platforms/index.md) |
| 查函数、字段和失败语义 | [API 总览](api/public-api.md) |
| 理解内存和模块关系 | [总体架构](architecture.md)、[内存模型](arch/memory.md) |

## 能力边界

| 能力 | 当前范围 |
|---|---|
| 执行 | 单实例、单驱动任务、同步有界 ReAct；支持协作取消和超时 |
| 模型 | 规范消息与 Tool 视图；OpenAI 非流式 Chat Completions |
| 上下文 | 系统指令、全文 Skill、Memory 快照、动态资料、完整历史 Turn |
| 工具 | 语法与应用语义验证、默认拒绝的 Policy、有界输出 |
| 会话 | 无历史存储、RAM 后端或 JSONL 文件后端，由应用选择 |
| 记忆 | SOUL、USER、MEMORY、每日笔记的整文读写及显式投影 |
| 平台 | ESP-IDF、OpenVela、RT-Thread 适配代码；POSIX 文件后端 |
| 不包含 | 语音/UI、设备驱动、网络重连、调度器、自动记忆提取、流式模型、人工确认恢复 |

Core 运行期不申请 Heap；HTTP/TLS SDK 可以分配内存。Host 契约测试不代替真实平台的
联网、证书、栈、Flash 重启与掉电验证。各平台限制见[平台概览](platforms/index.md)。

## 学习顺序

首次接入建议完成快速开始，再依次增加 OpenAI、一个只读 Tool、Session 和 Memory。
先验证每一步的返回值与生命周期，再扩大容量或加入副作用工具。

ADR 保存设计动机和候选方案；开发日志保存某次实现与验证的事实。
两者都不是调用手册，遇到历史内容差异时以当前公共头文件和本手册接口参考为准。

## 项目资源

- [源码仓库](https://github.com/Scintela/cAGENT)
- [测试与贡献](contributing/testing.md)
- [文档组织与维护](contributing/documentation.md)
- [架构决策索引](adr/index.md)

项目采用 [MIT License](https://github.com/Scintela/cAGENT/blob/main/LICENSE)。
