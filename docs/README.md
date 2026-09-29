# cAgentV2 文档

本目录包含文档站点（`website/`）与中文内容（`zh/`），经 Docusaurus 构建后发布
（配置见 `website/docusaurus.config.js`）。中文为默认语言，英文逐步补齐。

## 目录结构

| 路径 | 内容 |
|------|------|
| `zh/` | 中文文档（站点默认语言）：首页、总体架构、模块责任地图、公共 API 草案与全部 ADR。 |
| `website/` | Docusaurus 应用：配置、侧边栏、主题与英文翻译（`website/i18n/en/`）。未翻译页面在站点上保持原 URL 并回退显示中文原文。 |
| `logs/`、`plan/` | 开发过程记录，不进入站点。 |

站点采用 docs-only 模式：页面 URL 与 `zh/` 下文件路径一致
（`/architecture`、`/adr/0022-...` 等），首页即 `zh/index.md`。

## 本地预览

需要 Node 18+（本仓库开发机用 fnm 管理）：

```sh
cd docs/website
npm install
npm start            # 开发服务器，http://localhost:3000/cAgentV2/，热更新
# 或构建后静态预览：
npm run build && npm run serve
```

CI（`.github/workflows/docs.yml`）在 `docs/` 变更时自动构建并发布到
GitHub Pages，无需部署分支。本地搜索基于 lunr +
[nodejieba](https://github.com/yanyiwu/nodejieba) 中文分词。

## 写作约定

- 新增 ADR：沿用四位递增编号与 `NNNN-kebab-case.md` 命名，放入 `zh/adr/`，
  并同步更新 `zh/adr/index.md` 的索引表与 `website/sidebars.js` 的导航。
- 新增页面：放入 `zh/`（翻译放入 `website/i18n/en/` 对应路径，缺省回退中文），
  并在 `website/sidebars.js` 登记。
- 站内页面之间使用相对链接；指向仓库源码的链接使用 GitHub 绝对 URL。
- 正文中的 `<`、`{` 等字符要放在行内代码或代码块里，否则 MDX 会当作 JSX 语法
  导致构建失败（现有文档已全部符合）。
