# 测试与贡献

改动应按风险增加契约测试，而不只验证公共头能编译。
Host、模拟 SDK、真实 SDK 编译和实机验证是四个不同级别。

## 日常验证

在仓库根运行：

```sh
bash tests/headers/compile.sh
bash tests/core/compile.sh
bash tests/tool/compile.sh
bash tests/skill/compile.sh
bash tests/context/compile.sh
bash tests/run/compile.sh
bash tests/json/compile.sh
bash tests/transport/compile.sh
bash tests/providers/openai/compile.sh
bash tests/session/compile.sh
bash tests/session/jsonl_compile.sh
bash tests/memory/compile.sh
bash tests/providers/memory_markdown/compile.sh
bash tests/providers/files/compile.sh
bash tests/ports/posix/file_store_compile.sh
bash tests/ports/posix/file_store_faults.sh
bash tests/build/tool/compile.sh
bash tests/build/context/compile.sh
bash tests/build/file_store/compile.sh
bash tests/ports/espidf/compile.sh
bash tests/ports/openvela/compile.sh
bash tests/ports/rtthread/compile.sh
bash tests/ports/rtthread/build.sh
```

脚本使用临时构建目录、严格编译选项和断言。
OpenAI 与 Run 测试的 HTTP 是假的，POSIX 文件测试是真实 Host 文件操作；
二者都不是实机验收。

## 新能力的最低测试

- 正常路径及每个失败边界，失败后对象可再次使用。
- 固定容量边界、零容量裁剪、最大输入与对齐。
- 借用/owned 生命周期、重入、取消和 deadline。
- sink 首错、畸形 UTF-8/JSON、输出不足。
- 存储短读/部分写、发布后 sync 失败以及清理失败。
- C99 和 C++11 公共头独立编译。
- 平台源文件按开关裁剪，依赖不泄漏到无关目标。

Sanitizer 和真实 SDK 检查采用各脚本明确支持的选项，不假设所有脚本都支持同一个变量。
目标板还需测任务栈、SDK Heap 峰值、证书、网络异常和掉电恢复。

## 提交约定

保持机制、领域格式和平台 SDK 分层。新增公共字段前说明所有权、零值和失败事实。
代码/文档较大时分阶段提交；提交前运行相关测试并更新开发者手册，
不要把尚未实现能力写成可用配置。
