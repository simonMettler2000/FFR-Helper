-- FFR Helper field-save adapter for UE4SS. No arbitrary code or memory writes.
-- Experimental: the hook has not yet been verified in a live FFR session.
local source = debug.getinfo(1, "S").source:gsub("^@", "")
local root = source:gsub("Scripts[/\\]main%.lua$", "")
local connection = root .. "connection/"
local ready, enabled, attempted = false, false, false
local hookPath, preId, postId = nil, nil, nil
local lastError = "Waiting for game reflection"
local leaseSession = nil

local function quote(value)
    return '"' .. tostring(value):gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('[\r\n]', ' ') .. '"'
end

local function status()
    local handle = io.open(connection .. "status.json.tmp", "wb")
    if not handle then return end
    handle:write('{"protocol":1,"adapter":"ffr-helper-field-save","experimental":true,"time":', os.time(),
        ',"ready":', tostring(ready), ',"enabled":', tostring(enabled),
        ',"battleRestart":false,"session":', quote(leaseSession or ""),
        ',"hook":', quote(hookPath or ""), ',"message":', quote(lastError), '}')
    handle:close()
    os.remove(connection .. "status.json")
    os.rename(connection .. "status.json.tmp", connection .. "status.json")
end

local function control()
    local handle = io.open(connection .. "control.txt", "rb")
    if not handle then enabled = false; return end
    local contents = handle:read(512) or ""
    handle:close()
    local version, session, expiry, requested = contents:match("^(FFR_HELPER_CONTROL_V1)\n([a-f0-9]+)\n(%d+)\n([01])\n$")
    local now = os.time()
    if not version or #session ~= 32 or tonumber(expiry) < now or tonumber(expiry) > now + 8 then
        enabled = false
        return
    end
    leaseSession = session
    enabled = ready and requested == "1"
end

local function installHook()
    if attempted then return end
    attempted = true
    local candidates = {}
    local functions = FindAllOf("Function") or {}
    for _, fn in ipairs(functions) do
        if fn:IsValid() and fn:GetFName():ToString() == "IsTouchSavePoint" then
            local full = fn:GetFullName()
            if full:find("/Script/mucha%.") then candidates[#candidates + 1] = fn end
        end
    end
    if #candidates ~= 1 then
        lastError = "Expected exactly one game-native IsTouchSavePoint function; found " .. #candidates
        return
    end
    local owner = candidates[1]:GetOuter():GetFName():ToString()
    hookPath = "/Script/mucha." .. owner .. ":IsTouchSavePoint"
    local ok, errorMessage = pcall(function()
        preId, postId = RegisterHook(hookPath, function() end, function()
            -- Only the save-point predicate is changed. Native menu/combat/event
            -- restrictions remain in place. An expired GUI lease disables it.
            control()
            if enabled then return true end
        end)
    end)
    ready = ok and preId ~= nil and postId ~= nil
    lastError = ready and "Field-save predicate hooked; test through the normal field menu" or tostring(errorMessage)
end

local function poll()
    if not attempted then
        local ok, message = pcall(installHook)
        if not ok then lastError = tostring(message) end
    end
    control()
    status()
end

-- Current UE4SS runs this callback on the game thread. The fallback is for
-- older compatible loaders; all reflection work is still scheduled there.
if LoopInGameThreadWithDelay then
    LoopInGameThreadWithDelay(500, poll)
else
    LoopAsync(500, function()
        ExecuteInGameThread(poll)
        return false
    end)
end
