# Il2CppFucker

一个 Zygisk 模块：注入 IL2CPP 游戏进程，把它变成一个**运行时可脚本化的逆向引擎**——
**dump 元数据、按名反射、读写内存、调用游戏自身方法、跑可热重载的 Lua**，全部在进程内完成。

[English](README.md) ｜ 完整 API/命令文档：[IL2CPP_FUCKER.md](IL2CPP_FUCKER.md)

魔改自 [Perfare/Zygisk-Il2CppDumper](https://github.com/Perfare/Zygisk-Il2CppDumper)。如果你**只**想要一份元数据
dump，上游项目更轻量——这个 fork 是一套完整的进程内工具引擎，单纯 dump 用它属于杀鸡用牛刀。

## 功能

- **Dump** 运行时把 il2cpp 元数据导成 `dump.cs`（绕过加壳/加密/混淆），可局内重 dump 捕获 HybridCLR 热更类。
- **通用反射**——运行时按名取类/字段/方法/属性、读字段偏移、枚举实例。
- **内存读写**——在游戏自身地址空间里任意 `process_vm_readv/writev`。
- **调用游戏自身方法**（反射 `il2cpp_runtime_invoke`）——比如服务器接受的动作，不回弹。
- **内嵌 Lua 5.4** + `il2.*` API + **热重载**：覆盖 `run.lua` ≈30ms 内自动重跑，无需重启游戏；可选启动脚本 `init.lua`。
- **悬浮窗绘制通道**——把矩形/文字写到文件给外部悬浮窗 APP（ESP 等）。
- **纯文件驱动**——全程通过游戏私有目录下的文件控制，无需额外守护进程。

> 引擎**不内置任何游戏专属命令**。游戏专属逻辑（ESP、各种功能）你用 Lua 自己写——见
> [`example.lua`](example.lua) / [`example_init.lua`](example_init.lua)。

## 构建

1. 安装 [Magisk](https://github.com/topjohnwu/Magisk) v24+ 并启用 Zygisk（或 KernelSU/APatch 等带 Zygisk 兼容层的 root）。
2. 编译模块：
   - **GitHub Actions**——fork → **Actions** 页 → **Build** workflow → **Run workflow** → 填目标游戏包名
     → 下载产物。（包名会替换 `game.h` 里的 `com.game.packagename` 占位符。）
   - **本地**——改 `module/src/main/cpp/game.h` 的 `GamePackageName` 为目标包名，然后
     `./gradlew :module:zipRelease`（zip 输出到 `out/`）。需 JDK 11–17（wrapper 是 Gradle 7.5）。
3. 在 Magisk 里刷入模块并重启。

## 用法

启动目标游戏。所有交互都在 `/data/data/<包名>/files/` 下：

- **Dump**：启动时自动生成 `dump.cs`。局内重 dump（含 HybridCLR 热更类）：`echo dump > .cmd`，或 Lua 里 `il2.dumpcs()`。
- **一次性命令**：写一行到 `.cmd`，从 `.result` 读结果（`class`/`fields`/`methods`/`read`/`write`/`method`/`invoke` …）。
- **脚本**：放 Lua 到 `run.lua`（改动即热重载）或 `init.lua`（启动跑一次）。用 `il2.loop(fn, ms)` 注册循环，或做完一次性工作直接返回。

完整命令 + Lua API：**[IL2CPP_FUCKER.md](IL2CPP_FUCKER.md)**。

> 文件访问：游戏私有目录有挂载命名空间隔离。要以游戏 uid 写控制文件（或 root 下 `nsenter -t 1 -m`）。
> Windows 上用 PowerShell 推送，别用 MSYS bash（会改写 `/sdcard` 路径）。

## ⚠️ 安全与风险

这是个进攻性的进程内工具，**只能**用于你有授权测试的 app/游戏。

- **崩溃与不稳定**——任意内存写和方法调用可能让目标崩溃或状态损坏。
- **封号**——调游戏方法/写内存是可检测的，很多游戏的反作弊会标记或封号；而且服务器权威的状态本来就没法从客户端伪造。
- **真正的攻击面是控制通道**：*任何能写进游戏 `files/` 目录的人，都能在游戏进程里跑任意 Lua/shell。*
  把那个目录当成代码执行入口看待——别暴露它，用完把模块卸掉。

## 致谢

- 原项目：[Perfare/Zygisk-Il2CppDumper](https://github.com/Perfare/Zygisk-Il2CppDumper)
- Lua 5.4、[xDL](https://github.com/hexhacking/xDL)
- 本 fork（引擎 + Lua + 工具链）：**LiangYu**
