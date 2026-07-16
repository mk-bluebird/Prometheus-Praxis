-- ker_composition_validator.lua
-- Lightweight Lua validator for KERComposition2026v1 invariants.
-- Non-actuating: used only for offline integrity checks.

local M = {}

-- theta is corridor-level risk cap; keep default consistent with ALN spec.
local THETA = 0.30

-- Expect composition row as:
-- {
--   left = { K=..., E=..., R=..., lane="RESEARCH|PILOT|PROD" },
--   right = { K=..., E=..., R=..., lane="..." },
--   comp = {
--     Kcombined = ...,
--     Ecombined = ...,
--     Rcombined = ...,
--     members   = "idmin,idmax",
--     ruleid    = "keroplusgeomminmaxv1",
--     lane      = "RESEARCH|PILOT|PROD",
--     evidencehex = "...", -- optional
--   }
-- }

local function kercombineriskcap(left, right, comp)
  if left.R <= THETA and right.R <= THETA then
    return comp.Rcombined <= THETA
  end
  return true
end

local function kercombineKEbounds(left, right, comp)
  local kmin = math.min(left.K, right.K)
  local kmax = math.max(left.K, right.K)
  if comp.Kcombined < kmin then
    return false
  end
  if comp.Kcombined > kmax then
    return false
  end
  if comp.Ecombined > left.E then
    return false
  end
  if comp.Ecombined > right.E then
    return false
  end
  return true
end

local function kercombineRmonotone(left, right, comp)
  if comp.Rcombined < left.R then
    return false
  end
  if comp.Rcombined < right.R then
    return false
  end
  return true
end

local function kercombinelanesafety(left, right, comp)
  if comp.lane == "PROD" then
    if left.lane ~= "PROD" or right.lane ~= "PROD" then
      return false
    end
  end
  return true
end

-- Provenance invariants: in this offline Lua checker we only require
-- that ruleid is "keroplusgeomminmaxv1" and members is non-empty,
-- because hashing is handled in a separate, signed layer.
local function kercombineprovenance(_, _, comp)
  if comp.ruleid ~= "keroplusgeomminmaxv1" then
    return false
  end
  if comp.members == nil or comp.members == "" then
    return false
  end
  return true
end

function M.validate(row)
  local left  = row.left
  local right = row.right
  local comp  = row.comp

  if left == nil or right == nil or comp == nil then
    return false
  end

  if not kercombineriskcap(left, right, comp) then
    return false
  end
  if not kercombineKEbounds(left, right, comp) then
    return false
  end
  if not kercombineRmonotone(left, right, comp) then
    return false
  end
  if not kercombineprovenance(left, right, comp) then
    return false
  end
  if not kercombinelanesafety(left, right, comp) then
    return false
  end

  return true
end

return M
