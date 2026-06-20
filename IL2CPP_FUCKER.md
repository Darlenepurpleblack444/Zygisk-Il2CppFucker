# Il2CppFucker —— 使用文档

> Zygisk 进程内 IL2CPP 通用反射 / 内存 / Lua 脚本引擎
> author: **LiangYu**　logcat TAG: `LiangYu`

魔改自 Zygisk-Il2CppDumper。注入**任意** IL2CPP 游戏进程内，提供：通用反射（按名取类/字段/方法、调用游戏自身方法）、任意内存读写、实例扫描、内嵌 Lua 5.4 脚本（"不可检测的 frida"）、悬浮窗绘制通道。

> **本引擎是通用的**，不绑定任何游戏。下文出现的 `com.seayoo.ggd` / `Goose.*` / `Adam.*` 只是**当前目标的举例**，换游戏时全部替换为新目标的包名/命名空间即可。

### 目标包名配置（换游戏只改这一处）
注入哪个进程由 `module/src/main/cpp/game.h` 决定：
```c
#define GamePackageName "com.seayoo.ggd"   // ← 改成新目标包名, 重新编译刷入即可
```
所有 `.cmd` 命令与 Lua API 都是**通用**的，不绑定任何游戏。ESP 之类的游戏专属逻辑请用 Lua 自己写（见 `example.lua`），引擎本身不内置任何游戏专属命令。

---

## 1. 架构 & 工作方式

- **进程内**：作为 Zygisk 模块随目标进程启动注入，所有读写/反射都在游戏自己的地址空间里（外部 heap 扫描会触发反作弊延迟自杀，进程内读安全）。
- **文件驱动**：宿主（APP / adb / PC）通过游戏私有目录下的文件与引擎通信。所有路径都在：
  ```
  /data/data/<包名>/files/
  ```
  > 注意命名空间隔离：游戏私有目录要用 `nsenter -t 1 -m` 或在游戏 uid 下访问；adb push 用 PowerShell（MSYS bash 会把 /sdcard 改路径）。

- **调度**：`hack.cpp` watch 循环 **30ms** 轮询：服务 `.cmd` 通道、按 mtime 重载 `run.lua`、驱动 Lua tick。

### 文件接口
| 文件 | 方向 | 作用 |
|---|---|---|
| `.cmd` | 入 | 一次性引擎命令（见 §2），写入即执行 |
| `.result` | 出 | 上条 `.cmd` 的结果 |
| `init.lua` | 入 | **启动瞬间跑一次**（见 §3.5） |
| `run.lua` | 入 | 常驻脚本，**改动 mtime 即热重载**（见 §3.5） |
| `script.log` | 出 | `il2.log(...)` 输出 |
| `overlay.txt` | 出 | `il2.box/text/present` 绘制指令（悬浮窗读） |

> 局内重新 dump（捕获 HybridCLR 热更类）：`echo dump > files/.cmd` 或脚本里 `il2.dumpcs()`。

---

## 2. `.cmd` 命令通道（暴露的指令）

写一行到 `.cmd`，结果出现在 `.result`。klass/obj/addr 均为**十六进制无 0x**。

| 命令 | 语法 | 说明 / 输出 |
|---|---|---|
| `dump` | `dump` | 重新 dump（含 HybridCLR 热更类）到 dump 文件 |
| `class` | `class <ns> <name>` | 返回 klass 指针（ns 为空写 `.`） |
| `fields` | `fields <klass>` | 列**自身**字段：`+0xOFF 类型 名称`（不含父类，要查父类用父类 klass） |
| `methods` | `methods <klass>` | 列方法：`名称 argc=N` |
| `props` | `props <klass>` | 列属性名 |
| `statics` | `statics <klass>` | 列静态字段 + 原始 u64 值（找单例/根） |
| `field` | `field <klass> <name>` | 返回该字段偏移（十进制；-1=无） |
| `getfield` | `getfield <obj> <klass> <name>` | 读对象字段：off + hex + floats + int64 |
| `setfield` | `setfield <obj> <klass> <name> <hexbytes>` | 写对象字段 |
| `getprop` | `getprop <obj> <klass> <name>` | 调属性 getter，输出 unbox 后的 hex/floats |
| `read` | `read <addr> <len>` | 读内存，返回 hex（len≤4096） |
| `rfloat` | `rfloat <addr> <cnt>` | 读 cnt 个 float（≤128） |
| `write` | `write <addr> <hexbytes>` | 写内存（process_vm_writev，≤4096B） |
| `instances` | `instances <klass> [max]` | 扫描 rw 段找该类实例（**会扫内存，多用触发反作弊**，优先非扫描导航） |
| `str` | `str <addr>` | Il2CppString → utf8 |
| `method` | `method <klass> <name> <argc>` | 返回 `method=.. codePtr=.. base=.. rva=0x..`（**直接给 IDA 用的 RVA**） |
| `invoke` | `invoke <klass> <name> <argc> <obj>` | 无参调用（仅 obj，无实参）；输出 exc/res |

> **方法论铁律**：别猜偏移/方法名，动态探出来。`class` → `fields`/`methods`/`statics` → `read 对象+0` 拿 klass 反查。能反射就反射（调游戏自己的方法，服务器接受、不回弹）；反射不了走透视(只读)。

---

## 3. Lua API（`il2.*`，run.lua 里用）

引擎把库注册为全局 `il2`。

### 反射 / 类
| 函数 | 签名 | 说明 |
|---|---|---|
| `il2.find(ns, name)` | →klass | 按命名空间+类名取 klass（ns 为空串 `""`） |
| `il2.classname(klass)` | →string | klass 名（常用 `il2.classname(il2.u64(obj))` 判类型） |
| `il2.field(klass, name)` | →int | 字段偏移（-1=无） |
| `il2.staticobj(klass, name)` | →int | 读静态字段值（取单例/导航根） |
| `il2.instances(klass[, max])` | →table | 扫描实例（慎用，会触发反作弊） |
| `il2.method(klass, name, argc)` | →MethodInfo* | 取方法（argc 必须匹配重载） |
| `il2.dumpcs()` | →bool | **il2cppdumper 原始功能**：把当前 il2cpp 元数据 dump 成 `dump.cs` 写到 `files/`。只想 dump 的用户写个脚本自定时机调一次即可（热更类要等其加载后再调，见下） |

### 内存读（失败返回 nil）
| 函数 | 读取 |
|---|---|
| `il2.u64/u32/u16/u8(addr)` | 无符号 64/32/16/8 位 |
| `il2.i32/i64(addr)` | 有符号 32/64 位 |
| `il2.f32/f64(addr)` | float / double |
| `il2.read(addr, n)` | n 字节裸串（≤65536） |
| `il2.str(strPtr)` | Il2CppString → lua string |

### 内存写
| 函数 | 说明 |
|---|---|
| `il2.write(addr, bytes)` | 写裸字节（配 `string.pack` 用），返回 bool |
| `il2.newstr(s)` | lua string → Il2CppString*（给 String 参方法用） |

### 调用游戏方法（反射 invoke，返回结果指针 / -1 异常 / 0 void）
| 函数 | 用于 |
|---|---|
| `il2.invoke(m, obj)` | 无实参方法 |
| `il2.invoke_f(m, obj, x)` | 单 float 参 |
| `il2.invoke_v3(m, obj, x,y,z)` | Vector3 参（Teleport / set_position） |
| `il2.invoke_v3b(m, obj, x,y,z, bool)` | Vector3+bool（SetMovementInput） |
| `il2.invoke_args(m, obj, spec, ...)` | **通用**，按 spec 传任意参（见下） |

**`invoke_args` 的 spec 字符**（按顺序对应后面的实参）：
```
i = int32    l = int64    f = float    b = bool
2 = Vector2(x,y)    3 = Vector3(x,y,z)    o = 对象/字符串指针(引用类型直接传指针)
```
例：`SendSceneTaskProgressReq(long id,int sub,long sn)` → `il2.invoke_args(m, ctl, "lil", id, sub, sn)`
　　`PlayAction(PlayerActor caster, Vector3 pos, bool enter)` → `il2.invoke_args(m, it, "o3b", me, x,y,z, 1)`

> 返回值注意：**对象指针返回可靠**（如 get_xxx 返回实例）；**基本类型(int/bool)返回不可靠**（拿到的是裸寄存器值）。要读基本返回值，改读字段/属性。**void 调用一律 OK**。

### 调度 / 循环（脚本内循环）
| 函数 | 说明 |
|---|---|
| `il2.loop(fn, ms)` | 注册 tick 回调，引擎每 ms 毫秒驱动一次（非阻塞、可被热重载打断）。**这是脚本主循环的正确写法**，别自己写 while+sleep 死循环 |
| `il2.stop()` | 注销 tick |
| `il2.sleep(ms)` | 阻塞睡（仅诊断脚本用，tick 里别用） |
| `il2.log(...)` | 写 script.log（tab 分隔多参） |

### 悬浮窗绘制（写 overlay.txt 给悬浮窗 APP）
| 函数 | 说明 |
|---|---|
| `il2.clear()` | 清当前帧绘制缓冲 |
| `il2.box(x, y, w, h[, color])` | 矩形（color=AARRGGBB，默认绿） |
| `il2.text(x, y, str[, color])` | 文字（默认白） |
| `il2.present()` | 把本帧绘制原子写入 overlay.txt |

---

## 3.5 Lua 脚本：放哪 & 什么时机执行

引擎有**两个脚本槽**，都在 `/data/data/<包名>/files/` 下，共用同一个 Lua 虚拟机：

| 槽 | 路径 | 执行时机 | 用途 |
|---|---|---|---|
| **init.lua** | `files/init.lua` | **进程启动、libil2cpp 加载完成 + 首次 dump 之后，调度循环开始之前，执行一次**（= il2cppdumper 原始 dump 的同一时机） | 开机一次性 hook / 初始化 / 提前布置 |
| **run.lua** | `files/run.lua` | 引擎每 30ms 轮询，**文件 mtime 一变就整段重跑**（热重载） | 实时迭代、常驻功能（ESP/加速等） |

两者都是：**需要常驻就调 `il2.loop(fn, ms)` 注册 tick；不需要就做完事直接返回**（一次性脚本）。init.lua 里注册的 tick 会一直被调度；之后 run.lua 可覆盖。

**★ 时机坑（重要）**：init.lua 跑得**很早**，此刻 **HybridCLR 热更类（如 `Goose.*`/`Adam.*`）还没加载**，`il2.find` 只能拿到 AOT 类，热更类会返回 0。两种解法：
- 要用热更类 → 在 init.lua 里 `il2.loop` 内**重试**（轮询直到 `il2.find` 拿到非 0），或者
- 直接用 **run.lua**（等游戏进对局、热更类就绪后再 push）。
- 也可 `echo dump > files/.cmd`（或 `il2.dumpcs()`）局内重新 dump 以捕获热更类。

**放置方式**（命名空间隔离，必须 PowerShell；bash 的 adb push /sdcard 会被 MSYS 改路径）：
```powershell
adb push xxx.lua /sdcard/Download/xxx.lua
$u = adb shell su -c "stat -c %U /data/data/<pkg>/files"   # 游戏 uid
adb shell su -c "cp /sdcard/Download/xxx.lua /data/data/<pkg>/files/run.lua; chown $u`:$u /data/data/<pkg>/files/run.lua"
```

示例：`example.lua`（run.lua 用，含 loop）、`example_init.lua`（init.lua 用，启动一次性 + 等热更类的重试范式）。

**只想 dump（不要任何功能）**——写个 init.lua 决定时机调 `il2.dumpcs()` 一次即可：
```lua
-- init.lua: 启动即 dump 一次 (AOT 类). 想连热更类一起 dump 就用下面的轮询版.
il2.dumpcs(); il2.log("dumped at boot")

-- 等热更类(Goose.*/Adam.*)加载后再 dump 一份完整的:
-- local done=false
-- il2.loop(function() if done then return end
--   if (il2.find("Goose.Actors","PlayerActor") or 0) ~= 0 then
--     il2.dumpcs(); il2.log("dumped with hot-update"); done=true; il2.stop() end
-- end, 300)
```
> 等价的 `.cmd` 方式：`echo dump > files/.cmd`。

---

## 4. 脚本内循环的标准结构

```lua
-- 1) 顶层：缓存 klass / 常量（只解析一次）
local gss = il2.find("Adam.Gameplay", "GameSettingSystem")
local pk  = il2.find("Goose.Actors", "PlayerActor")
local frame = 0

-- 2) 非扫描导航到根（静态单例 → game → world），别用 instances 扫
local function getworld()
  local inst = il2.staticobj(gss, "instance"); if not inst or inst == 0 then return nil end
  local game = il2.u64(inst + 0x20); if not game or game == 0 then return nil end
  return il2.u64(game + 0x58), game        -- world, game
end

-- 3) 注册 tick（引擎驱动；约 33fps）
il2.loop(function()
  frame = frame + 1
  local world = getworld(); if not world then return end
  -- ... 每帧逻辑 ...
end, 30)
```

要点：
- 偏移/方法名先用 `.cmd` 的 `fields`/`methods` 动态探出来，写进脚本。
- 导航用静态链（`staticobj`→`u64`链），**不要 `instances` 扫描**（反作弊会盯 heap 扫描）。
- 改值常无效（服务器权威/装饰字段）→ 改调游戏方法（`invoke*`）。
- 热重载：存 `run.lua`（PowerShell push）即生效，无需重启游戏。

完整可运行示例见同目录 **`example.lua`**。

---

## 5. 典型工作流

```powershell
# 取某方法的原生地址 RVA（丢进 IDA 反编译/下断）
echo "class Goose.Ctls SoulOutCtl" > .cmd   # 取 klass
echo "method <klass> OnUpdate 0"   > .cmd   # 取 codePtr + rva

# 探结构
echo "fields <klass>"  > .cmd
echo "methods <klass>" > .cmd

# 推常驻脚本（热重载）
adb push run.lua /sdcard/Download/run.lua
adb shell su -c 'cp /sdcard/Download/run.lua /data/data/<pkg>/files/run.lua; chown <u>:<u> ...'
```

> HybridCLR：Goose/Adam 命名空间是热更解释执行，IDA 里方法体只是解释器跳板（`sub_2A3FEC4`），真实逻辑在 IL 字节码里。结构/签名能 dump，方法体需另走 IL 提取。
