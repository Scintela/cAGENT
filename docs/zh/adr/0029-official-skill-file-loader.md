# ADR 0029: 官方可选 Skill 文件加载器

- 状态：提案（触发条件未满足，暂不实现）
- 日期：2026-10-01
- 关联：[ADR 0023](0023-file-backed-memory-soul-skill.md)、[ADR 0025](0025-builtin-tools-boundary.md)、[ADR 0026](0026-session-file-io-adapters.md)、[ADR 0027](0027-minimal-markdown-memory-layout.md)、[ADR 0028](0028-skill-projection-modes.md)

## 背景

ADR 0023 规定 Skill 加载器留在应用侧，Core 不接触文件系统。V1 参赛版按此模型在
smart_home 应用内实现了完整的文件加载器（约 470 行：目录枚举、定长读取、
front-matter 解析、缓冲管理与 read_skill 工具），验证了链路可行，也暴露了三类
真实问题：

1. **部分失败留下不一致状态**：`load_dir` 中途解析失败直接返回，此前已注册的
   Skill 与已分配缓冲不回滚；叠加影子 store 后，模型仍可读到半加载内容。
2. **影子真相**：read_skill 查询应用侧 store 而非注册表，注销后仍可读（V1 的
   ENABLED 漏洞同构）。
3. **手搓解析**：参数 JSON 与转义各手写一份，属可复用机制债。

同时 ADR 0026 已为先例：JSONL Session 文件适配器作为可选文件 I/O 包交付。本文
裁决 Skill 文件加载器是否、何时、以何种形态成为官方交付物。

## 决定

### 交付形态：官方可选包，两层结构

- **通用加载层**：front-matter 解析、校验、注册与回滚。不触碰任何平台 SDK，
  以"文件名 + 有界字节"为输入，Host 上可完整测试；作为独立可选目标
  （候选 `providers/skill/loader/`）交付。归 `providers/` 而非 `src/`：它是可选
  格式实现，`src/` 只收每个构建都存在的 Kernel 机制，`src/skill/` 保留注册表
  机制并作为本层的下游。目录归属规则见
  [当前模块与目录地图](arch/module-map.md)。
- **POSIX 薄层**（候选 `ports/posix/skills/`）：目录枚举、定长读取与排序。
  Host 可验证通用逻辑；ESP-IDF VFS、NuttX VFS、RT-Thread DFS 仅是候选复用目标，
  须分别验证目标文件系统的目录、路径与文件操作语义。

不进 Core，不新增 Core loader ops 公共契约（格式未冻结前固化接口属过早抽象）。
裸机或私有机型继续应用侧自写，不因本 ADR 被排除。

### 触发条件：三条全满足后才实现完整目录加载

1. Skill registry、双模式投影（ADR 0028）与读取 Tool 链路在至少一个真实产品
   验证可用；
2. front-matter 键名与目录约定在至少两个产品中稳定（格式事实冻结）；
3. 公共 Skill 查询接口按 ADR 0028 裁决的语义落地（签名随实现定稿），单一真相
   在注册表。

触发前，应用参照 V1 参赛版自写加载器是预期路径；允许提前交付一个不冻结格式的
单文件辅助（见"实施节奏"第 0 步）。

### API 形态：一次调用 + 配置结构体 + store 句柄 + 结果数组

路径是主输入但不构成完整契约；内存、信任、失败与拆除无法从路径推导。
以下是**契约草案**，具体类型名与签名在实现时定稿：

```c
typedef struct {
    const char* dir;                /* 必填：扫描目录，应为绝对挂载点。 */
    /* 内存策略，两种互斥方式：注入分配器（方式 A）或应用缓冲池（方式 B）。 */
    const agent_allocator_t* allocator; /* A：store 复制 allocator 值并释放本批分配。 */
    void* pool;                     /* B：应用提供、由本 store 独占的连续缓冲。 */
    size_t pool_bytes;
    size_t max_file_bytes;          /* 单文件上限；超限按容量失败，禁止无界读取后截断。 */
    bool (*accept)(void* ctx, const char* filename); /* 可选白名单；NULL=全收（见信任前提）。 */
    void* accept_context;
    agent_skill_projection_t projection_default;     /* front-matter 缺省时的模式。 */
    bool best_effort;               /* 默认 false：严格失败并回滚本批。 */
} agent_skill_loader_config_t;

typedef enum {
    AGENT_SKILL_LOAD_REGISTERED,
    AGENT_SKILL_LOAD_FAILED,
    AGENT_SKILL_LOAD_ROLLED_BACK,
    AGENT_SKILL_LOAD_NOT_PROCESSED
} agent_skill_load_disposition_t;

typedef struct {
    agent_string_view_t filename; /* 借用调用方 inventory，非 store 缓冲。 */
    agent_string_view_t name;     /* 仅 REGISTERED 时借用 store；其他状态为空。 */
    agent_error_t status;         /* FAILED 时为具体错误；其他状态为 AGENT_OK。 */
    agent_skill_load_disposition_t disposition;
} agent_skill_loader_result_t;

agent_skill_loader_config_t agent_skill_loader_config_default(const char* dir);
agent_error_t agent_skill_loader_load(agent_t* agent,
                                      agent_skill_loader_store_t* store,
                                      const agent_skill_loader_config_t* config,
                                      void* inventory, size_t inventory_bytes,
                                      agent_skill_loader_result_t* results,
                                      size_t results_capacity,
                                      size_t* results_written);
agent_error_t agent_skill_loader_unload_all(agent_t* agent,
                                            agent_skill_loader_store_t* store);
```

一个 store 一次只管理一批：非空 store 再次 `load` 应拒绝；`unload_all` 只注销该
store 注册的项，不触碰应用先前注册的 Skill。store 持有注册项所借用的内容、
name 与 description；应用池必须由该 store 独占，直到卸载或失败回滚完成，
不得穿插其他分配。应用须在 idle 期串行调用 `load`/`unload_all`；`accept` 回调
不得重入 Agent 或修改目录。store 未卸载前，应用不得绕过 store 单独注销或同名
重新注册本批 Skill；若未来需要选择性移除，须增加注册身份契约，不能只凭名称
判定所有权。`unload_all` 先注销，全部成功后才释放/归还缓冲；如因活跃
turn 等原因无法注销，须保留尚被借用的缓冲，不得制造悬空视图。allocator 的
回调表按值复制，其 context 保持应用持有。应用负责让 store、目录字符串和文件
内容缓冲活到注销或 Agent 销毁；Agent 销毁后才可直接释放 store。

`inventory` 是独立于内容池的调用方缓冲，用于保存并排序本批被 `accept` 接受的
文件名；不能在回滚或 `unload_all` 时由加载器释放。导入前先有界枚举并核对
`inventory_bytes` 和 `results_capacity`，不足则返回 `AGENT_ERROR_CAPACITY`、
`results_written=0`，**不注册任何项**；不能处理到一半才发现报告放不下。
预检成功后 `results_written` 等于被接受的文件数，每个结果对应一个文件，
包括严格失败后尚未处理的文件。`filename` 借用 inventory，直到应用复用它；
`name` 仅在 `REGISTERED` 时有效，借用期到该项注销为止。调用方应在卸载前
读取或复制结果，不可把其中的视图长期保存；必须先读 `disposition`，不能仅凭
`status == AGENT_OK` 推断已注册。导入期间应用须保证目录集合和
文件内容不被并发修改；读时变化仍按 I/O 或格式失败处理。

front-matter 到 `agent_skill_t` 的字段映射（name/description/priority/
projection/required）在格式冻结时终稿；缺省值由 `projection_default` 兜底。name
冲突按注册表既有语义返回 `AGENT_ERROR_EXISTS`，计入该文件的失败报告。

### 失败语义：默认严格回滚，best_effort 显式选择

- **严格模式（默认）**：任一被接受文件失败时，逆序注销本批已注册项并释放/归还
  缓冲（应用池回到批次标记），返回该失败类别；返回后注册表与 store 与加载前
  一致。失败文件记为 `FAILED`，此前成功的项改为 `ROLLED_BACK` 并清空 `name`，
  后续文件为 `NOT_PROCESSED`。逆序注销只允许在 idle 期，不要求复制整批内容。
- **best_effort 模式（显式置位）**：失败文件跳过——未注册、不占缓冲——继续
  加载其余；成功项为 `REGISTERED`，失败项为 `FAILED`。总返回值取首个失败
  类别；**返回错误不代表没有已注册项**，调用方必须检查结果或执行卸载。

两种模式各有防护对象，产品应按需求选择：

| 模式 | 防什么 | 代价 |
|---|---|---|
| 严格回滚（默认） | 应用忽略结果数组导致的静默降级；与同步 MVP 的失败关闭哲学一致 | 非关键文件损坏（含 flash 位翻转）殃及本批全部，包括完好的关键 Skill |
| best_effort（显式） | 尽职应用按文件检查关键 Skill，完好能力得以保留 | 懒应用会静默降级 |

关键度是 per-Skill 的，而严格模式是全有或全无；需要局部可用性的产品可选择
best_effort，但必须逐文件检查。结果只涵盖枚举到的文件，**无法发现本应存在却
缺失的关键 Skill**；应用须另外提供预期名称清单，并在加载后核验注册表。
front-matter 中的 `required` 只约束 Context 投影，不代表文件缺失时的加载策略。
无论哪种模式，I/O、格式、容量错误都不得静默。

### 顺序、信任与符号链接

- **稳定排序**：目录枚举顺序（`readdir` 在不同文件系统/挂载间不稳定）不进入
  语义；加载前按文件名字节序排序后依次注册，保证同优先级 Skill 的注册序可跨
  挂载复现，投影顺序可测试。排序需要的文件名清单及结果数组都由调用方预算；
  不允许溢出后退化为未排序导入。
- **`accept == NULL` 的信任前提**：目录必须是应用授权、且不可被模型可见的写类
  Tool 写入的位置（否则重现"模型写文件即新增能力"的 ai_agent 漏洞）。该前提
  写入函数契约注释，由产品保证。
- **符号链接默认拒绝**：防止链接逃逸目录边界或指向可变目标。单独先检查
  再按路径打开仍有替换窗口；Port 应使用目标支持的安全相对打开方式，不能
  实现时须限定为应用控制且导入期间不可变的目录，或拒绝该组合。白名单不是
  路径隔离机制。

### 设计红线

- **显式调用**：`load` 只由应用显式调用，绝不挂接 `agent_init()` 或任何自动
  目录扫描；文件存在不构成注册（ADR 0023）。
- **内存显式**：两种互斥方式二选一，不隐式 heap；单文件上限固定。
- **ON_DEMAND 不等于懒加载**：本方案在导入时仍保存全文；它节省模型输入，
  不节省正文的常驻 RAM。若产品需要从文件按需读取，须另立内容读取契约。
- **无热重载心智**：重载 = idle 期 `unload_all` + `load`，turn 边界由调用
  时机保证；应用负责串行化和确保目录在导入期间稳定，加载器不监视文件系统。
- **不做策略**：目录可信边界、加载时机、白名单范围、失败模式选择始终由应用
  决定；loader 只消灭机械代码（枚举、定长读、排序、解析）。

## 不采用的方案

- **Core 内置文件加载**：破坏可移植性不变量（裸机目标）、文件缓冲内存域与
  Core 零 heap workspace 冲突、重新引入"扫描即启用"；拒绝。
- **Core 定义 loader ops 公共契约**：格式未冻结即固化接口，过早抽象；拒绝。
- **init 期自动扫描挂载目录**：ai_agent 反例路线，文件存在等同能力启用；拒绝。
- **默认 best-effort 静默跳过**：懒应用获得不可见的降级运行；失败必须响亮，
  best_effort 只能显式选择并配套结果检查。

## 与其他 ADR 的关系

- 细化 ADR 0023 的交付层：应用侧加载边界不变，官方可选包是"应用可复用的
  薄适配"，不是 Core 职责转移。
- 与 ADR 0026 共享"可选文件适配器"的组织先例；本 ADR 进一步把"通用逻辑"与
  "平台薄层"分离，避免 ports/ 收留无 SDK 依赖的代码。
- 依赖 ADR 0028 的双模式契约（projection 映射目标）与其裁决的公共只读查询
  语义（ACTIVE 期 const 读安全），读取 Tool 节奏遵循 ADR 0025。
- Skill 文件不在 ADR 0027 的四类 Memory 文件之列，目录布局约定独立裁决。

## 实施节奏

0. （触发前可选）POSIX 薄层先交付**单文件有界读取辅助**：应用自行提供全部
   元数据，辅助函数只负责"定长读入应用缓冲"；不解析 front-matter，因此不
   冻结格式。价值有限（约几十行），按需提供。
1. 触发条件满足前：仅维护本契约，不实现完整目录加载；应用自写加载器。
2. 触发条件满足后：定稿 front-matter 键名与目录约定，实现两层可选包与契约
   测试（回滚完整性、白名单、顺序复现、借用期、unload 一致性）。
3. 之后按产品需求评估热重载触发器的应用侧形态；加载器本身不内建触发器。

## 验证要求

- 严格模式：注入坏文件后，注册表与 store 与加载前逐项一致（回滚完整性）；
  返回错误类别正确。
- best_effort：结果数组完整覆盖被接受文件，失败类别正确；被跳过文件未注册
  且不占缓冲；返回错误但成功项仍可查询。
- 结果容量或 inventory 容量不足时，在注册前失败；严格回滚后结果中没有悬空
  `name`，已回滚与未处理文件可区分；结果的 `filename` 在卸载后仍有效，直到
  调用方复用 inventory。
- 同优先级注册顺序由文件名排序决定，跨挂载/重启可复现。
- 符号链接和替换攻击在所宣称支持的目标文件系统上被拒绝或有明确部署约束；
  白名单回调生效，未接受文件不读内容、不占缓冲。
- 借用期：store 释放后无存活视图；unload 后读取 Tool（经公共查询）查不到
  对应 Skill。
- 绕过 store 单独注销或同名重新注册本批项属于契约违规；`accept` 回调重入
  Agent 不得造成半注册状态。卸载被拒绝时不得释放尚被借用的缓冲。
- 单文件超上限按容量失败；无隐式 heap 分配；池模式 unload 只归还自己的整批，
  不影响其他应用分配。应用核验缺失的关键 Skill。
- `agent_init()` 之后无任何自动扫描行为；不注册 loader 时代码体积为零。

## 待定问题

- front-matter 键名终稿与目录布局约定（随触发条件 ② 冻结）。
- 是否支持子目录递归，首版建议不支持。
- 公共 Skill 查询接口的最终签名（语义已由 ADR 0028 裁决）。
