# 工程规则

全局工程纪律不放在文档站，而在仓库 `rules/` 目录——工程契约面向贡献者与 AI 结对会话，与检验脚本同址维护。

- `rules/engineering.md` — 全局规则（嵌入式 / 跨系统 / 轻量 / 稳定四段，每条附可判定检验）
- `rules/exceptions.md` — 例外登记（理由与撤销条件）
- `rules/checks.sh` — 红线检验：`bash rules/checks.sh`，退出码 0 即通过

修改代码前先读对应规则；修改规则与修改代码同级严肃。测试规范另见[测试与贡献](./testing.md)。
