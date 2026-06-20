--==============================================================
-- Il2CppFucker 启动脚本示例 (by LiangYu)
--   放到 /data/data/<包名>/files/init.lua
--   时机: 进程启动、libil2cpp 加载 + 首次 dump 之后立刻执行一次
--         (= il2cppdumper 原始 dump 时机), 早于调度循环.
--
--   ★ 此刻 HybridCLR 热更类(Goose.*/Adam.*)通常还没加载, il2.find 会返回 0.
--     下面演示: ① 一次性 boot 工作  ② 用 il2.loop 轮询等热更类就绪再干活.
--==============================================================

il2.log("=== init.lua: boot @ dump-time ===")

-- ① 启动瞬间的一次性工作 (AOT 类此时已可用)
--   比如记录基址、读全局配置、提前 hook AOT 层的东西.
local K_GSS = il2.find("Adam.Gameplay", "GameSettingSystem")
il2.log("boot: GameSettingSystem klass = " .. string.format("%x", K_GSS or 0))

-- 若只需一次性工作, 到这里 return 即可 (不注册 loop = 不常驻):
-- do return end

-- ② 需要热更类 → 注册 loop 轮询, 等类加载出来再初始化, 然后停表/转入功能.
local inited = false
il2.loop(function()
  if inited then return end
  local pk = il2.find("Goose.Actors", "PlayerActor")   -- 热更类: 早期为 0
  if not pk or pk == 0 then return end                  -- 还没就绪, 下个 tick 再看
  -- 到这里热更类已就绪, 做真正的初始化 (探偏移 / 装功能 / 起 ESP ...)
  il2.log("init.lua: hot-update ready, PlayerActor=" .. string.format("%x", pk))
  inited = true
  -- 若要常驻功能, 这里可以 il2.loop(主功能fn, 30) 覆盖成正式循环;
  -- 若只是一次性初始化, il2.stop() 收掉这个轮询.
  il2.stop()
end, 200)  -- 200ms 轮询一次, 不抢资源
