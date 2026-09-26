# cAgentV2

cAgentV2 是面向 ESP-IDF、openvela、RT-Thread 和 Host 的轻量、可组合、资源可预算的 C
语言 Agent Core。仓库当前处于架构与公共 API 草案阶段，尚未提供可链接的完整运行时。

## 文档

- [总体架构](docs/architecture.md)
- [公共 API 草案](docs/api/public-api.md)
- [ADR 索引](docs/README.md)

## 头文件检查

```sh
bash tests/headers/compile.sh
```

该检查覆盖所有公共头文件的独立包含、组合包含以及 C99/C++11 编译；不证明运行时行为。

## 许可证

本项目采用 [MIT License](LICENSE)。
