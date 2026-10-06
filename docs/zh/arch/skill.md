# Skill 架构

Skill 是由应用提供的可信指令资源。Core 负责注册准入、固定容量、稳定排序与借用寿命；
文件读取、Markdown/front-matter 解析和目录发现属于可选加载器。

```text
应用静态文本 / 可选加载器
  -> agent_skill_t
  -> agent_register_skill()
  -> workspace 内的 Skill Registry
  -> Context 选择与预算
  -> model_request.system_prompt
  -> Model Provider 协议编码
```

## 文件与状态

| 文件 | 职责 |
|---|---|
| `include/agent/skill.h` | 借用定义、注册和注销接口 |
| `src/skill/skill_internal.h` | 私有固定数组布局、注册表字节数和投影入口 |
| `src/skill/skill_registry.c` | 准入、稳定插入、注销、独立全文投影 |
| `src/core/text_internal.h` | 无 JSON 依赖的 UTF-8 与标识校验 |
| `src/core/lifecycle.c` | 按对齐切分注册表，编译期验证 workspace 足够 |

注册表浅拷贝 `agent_skill_t`，并不复制全文。排序直接发生在固定数组中，无 heap、无额外索引树。
注销通过紧凑移动删除一个条目，同优先级的其他条目保持顺序。

## 预算与职责

独立 Skill 投影先计算必需内容和分隔符预算，再选择可选全文。Context 的联合投影需要同时
预留固定指令、Memory 和动态贡献，因此读取同一份已排序元数据并执行跨来源预算。
两条路径均不得截断 Skill。优先级是呈现顺序，`required` 是空间保障，两者不是同一个维度。

注册表是长期状态；Context 选择结果属于 turn。借用的 Skill 文本至少保持到注销，因此能跨越
同一 turn 中的多次模型调用。Context 不取得加载器内存的所有权。

## 当前边界

已经完成已有的全文贡献契约。没有自动启用目录、按需摘要模式、技能执行器、隐式 Tool 注册
或权限继承。ADR 0028 的按需模式仍是后续设计，ADR 0029 的加载器仍未实现。

测试入口为 `tests/skill/compile.sh`，联合使用见 `tests/context/compile.sh`。
公共用法见 [Skill 接口](../api/skill.md)。
