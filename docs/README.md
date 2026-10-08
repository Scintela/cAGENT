# 文档站维护

中文开发者手册位于 `docs/zh/`，Docusaurus 配置位于 `docs/website/`。
首页为教程、指南、参考、平台与架构提供入口；ADR 和开发日志在维护资料中保留，
不作为应用接入的前置阅读。

## 本地构建

需要 Node.js 22、npm；示例验证另外需要 CMake 3.16+ 与 C99 编译器。

```sh
cd docs/website
npm ci
npm run check
npm run check:examples
npm run build
npm start -- --host 127.0.0.1
```

预览地址为 http://localhost:3000/cAGENT/。静态产物位于 `website/build/`；
GitHub Actions 构建并发布至 https://scintela.github.io/cAGENT/。
开发者手册提供中英文版本。英文内容位于
`website/i18n/en/docusaurus-plugin-content-docs/current/`，与中文手册保持相同页面 ID。
维护资料（ADR 与开发日志）只维护中文，不新增英文版；英文导航明确标注为中文资料，
未翻译页面回退中文原文。

## 内容结构

| 路径 | 读者与用途 |
|---|---|
| zh/getting-started/ | 第一次运行与加入应用构建 |
| zh/guides/ | 模型、工具、上下文、会话、记忆与故障操作 |
| zh/api/ | 当前头文件对应的接口参考 |
| zh/platforms/ | 平台依赖、装配与支持限制 |
| zh/architecture.md、zh/arch/ | 模块、执行和内存原理 |
| zh/development/、zh/adr/ | 历史实施与架构决策 |
| website/ | 导航、搜索、主题、验证脚本与翻译 |

## 维护规则

站内页面使用相对 Markdown 链接，源码使用 GitHub 链接。
翻译页与中文维护资料交叉链接时使用文档根相对形式（如 `api/context.md`、
`development/skill.md`），由 Docusaurus 在当前语言和原文目录中解析；
不要使用只在某一种语言的物理目录下成立的 `../` 路径。
新增手册页登记 sidebars；ADR 与开发日志按目录自动纳入维护导航。
不修改 ADR 编号，不删除设计历史来伪造已实现状态。

`npm run check` 检查中英文 Markdown 链接、导航覆盖、英文手册覆盖和代码示例一致性；
`check:examples` 提取中英文 C 示例，编译指南片段并分别运行快速开始。
完整站点构建检查 MDX、路由和站内锚点。
示例验证不调用真实 LLM 或 SDK，不等于设备验收。

新增或修改开发者手册时同步维护两种语言；API 标识、构建开关和可执行代码块保持一致。
不把英文译文中的能力描述升级为尚未实现的承诺。

详细写作约定及参考来源见 [文档组织与维护](zh/contributing/documentation.md)。
