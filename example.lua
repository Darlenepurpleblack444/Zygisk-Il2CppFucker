--==============================================================
-- Il2CppFucker 示例 / 参考模板 (by LiangYu)
--   放到 /data/data/<包名>/files/run.lua, 存盘即热重载.
--   演示: 导航->ESP绘制->反射调用->cmd处理->config, 全部用 il2.loop 驱动.
--
-- ★ 目标相关的「类名 / 偏移 / 方法名」都用 .cmd 的 class/fields/methods 动态探出来再填.
--   下面以 com.seayoo.ggd 为具体例子, 换游戏时整体替换.
--==============================================================

local DIR = "/data/data/com.seayoo.ggd/files/"

-- ① 顶层只解析一次: klass 句柄
local K_GSS = il2.find("Adam.Gameplay", "GameSettingSystem")  -- 单例所在类
local K_PLAYER = il2.find("Goose.Actors", "PlayerActor")

-- ② 目标相关偏移 (← 用 `fields <klass>` 探出来填; 这里是示例值)
local O_POS, O_UID, O_NAME_HOLDER = 0x64, 0x110, 0x160
local C_POS, C_ORTHO, C_SW, C_SH, C_TARGET = 0x64, 0xfc, 0x100, 0x104, 0x110

-- ③ 工具
local function rfile(p) local f=io.open(p,"r"); if not f then return nil end local s=f:read("*a"); f:close(); return s end
local function wfile(p,s) local f=io.open(p,"w"); if not f then return end f:write(s); f:close() end
local function num(v,d) return tonumber(v) or d end

-- ④ 非扫描导航: 静态单例 -> game -> world -> camera (绝不用 instances 扫, 防反作弊)
local function nav()
  local inst = il2.staticobj(K_GSS, "instance"); if not inst or inst==0 then return nil end
  local game = il2.u64(inst + 0x20);             if not game or game==0 then return nil end
  local world = il2.u64(game + 0x58);            if not world or world==0 then return nil end
  local cam = il2.u64(world + 0x80)
  local me  = cam~=0 and il2.u64(cam + C_TARGET) or 0   -- 本机角色 = 相机的 target
  return world, cam, me, game
end

-- ⑤ 缓存反射方法 (首次拿到 me 后解析一次)
local mTele = nil
local frame = 0

-- ⑥ 配置 (APP 写 cfg.txt: key=value)
local function readcfg()
  local c = { esp = "1", speed = "0" }
  local s = rfile(DIR.."cfg.txt")
  if s then for line in s:gmatch("[^\r\n]+") do
    local k,v = line:match("^%s*(%w+)%s*=%s*(.-)%s*$"); if k then c[k]=v end
  end end
  return c
end

-- ⑦ 主循环 (引擎每 30ms 驱动; 改 run.lua 即热重载)
il2.log("=== example armed ===")
il2.loop(function()
  frame = frame + 1
  local cfg = readcfg()
  local world, cam, me = nav(); if not world or cam==0 then return end

  -- 相机投影参数 (世界坐标 -> 屏幕像素)
  local cx, cy = il2.f32(cam+C_POS), il2.f32(cam+C_POS+4)
  local ortho = il2.f32(cam+C_ORTHO); local sw, sh = il2.u32(cam+C_SW), il2.u32(cam+C_SH)
  if not (cx and cy and ortho and sw and sh) or ortho<=0 then return end
  local ppu = sh/(2*ortho)
  local function sx(x) return sw/2 + (x-cx)*ppu end
  local function sy(y) return sh/2 - (y-cy)*ppu end

  local mex, mey, mez = 0,0,0
  if me and me~=0 then mex,mey,mez = il2.f32(me+O_POS), il2.f32(me+O_POS+4), il2.f32(me+O_POS+8) end

  -- === ESP: 遍历 world.actors[] (size@+0x58, 数据@actors+0x20), 透视(只读)绘制 ===
  il2.clear()
  if num(cfg.esp,1) ~= 0 then
    local actors = il2.u64(world+0x48); local size = il2.u32(world+0x58) or 0
    if size>256 then size=256 end
    for i=0,size-1 do
      local a = il2.u64(actors + 0x20 + i*8)
      if a and a~=0 and il2.u64(a)==K_PLAYER then         -- 校验 klass
        local x,y = il2.f32(a+O_POS), il2.f32(a+O_POS+4)
        if x and y then
          local px,py = sx(x), sy(y); local bw,bh = ppu*0.9, ppu*1.8
          il2.box(math.floor(px-bw/2), math.floor(py-bh), math.floor(bw), math.floor(bh), 0xff00ff00)
        end
      end
    end
  end
  il2.present()

  -- === 反射调用示例: 加速 = 调游戏自己的 Teleport(Vector3) 步进 (服务器接受, 不回弹) ===
  if not mTele and me and me~=0 then mTele = il2.method(il2.u64(me), "Teleport", 1) end
  local spd = num(cfg.speed, 0)
  if spd>0 and me and me~=0 and mTele and mTele~=0 then
    -- (真实加速需按移动增量步进, 此处仅示意 invoke_v3 用法)
    -- il2.invoke_v3(mTele, me, mex+dx, mey+dy, mez)
  end

  -- === 一次性命令: cmd.txt (执行后清空) ===
  local cmd = rfile(DIR.."cmd.txt")
  if cmd and #cmd>1 and me and me~=0 then
    local act, arg = cmd:match("^%s*(%w+)%s*(.-)%s*$")
    if act=="tp" and mTele and mTele~=0 then
      -- 例: tp <x> <y>  瞬移到坐标
      local tx,ty = arg:match("([%-%d%.]+)%s+([%-%d%.]+)")
      if tx then il2.invoke_v3(mTele, me, tonumber(tx), tonumber(ty), mez); il2.log("tp "..arg) end
    end
    wfile(DIR.."cmd.txt", "")
  end
end, 30)
