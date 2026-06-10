vrmod = vrmod or {}
vrmod.logger = vrmod.logger or {}
vrmod.debug_cvars = vrmod.debug_cvars or {}
-- ConVars
local cv_console = CreateConVar("vrmod_log_console", "0", FCVAR_REPLICATED + FCVAR_ARCHIVE, "Minimum log level for console (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)")
local cv_file = CreateConVar("vrmod_log_file", "0", FCVAR_REPLICATED + FCVAR_ARCHIVE, "Minimum log level for file (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)")
local cv_console_rate = CreateConVar("vrmod_log_console_maxrate", "0", FCVAR_ARCHIVE, "Max console messages per second (0 = unlimited). Good for VR pipeline.")
-- Subsystem convars
for subsystem, _ in pairs(vrmod.status or {}) do
    if subsystem ~= "api" then
        local name = "vrmod_debug_" .. subsystem
        if not vrmod.debug_cvars[subsystem] then vrmod.debug_cvars[subsystem] = CreateConVar(name, "0", FCVAR_ARCHIVE + FCVAR_REPLICATED, "Enable debug for VRMod subsystem: " .. subsystem) end
    end
end

local levels = {
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    DEBUG = 4
}

local levelColors = {
    DEBUG = Color(150, 150, 150),
    INFO = Color(0, 200, 0),
    WARN = Color(255, 200, 0),
    ERROR = Color(255, 0, 0),
}

local sideColors = {
    SERVER = Color(0, 150, 255),
    CLIENT = Color(255, 100, 255),
}

local subsystemColors = {}
local function getSubsystemColor(name)
    if subsystemColors[name] then return subsystemColors[name] end
    local col = Color(math.random(50, 255), math.random(50, 255), math.random(50, 255))
    subsystemColors[name] = col
    return col
end

local function tostr(v)
    if istable(v) then
        local parts = {}
        for k, val in pairs(v) do
            table.insert(parts, tostring(k) .. "=" .. tostr(val))
        end
        return "{" .. table.concat(parts, ", ") .. "}"
    elseif IsEntity and IsEntity(v) and v:IsValid() then
        return string.format("Entity[%s:%s]", v:GetClass(), v:EntIndex())
    elseif IsEntity and IsEntity(v) then
        return "Entity[INVALID]"
    elseif isvector and isvector(v) then
        return string.format("Vector(%.2f, %.2f, %.2f)", v.x, v.y, v.z)
    elseif isangle and isangle(v) then
        return string.format("Angle(%.2f, %.2f, %.2f)", v.p, v.y, v.r)
    else
        return tostring(v)
    end
end

local function detectSubsystem()
    local lvl = 3
    while true do
        local info = debug.getinfo(lvl, "S")
        if not info then break end
        if info.short_src then
            local src = string.lower(info.short_src)
            if src:find("vrmod/") and not src:find("vrmod/logger.lua") then
                local match = src:match("vrmod/([^/]+)/[^/]+%.lua$")
                if match and vrmod.status and vrmod.status[match] ~= nil then return match end
            end
        end

        lvl = lvl + 1
    end
    return "unknown"
end

-- === FILE INIT + NON-BLOCKING QUEUE ===
local logDir = "vrmod_logs"
if not file.Exists(logDir, "DATA") then file.CreateDir(logDir) end
local side = SERVER and "SERVER" or "CLIENT"
local logFileName = string.format("%s/vrmod_%s_%s.txt", logDir, side:lower(), os.date("%Y%m%d_%H%M%S"))
local logQueue = {}
local function flushLogQueue()
    if #logQueue == 0 then return end
    file.Append(logFileName, table.concat(logQueue, "\n") .. "\n")
    logQueue = {}
end

local function queueFileLine(line)
    table.insert(logQueue, line)
    if #logQueue >= 50 then flushLogQueue() end
end

timer.Remove("vrmod_logger_flush")
timer.Create("vrmod_logger_flush", 0.25, 0, flushLogQueue)
hook.Add("ShutDown", "vrmod_logger_shutdown", flushLogQueue)
-- === CORE LOG FUNCTION ===
local lastConsoleTime = 0
local function Log(level, fmt, ...)
    local lvlNum = levels[level]
    if not lvlNum then return end
    local subsystem = detectSubsystem()
    if lvlNum == levels.DEBUG then
        local cv = vrmod.debug_cvars[subsystem]
        if not (cv and cv:GetBool()) then return end
    end

    -- Build message
    local msg
    if select("#", ...) > 0 then
        local args = {...}
        for i = 1, #args do
            args[i] = tostr(args[i])
        end

        if type(fmt) == "string" and fmt:find("%%") then
            msg = string.format(fmt, unpack(args))
        else
            table.insert(args, 1, fmt)
            msg = table.concat(args, " ")
        end
    else
        msg = tostr(fmt)
    end

    -- Console output (immediate, with optional rate limit)
    if lvlNum <= cv_console:GetInt() then
        local maxRate = cv_console_rate:GetInt()
        if maxRate > 0 then
            local now = SysTime()
            if now - lastConsoleTime < 1 / maxRate then
                return -- drop this console message to protect VR pipeline
            end

            lastConsoleTime = now
        end

        local colLvl = levelColors[level] or color_white
        local colSide = sideColors[side] or color_white
        local colSub = getSubsystemColor(subsystem)
        MsgC(colSide, "[VR][" .. side .. "]", colSub, "[" .. subsystem:upper() .. "]", colLvl, "[" .. level .. "]", color_white, msg .. "\n")
    end

    -- File output (non-blocking)
    if lvlNum <= cv_file:GetInt() then
        local line = string.format("[VR][%s][%s][%s] %s", side, subsystem:upper(), level, msg)
        queueFileLine(os.date("[%H:%M:%S] ") .. line)
    end
end

-- Public API
function vrmod.logger.Debug(fmt, ...)
    Log("DEBUG", fmt, ...)
end

function vrmod.logger.Info(fmt, ...)
    Log("INFO", fmt, ...)
end

function vrmod.logger.Warn(fmt, ...)
    Log("WARN", fmt, ...)
end

function vrmod.logger.Err(fmt, ...)
    Log("ERROR", fmt, ...)
end

function vrmod.logger.Flush()
    flushLogQueue()
end