# 安全与故障边界

嵌入式 Agent 能调用物理设备，模型文本不是可信控制命令。
有界缓冲与 Policy 降低风险，但不能替代产品级授权和设备安全逻辑。

## 产品必须负责

| 边界 | 要求 |
|---|---|
| HTTPS | 验证证书链及主机名；缺信任根时失败，不能静默降级 |
| 凭证 | 由应用保管；日志、事件和调试输出避免暴露 |
| Tool | 默认拒绝；validate 检查业务参数，Policy 检查真实身份和权限 |
| 副作用 | 幂等/状态查询/操作 ID；超时后动作状态可能未知 |
| 文档 | SOUL/Skill 来自可信部署；用户文档视为资料而非授权 |
| 文件 | root 由应用授权；禁止用模型参数拼任意路径 |
| 多用户 | 独立 Store root/绑定及串行化，不能只换 Session ID |
| 并发 | 一个实例一个驱动任务；共享后端和外部文件修改自行协调 |

`READ_ONLY`、group、category、Skill 描述都是元数据，不是安全证明。
把资料放在 USER 角色也不能保证消除提示注入；最终设备权限仍在应用侧检查。

## 不应做的恢复

- CAPACITY/CONTEXT_OVERFLOW 后静默截断必需规则。
- TIMEOUT/CANCELLED 后假定 Tool 未产生效果。
- Storage sync 错误后假定记录一定不存在。
- Memory replace 失败但 APPLIED/UNKNOWN 时盲目覆盖重试。
- 为排障关闭 TLS 证书校验或在日志打印 Authorization。
- 以无符号链接构建模式替代真实 symlink 文件系统的安全校验。

## 上线验收

在目标固件测量 Core/Provider/HTTP/TLS 峰值和任务栈；验证真实证书、超时、弱网、
重启回放、Flash 满/只读/掉电以及动作失败。Host 假 SDK 测试只能覆盖软件契约。
当前 Session 不是动作执行前日志，不提供物理动作 exactly-once。
