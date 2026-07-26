-- File: ecorestorationshard/tools/lane_lyapunov_timed_automaton.lua

--[[
39. Multi‑tiered Lyapunov corridor with “safe”, “warning”, and “emergency” bands,
expressed as a timed automaton governing lane transitions from EXP to PROD.

This Lua module defines:
  - V_t bands for a workload lane: SAFE, WARNING, EMERGENCY.
  - A timed automaton with states:
      EXP        (experimental lane)
      SAFE_STREAK (EXP lane undergoing 30‑day observation)
      PROD       (production lane)
      EMERG      (emergency lane for V_t > emergency_max)
  - Transition rules:
      * EXP -> SAFE_STREAK when lane enters observation.
      * SAFE_STREAK -> PROD only if V_t stays strictly within SAFE band
        for 30 consecutive days.
      * SAFE_STREAK -> EXP if any day leaves SAFE band.
      * Any lane state -> EMERG if V_t leaves EMERGENCY band.
      * EMERG -> EXP after mitigation and reset.

Non‑actuating: this code only evaluates state transitions; it does not
control physical machinery. It can be embedded in CI, monitoring, or
governance layers as a diagnostic automaton.
]]--

local Automaton = {}

-- Corridor parameters: bands for V_t.
local Corridor = {
  safe_min      = 0.0,
  safe_max      = 0.5,
  warning_min   = 0.5,
  warning_max   = 0.8,
  emergency_min = 0.8,
  emergency_max = math.huge
}

-- Lane states for the automaton.
local LaneState = {
  EXP         = "EXP",
  SAFE_STREAK = "SAFE_STREAK",
  PROD        = "PROD",
  EMERG       = "EMERG"
}

-- Encapsulate lane automaton state.
local LaneAutomaton = {}
LaneAutomaton.__index = LaneAutomaton

-- Constructor for a lane automaton instance.
function LaneAutomaton.new(lane_id, initial_state)
  local self = setmetatable({}, LaneAutomaton)
  self.lane_id = lane_id or "UNKNOWN_LANE"
  self.state = initial_state or LaneState.EXP
  self.safe_streak_days = 0       -- counts consecutive days in SAFE band
  self.last_update_yyyymmdd = nil -- last date processed
  return self
end

-- Classify V_t into corridor band.
local function classify_vt(vt)
  if vt >= Corridor.safe_min and vt <= Corridor.safe_max then
    return "SAFE"
  elseif vt > Corridor.warning_min and vt <= Corridor.warning_max then
    return "WARNING"
  elseif vt > Corridor.emergency_min then
    return "EMERGENCY"
  else
    -- Below safe_min is treated as SAFE for this corridor (improvement).
    return "SAFE"
  end
end

-- Timed automaton update function.
--
-- Inputs:
--   yyyymmdd : string date
--   vt       : residual V_t for lane on that date
--
-- Effects:
--   - Updates self.state and self.safe_streak_days.
--   - Returns the new state plus a descriptive transition label.
function LaneAutomaton:update(yyyymmdd, vt)
  local band = classify_vt(vt)
  local prev_state = self.state
  local transition_label = "NO_TRANSITION"

  -- Emergency override: any band classified as EMERGENCY promotes lane
  -- into EMERG state immediately.
  if band == "EMERGENCY" then
    self.state = LaneState.EMERG
    self.safe_streak_days = 0
    transition_label = "ANY->EMERG"
    self.last_update_yyyymmdd = yyyymmdd
    return self.state, transition_label
  end

  -- If lane is in EMERG, it remains until external mitigation resets
  -- it back to EXP. This module does not auto‑recover.
  if self.state == LaneState.EMERG then
    self.last_update_yyyymmdd = yyyymmdd
    return self.state, transition_label
  end

  -- State machine for EXP and SAFE_STREAK.
  if self.state == LaneState.EXP then
    -- Experimental lane; a governance action may initiate a SAFE_STREAK
    -- observation window. Here we model that any SAFE day can be treated
    -- as starting the SAFE_STREAK window.
    if band == "SAFE" then
      self.state = LaneState.SAFE_STREAK
      self.safe_streak_days = 1
      transition_label = "EXP->SAFE_STREAK_START"
    else
      -- WARNING band in EXP lane: remain EXP, no promotion.
      self.safe_streak_days = 0
    end
  elseif self.state == LaneState.SAFE_STREAK then
    if band == "SAFE" then
      self.safe_streak_days = self.safe_streak_days + 1
      if self.safe_streak_days >= 30 then
        -- Promotion rule: only if V_t has stayed in SAFE for 30 consecutive days.
        self.state = LaneState.PROD
        transition_label = "SAFE_STREAK(30)->PROD"
      else
        transition_label = "SAFE_STREAK_CONTINUE"
      end
    else
      -- Any non‑SAFE day resets the observation and returns the lane to EXP.
      self.state = LaneState.EXP
      self.safe_streak_days = 0
      transition_label = "SAFE_STREAK_BREAK->EXP"
    end
  elseif self.state == LaneState.PROD then
    -- Production lane; SAFE and WARNING bands are allowed, but WARNING
    -- may trigger governance alerts. Leaving SAFE/WARNING for EMERGENCY
    -- was already handled above.
    if band == "SAFE" then
      transition_label = "PROD_SAFE"
    elseif band == "WARNING" then
      transition_label = "PROD_WARNING"
    end
  end

  self.last_update_yyyymmdd = yyyymmdd
  return self.state, transition_label
end

-- External mitigation hook: reset lane from EMERG back to EXP after
-- governance confirms remediation.
function LaneAutomaton:reset_from_emergency()
  if self.state == LaneState.EMERG then
    self.state = LaneState.EXP
    self.safe_streak_days = 0
    return true
  end
  return false
end

-- Pretty printer for automaton state.
function LaneAutomaton:describe()
  return string.format(
    "Lane[%s] state=%s safe_streak_days=%d last_update=%s",
    self.lane_id,
    self.state,
    self.safe_streak_days,
    self.last_update_yyyymmdd or "n/a"
  )
end

-- Exported API.
Automaton.Corridor    = Corridor
Automaton.LaneState   = LaneState
Automaton.LaneAutomaton = LaneAutomaton

-- Demonstration: can be removed or wrapped in tests in CI.
local function demo()
  local lane = LaneAutomaton.new("PHX-MAR-LANE-01", LaneState.EXP)

  -- Simulate 35 days of SAFE band V_t.
  for day = 1, 35 do
    local date = string.format("202608%02d", day)
    local vt   = 0.3 -- within SAFE band
    local state, label = lane:update(date, vt)
    print(date, vt, state, label)
  end

  print(lane:describe())

  -- Inject an emergency day.
  local state, label = lane:update("20260901", 0.95)
  print("EMERG DAY", state, label)
  print(lane:describe())

  -- Reset from emergency after mitigation.
  local reset_ok = lane:reset_from_emergency()
  print("RESET_FROM_EMERG", reset_ok, lane:describe())
end

if ... == nil then
  -- Run demo only when script is executed directly.
  demo()
end

return Automaton
