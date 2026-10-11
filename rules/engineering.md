# 全局工程规则

规则按四个目标分段：嵌入式（E）、跨系统（P）、轻量（L）、稳定（S）。每条规则附可判定的检验——检验不过即违规，与其讨论意图，不如改代码或修订规则本身。

E1/E2/E3/E5 的检验有机器版（`checks.sh`），CI 应调用同一脚本；标注"评审"的条目诚实依赖 review，不假装是红线。

## 嵌入式纪律

### E1 动态内存

src/ 与 include/ 禁止调用 malloc/calloc/realloc/free；内存只来自调用方 workspace、注入 allocator 或栈。注入分配器的成员调用（`allocator->alloc/free`）不在此列。

检验：checks.sh E1。设计背景：ADR 0014、0017。

### E2 系统头白名单

src/ 与 include/ 只允许 `<string.h>` `<stdint.h>` `<stddef.h>` `<stdbool.h>` 与本项目头（`<agent.h>`、`<agent/*.h>`）。stdio/errno/time/pthread 一律禁止；新增系统头必须先修订本条。

检验：checks.sh E2。设计背景：ADR 0017。

### E3 预处理边界

src/ 的 `#if/#ifdef/#ifndef` 只允许两类：`__cplusplus` 守卫、config 容量宏（`#if AGENT_MAX_TOOLS > 0` 形态）。平台判断不得进入 src/；触碰平台 SDK 头的代码只住 ports/。

检验：checks.sh E3。设计背景：ADR 0017、0018。

### E4 容量上界

每个数组、缓冲、池必须有编译期上界，且经三级宏（`AGENT_*` ← `CONFIG_AGENT_*` ← 默认值）可配；Kconfig 侧必须带 range 与档位默认。禁止运行期无界增长。

检验：评审——新增容量宏时 config.h、Kconfig、AgentBuildConfig.cmake 三处必须同步（三通道归一是已知债务，完成前以此条兜底）。设计背景：ADR 0011、0019。

### E5 可重入

禁止文件级与函数级可变 static/全局状态；一个进程可并存多个 agent 实例。禁止隐藏线程与忙等，阻塞语义只能来自注入回调。const 查表允许。

检验：checks.sh E5。设计背景：ADR 0017。

### E6 调用窗口标注

每个公共函数的注释必须写明调用窗口：idle-only / active 允许 / 回调期禁止 / 非 ISR-safe。

检验：评审（公共头抽查）。设计背景：ADR 0015。

## 跨系统纪律

### P1 语言基线

ISO C99、无编译器扩展（`C_EXTENSIONS OFF`）；公共头必须在 C++ 下可编译（`extern "C"` 守卫）。

检验：tests/headers 的 C/C++ 双语言编译矩阵 + `-std=c99 -pedantic -Werror` 全绿。

### P2 数据中立

不假设字长（用定宽类型）、端序（禁止指针重解释）、对齐（arena 手工对齐）；`uint64_t` 仅用于计数与字节量；`long double` 仅出现在对齐 union。8 位目标明确不在支持范围。

检验：评审——无 union punning、无类型双关转换。设计背景：ADR 0009。

### P3 vendor 隔离

第三方代码只住 codecs/json/vendor/、列禁改区、符号静态化（JSMN_STATIC 模式），不得泄漏进公共链接命名空间。

检验：tests/json/link_collision.c。设计背景：ADR 0021、0022。

## 轻量纪律（L）与演进纪律（S）

成文中，届时补充。发布前本节留空不作数。
