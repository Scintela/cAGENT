# Skill 开发日志

日期：2026-10-06。

## 本次实现

- 用固定容量 `agent_skill_registry_t` 替换注册表骨架，支持注册、注销和稳定优先级排序。
- 注册时检查名字、字段上限、UTF-8、NUL 和 workspace 借用冲突。
- 实现全文投影：必需内容预留空间，可选内容整段跳过，sink 错误透传。
- 注册表进入 `agent_init()` 布局与编译期尺寸校验，零容量时不分配注册表。
- 提取 Core 文本准入 helper，Tool 与 Skill 共用；Skill 不依赖 JSON 编解码器。

## 接口与数据流

`agent_register_skill()` 复制描述符，`agent_unregister_skill()` 解除借用。
描述符内容与生命周期见 [API](api/skill.md)；固定数组和投影边界见[架构](arch/skill.md)。
应用文本进入 Registry，Context 按本轮预算选择，最终成为 Model 请求中的指令文本。
库不会在注册时读文件，也不会在注销时释放应用内存。

## 验证

`bash tests/skill/compile.sh` 覆盖零/单/默认槽位、重名、非法名字、非法 UTF-8、槽位不足、
必需内容预留、优先级及稳定排序、回调/ACTIVE 增删拒绝和 sink 失败。
Core、Tool、Session、Memory、OpenAI 和公共头回归通过。

后续 Context 的联合测试与 Host/模拟 IDF 构建进一步验证了关闭 Tool/JSON 后的独立使用。
真实 MCU 的栈和峰值测量未在本次完成；按需模式、文件加载器以及 Run 驱动仍需独立实现。
