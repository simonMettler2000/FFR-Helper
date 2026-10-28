-- Native FFR research recorder. Observation only: no setters, overrides,
-- save/load calls, actor construction, console execution, or memory writes.
-- An observation is deliberately NOT represented as a restorable save state.
local script = debug.getinfo(1, "S").source:gsub("^@", "")
local root = script:gsub("Scripts[/\\]main%.lua$", "")
local connection = root .. "connection/"
local session, request, expires = nil, nil, 0
local events, hooks = {}, {}
local sequence, lastRequest, nextDiscovery = 0, nil, 0
local message = "Waiting for desktop connection"
local specs = {
    {owner="CPP_MapManager", name="Encount", event="encounter-opening"},
    {owner="CPP_BFL_Battle", name="EndBattle", event="battle-end-request"},
    {owner="CPP_BtlUnitAI", name="ExecuteOnBeginBattleInner", event="enemy-battle-opening"}
}

local function encode(value, depth)
    depth = depth or 0
    if depth > 16 then error("Observation nesting exceeded") end
    local kind = type(value)
    if kind == "nil" then return "null" end
    if kind == "boolean" then return tostring(value) end
    if kind == "number" then
        if value ~= value or value == math.huge or value == -math.huge then return "null" end
        return tostring(value)
    end
    if kind == "string" then
        return '"' .. value:gsub('[%z\1-\31\\"]', function(c)
            if c == '"' then return '\\"' end
            if c == '\\' then return '\\\\' end
            return string.format('\\u%04x', string.byte(c))
        end) .. '"'
    end
    if kind ~= "table" then error("Only detached observation values can be exported") end
    local pieces = {}
    if rawget(value, "__array") then
        for index=1,#value do pieces[#pieces+1] = encode(value[index], depth+1) end
        return "[" .. table.concat(pieces, ",") .. "]"
    end
    for key, item in pairs(value) do
        if type(key) ~= "string" then error("Observation object keys must be strings") end
        pieces[#pieces+1] = encode(key, depth+1) .. ":" .. encode(item, depth+1)
    end
    table.sort(pieces)
    return "{" .. table.concat(pieces, ",") .. "}"
end

local function array() return {__array=true} end
local function valid(object)
    local ok, result = pcall(function() return object ~= nil and object:IsValid() end)
    return ok and result == true
end
local function get(object, name)
    local ok, value = pcall(function() return object[name] end)
    if ok then return value end
end
local function primitive(value)
    local kind = type(value)
    if kind == "boolean" or kind == "number" then return value end
end
local function fields(object, names)
    local values = {}
    for _, name in ipairs(names) do values[name] = primitive(get(object, name)) end
    return values
end
local function count(value)
    local ok, result = pcall(function() return value:GetArrayNum() end)
    if ok and type(result) == "number" and result >= 0 then return result end
end
local function visit(value, maximum, callback)
    local size = count(value)
    if not size or size > maximum then return false end
    local ok = pcall(function()
        value:ForEach(function(index, element)
            -- The source array may change between ticks; all values are detached
            -- synchronously on the game thread, never retained as remote wrappers.
            callback(element:get(), index)
        end)
    end)
    return ok
end
local function integer_array(value, maximum)
    local values = array()
    local ok = visit(value, maximum, function(item)
        if type(item) ~= "number" or item % 1 ~= 0 then error("Expected integer array") end
        values[#values+1] = item
    end)
    if ok then return values end
end

local function lease()
    local file = io.open(connection .. "observe.txt", "rb")
    if not file then expires = 0; return false end
    local raw = file:read(256) or ""
    file:close()
    local nextSession, expiry, nextRequest = raw:match(
        "^FFR_HELPER_OBSERVER_V1\n([a-f0-9]+)\n(%d+)\n([a-f0-9]+)\n$")
    local now = os.time()
    if not nextSession or #nextSession ~= 32 or #nextRequest ~= 32
            or tonumber(expiry) < now or tonumber(expiry) > now+8 then
        expires = 0
        return false
    end
    if session ~= nextSession then
        -- A new desktop connection cannot inherit a previous session's history.
        events, sequence, lastRequest = {}, 0, nil
    end
    session, request, expires = nextSession, nextRequest, tonumber(expiry)
    return true
end

local function write(name, value)
    local raw = encode(value)
    if #raw > 524288 then error("Observation exceeds output limit") end
    local file = io.open(connection .. name .. ".tmp", "wb")
    if not file then error("Recorder directory is not writable") end
    file:write(raw)
    file:close()
    os.remove(connection .. name)
    local ok = os.rename(connection .. name .. ".tmp", connection .. name)
    if not ok then error("Unable to publish observation") end
end

local function unique(className)
    local candidates = {}
    local ok, found = pcall(FindAllOf, className)
    if not ok then return nil, "unavailable" end
    for _, object in ipairs(found or {}) do
        local ok, eligible = pcall(function()
            if not valid(object) or not object:IsA("/Script/mucha." .. className) then return false end
            local name = object:GetFName():ToString()
            if name:find("^Default__") or name:find("^SKEL_") or name:find("^REINST_") then return false end
            -- World subsystems must belong to a live world; the save manager is
            -- a GameInstanceSubsystem and is selected by unique active instance.
            if className ~= "CPP_MuchaSaveDataManager" and not valid(object:GetWorld()) then return false end
            return true
        end)
        if ok and eligible then candidates[#candidates+1] = object end
    end
    if #candidates == 1 then return candidates[1], "unique" end
    return nil, #candidates == 0 and "not-loaded" or "ambiguous"
end

local function unit_record(unit)
    local parameter = get(unit, "m_pUnitParameter")
    if not valid(parameter) then return {available=false} end
    local out = {available=true, values={}}
    for _, name in ipairs({"GetUnitId", "GetLevel", "GetHitPoint", "GetMagicPoint", "GetBreakPoint",
            "GetLimitBurstPoint", "GetMaxHitPoint", "GetMaxMagicPoint", "GetMaxBreakPoint",
            "GetActionCountInBattle", "GetThinkNum", "GetBreakNum", "GetReciveDamageNum"}) do
        local ok, value = pcall(function() return parameter[name](parameter) end)
        if ok then out.values[name] = primitive(value) end
    end
    local ai = get(unit, "m_pAI")
    if valid(ai) then
        out.ai = fields(ai, {"m_SkillId", "m_ScheduledSkillId", "m_StatusChangedSkillId"})
        out.ai.targetCount = count(get(ai, "m_TargetList"))
        out.ai.skillStackCount = count(get(ai, "m_SkillStackData"))
        local ok, turn = pcall(function() return ai:GetTurnNum() end)
        if ok then out.ai.turn = primitive(turn) end
    end
    return out
end

local function observe()
    local objects = {}
    local ui, state = unique("CPP_UI_BattleSubsystem")
    objects.battleUI = {availability=state}
    if ui then objects.battleUI.values = fields(ui, {"bDuringBattle", "BtlPhaseType", "CurrentActionUnitUniqueId", "FocusSkillId"}) end
    local manager
    manager, state = unique("CPP_BtlManager")
    objects.battle = {availability=state}
    if manager then
        local unitManager = get(manager, "m_pUnitManager")
        local units = array()
        objects.battle.units = units
        objects.battle.unitsReadable = visit(get(unitManager, "m_UnitList"), 64, function(unit)
            units[#units+1] = unit_record(unit)
        end)
        local scheduler = get(unitManager, "m_pTimelineScheduler")
        objects.battle.additionalTimelineCount = count(get(scheduler, "m_AdditionalTimeline"))
        local jobs = get(manager, "m_pBattleJobManager")
        objects.battle.waitingJobCount = count(get(jobs, "m_WaitingJobList"))
        objects.battle.workingJobCount = count(get(jobs, "m_WorkingJobList"))
    end
    local saves
    saves, state = unique("CPP_MuchaSaveDataManager")
    objects.progression = {availability=state}
    if saves then
        local saved = get(saves, "m_pGameSaveData")
        objects.progression.saveObjectAvailable = valid(saved)
        if valid(saved) then
            objects.progression.unitCount = count(get(saved, "unitParameterList"))
            objects.progression.inventoryCount = count(get(saved, "inventory"))
            objects.progression.visionCount = count(get(saved, "visionList"))
        end
    end
    return objects
end

local function entry_record(entry)
    local out = fields(entry, {"canEscape", "probabilityOfSuccessfulEscape", "isResultSkipOnWin",
        "isResultSkipOnLose", "battleGroupId", "battleStageId", "battleEventId", "ap", "regionId",
        "preemptiveRelation", "isNotifyPreemtiveAttack", "isStartupByBattleTest", "isContinueBGM", "isBounty"})
    for _, name in ipairs({"friendliesUnitIdList", "friendliesLocationIdList", "enemiesUnitIdList", "enemiesLocationIdList"}) do
        out[name] = integer_array(get(entry, name), 64)
    end
    return out
end

local function record(spec, context, entry)
    if not lease() then return end
    sequence = sequence+1
    local event = {sequence=sequence, kind=spec.event, time=os.time()}
    if spec.event == "encounter-opening" then
        local ok, value = pcall(function() return entry_record(entry:get()) end)
        if ok then event.entry = value else event.entryUnavailable = true end
    end
    -- Keeping detached data prevents engine object lifetimes from invalidating
    -- older reports. This is observation only, not a battle-opening restore.
    events[#events+1] = event
    if #events > 32 then table.remove(events, 1) end
end

local function discover()
    if os.time() < nextDiscovery then return end
    nextDiscovery = os.time()+5
    for _, spec in ipairs(specs) do
        local path = "/Script/mucha." .. spec.owner .. ":" .. spec.name
        if hooks[path] ~= "observing" then
            local fn = StaticFindObject(path)
            if valid(fn) then
                -- Both callbacks always return nil; engine results and arguments
                -- are never overridden, even when the recorder is disconnected.
                local ok, pre, post = pcall(function()
                    return RegisterHook(path, function(context, entry)
                        pcall(record, spec, context, entry)
                    end, function() end)
                end)
                hooks[path] = ok and pre ~= nil and post ~= nil and "observing" or "unavailable"
            else
                hooks[path] = "not-loaded"
            end
        end
    end
end

local function poll()
    if not lease() then return end
    local ok = pcall(discover)
    if not ok then message = "Function discovery unavailable; object observation may still work"
    else message = "Read-only recorder connected; game compatibility is not yet verified" end
    if request ~= string.rep("0", 32) and request ~= lastRequest then
        local captured = pcall(function()
            local report = {
            protocol=1, adapter="ffr-helper-runtime-observer", session=session, request=request, time=os.time(),
            format="ffr-helper-runtime-report", version=1, kind="diagnostic-observation", readOnly=true, restorable=false,
            objects=observe(), events=array(), missingState=array()
            }
            for _, event in ipairs(events) do report.events[#report.events+1] = event end
            for _, missing in ipairs({"Complete native battle manager and timeline state", "Pending jobs, latent events and callbacks",
                "Complete enemy AI and summoned/dead unit lifecycle", "Native RNG state and draw order",
                "World/story event state and full progression payload", "Engine threads, rendering and external device state"}) do
                report.missingState[#report.missingState+1] = missing
            end
            write("runtime-report.json", report)
        end)
        if not captured then message = "Capture failed; request a new report when the field/battle is stable. No game state was changed" end
        lastRequest = request
    end
    write("observer-status.json", {protocol=1, adapter="ffr-helper-runtime-observer", session=session,
        time=os.time(), readOnly=true, hooks=hooks, message=message})
end

local function safe_poll()
    local ok = pcall(poll)
    if not ok then message = "Observation failed; no game state was changed" end
end
if LoopInGameThreadWithDelay then
    LoopInGameThreadWithDelay(500, safe_poll)
else
    LoopAsync(500, function() ExecuteInGameThread(safe_poll); return false end)
end
