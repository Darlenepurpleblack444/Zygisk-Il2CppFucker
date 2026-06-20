# Il2CppFucker

A Zygisk module that injects into an IL2CPP game process and turns it into a live, scriptable
reverse-engineering engine — **dump metadata, reflect by name, read/write memory, call the game's
own methods, and run hot-reloadable Lua** — all in-process.

中文说明请戳 [README.zh-CN.md](README.zh-CN.md) ｜ 完整 API/命令文档：[IL2CPP_FUCKER.md](IL2CPP_FUCKER.md)

Heavily modded from [Perfare/Zygisk-Il2CppDumper](https://github.com/Perfare/Zygisk-Il2CppDumper). If you
**only** need a metadata dump, the upstream project is the lighter choice — this fork is a full
in-process tooling engine and is overkill for plain dumping.

## Features

- **Dump** il2cpp metadata to `dump.cs` at runtime (bypasses packing/encryption/obfuscation), including
  HybridCLR hot-update classes via in-game re-dump.
- **Generic reflection** — resolve classes/fields/methods/properties **by name** at runtime; read field
  offsets; enumerate instances.
- **Memory read/write** — arbitrary `process_vm_readv/writev` in the game's own address space.
- **Call the game's own methods** by reflection (`il2cpp_runtime_invoke`) — e.g. server-accepted actions,
  no rubber-banding.
- **Embedded Lua 5.4** with a `il2.*` API and **hot-reload**: overwrite `run.lua` and it re-runs within
  ~30 ms, no game restart. Optional boot-time `init.lua`.
- **Overlay draw channel** — emit boxes/text to a file for an external overlay app (ESP, etc.).
- **File-driven** — control entirely through files in the game's private dir; no extra daemon.

> The engine ships with **no game-specific commands**. Anything game-specific (ESP, hacks, …) you write in
> Lua — see [`example.lua`](example.lua) / [`example_init.lua`](example_init.lua).

## Build

1. Install [Magisk](https://github.com/topjohnwu/Magisk) v24+ and enable Zygisk (or a Zygisk-compatible
   root such as KernelSU/APatch with the Zygisk add-on).
2. Build the module:
   - **GitHub Actions** — fork → **Actions** tab → **Build** workflow → **Run workflow** → enter the
     target game package name → download the artifact. (The package name replaces the
     `com.game.packagename` placeholder in `game.h`.)
   - **Local** — edit `module/src/main/cpp/game.h` and set `GamePackageName` to your target package, then
     `./gradlew :module:zipRelease` (the zip lands in `out/`). Needs JDK 11–17 (the wrapper is Gradle 7.5).
3. Flash the module in Magisk and reboot.

## Usage

Start the target game. Everything happens under `/data/data/<package>/files/`:

- **Dump**: `dump.cs` is generated automatically on launch. For an in-game re-dump (HybridCLR classes):
  `echo dump > .cmd`, or from Lua `il2.dumpcs()`.
- **One-shot commands**: write a line to `.cmd`, read the result from `.result`
  (`class`, `fields`, `methods`, `read`, `write`, `method`, `invoke`, …).
- **Scripting**: put a Lua script at `run.lua` (hot-reloaded on change) or `init.lua` (runs once at
  startup). Register a loop with `il2.loop(fn, ms)` or just do one-shot work and return.

Full command + Lua API reference: **[IL2CPP_FUCKER.md](IL2CPP_FUCKER.md)**.

> File access note: the game's private dir is namespace-isolated. Write the control files as the game's
> uid (or via `nsenter -t 1 -m` as root). On Windows, push with PowerShell, not MSYS bash (it mangles
> `/sdcard` paths).

## ⚠️ Security & risk

This is an offensive in-process tool. Use it **only** on apps/games you are authorized to test.

- **Crashes & instability** — arbitrary memory writes and method calls can crash or corrupt the target.
- **Bans** — calling game methods / writing memory is detectable; many titles ship anti-cheat that will
  flag or ban accounts. Server-authoritative state cannot be faked from the client anyway.
- **The real attack surface is the control channel**: *anyone who can write to the game's `files/` dir can
  run arbitrary Lua/shell inside the game process.* Treat that directory as a code-execution sink — do not
  expose it, and remove the module when you are done.

## Credits

- Original: [Perfare/Zygisk-Il2CppDumper](https://github.com/Perfare/Zygisk-Il2CppDumper)
- Lua 5.4, [xDL](https://github.com/hexhacking/xDL)
- This fork (engine + Lua + tooling): **LiangYu**
