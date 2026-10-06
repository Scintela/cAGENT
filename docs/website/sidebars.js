// 侧边栏导航：与 docs/zh/ 目录对应。
// 新增 ADR 时在"设计决策"分类下追加一项（doc id 为去掉 .md 的相对路径），
// 并同步更新 docs/zh/adr/index.md 的索引表。

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  docs: [
    { type: 'doc', id: 'index', label: '首页' },
    {
      type: 'category',
      label: '架构',
      items: [
        { type: 'doc', id: 'architecture', label: '总体架构' },
        { type: 'doc', id: 'arch/module-map', label: '模块责任地图' },
        { type: 'doc', id: 'arch/tool', label: 'Tool 架构' },
      ],
    },
    { type: 'doc', id: 'api/public-api', label: 'API 参考' },
    { type: 'doc', id: 'api/tool', label: 'Tool 接口' },
    { type: 'doc', id: 'development/tool', label: 'Tool 开发记录' },
    { type: 'doc', id: 'development/memory', label: 'Memory 开发记录' },
    {
      type: 'category',
      label: '设计决策 (ADR)',
      items: [
        { type: 'doc', id: 'adr/index', label: 'ADR 索引' },
        { type: 'doc', id: 'adr/0006-json-integration', label: '0006 JSON 集成（历史）' },
        { type: 'doc', id: 'adr/0007-http-transport-adapters', label: '0007 HTTP Transport 适配' },
        { type: 'doc', id: 'adr/0008-error-contract', label: '0008 错误契约' },
        { type: 'doc', id: 'adr/0009-types-boundary', label: '0009 类型边界' },
        { type: 'doc', id: 'adr/0010-context-projection', label: '0010 Context 投影' },
        { type: 'doc', id: 'adr/0011-configuration-boundaries', label: '0011 配置边界' },
        { type: 'doc', id: 'adr/0012-agent-header-boundary', label: '0012 agent.h 边界' },
        { type: 'doc', id: 'adr/0013-text-representation', label: '0013 文本表示' },
        { type: 'doc', id: 'adr/0014-memory-domains', label: '0014 内存域' },
        { type: 'doc', id: 'adr/0015-event-model', label: '0015 事件模型' },
        { type: 'doc', id: 'adr/0016-session-history-storage', label: '0016 会话历史存储' },
        { type: 'doc', id: 'adr/0017-runtime-portability', label: '0017 Runtime 可移植性' },
        { type: 'doc', id: 'adr/0018-transport-portability', label: '0018 Transport 可移植性' },
        { type: 'doc', id: 'adr/0019-kconfig-integration', label: '0019 Kconfig 集成' },
        { type: 'doc', id: 'adr/0020-synchronous-run-mvp', label: '0020 同步运行 MVP' },
        { type: 'doc', id: 'adr/0021-platform-cjson-reuse', label: '0021 平台 cJSON 复用' },
        { type: 'doc', id: 'adr/0022-bounded-json-codec', label: '0022 有界 JSON codec' },
        { type: 'doc', id: 'adr/0032-memory-domain-management', label: '0032 Memory 领域管理' },
      ],
    },
  ],
};

module.exports = sidebars;
