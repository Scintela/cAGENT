// cAgentV2 文档站点配置（Docusaurus，docs-only 模式）
//
// 本地预览（在 docs/website/ 目录下）：
//   npm ci
//   npm start            # http://localhost:3000/cAGENT/
//   npm run build && npm run serve
//
// 推送 GitHub 后：Settings -> Pages -> Source 选 "GitHub Actions"，

/** @type {import('@docusaurus/types').DocusaurusConfig} */
const config = {
  title: 'cAgentV2',
  tagline: '面向嵌入式与 Host 的跨平台 C 语言 Agent 库',
  url: 'https://scintela.github.io',
  baseUrl: '/cAGENT/',

  // 中文为默认语言，英文翻译位于 docs/website/i18n/en/，
  // 未翻译页面自动回退显示中文原文。
  i18n: {
    defaultLocale: 'zh',
    locales: ['zh', 'en'],
    localeConfigs: {
      zh: { label: '简体中文' },
      en: { label: 'English' },
    },
  },

  onBrokenLinks: 'throw',
  onBrokenAnchors: 'throw',

  presets: [
    [
      '@docusaurus/preset-classic',
      {
        docs: {
          // 中文内容直接读取仓库 docs/zh/，不移动文件。
          path: '../zh',
          // docs-only 模式：文档即整站，docs/zh/index.md 就是首页，
          // 页面 URL 与文件路径一致（/architecture、/adr/0022-... 等）。
          routeBasePath: '/',
          sidebarPath: require.resolve('./sidebars.js'),
          // ADR 的数字前缀是身份标识而非排序装饰，
          // 禁用默认的数字前缀剥离，保持 id/URL 与文件名一致。
          numberPrefixParser: false,
        },
        blog: false,
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      },
    ],
  ],

  // 本地 lunr 搜索，中英双语；替代 MkDocs 时代的搜索配置。
  themes: [
    [
      '@cmfcmf/docusaurus-search-local',
      {
        language: ['en', 'zh'],
        indexDocs: true,
        indexBlog: false,
      },
    ],
  ],

  themeConfig: {
    navbar: {
      title: 'cAgentV2',
      logo: {
        alt: 'Scintela',
        src: 'img/logo-light.png',
        srcDark: 'img/logo-dark.png',
      },
      items: [
        {
          type: 'localeDropdown',
          position: 'right',
        },
        {
          href: 'https://github.com/Scintela/cAGENT',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      copyright: 'cAgentV2 contributors · MIT License',
    },
  },
  favicon: 'img/favicon.png',
};

module.exports = config;
