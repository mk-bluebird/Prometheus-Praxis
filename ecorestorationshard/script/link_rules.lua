-- link_rules.lua
-- Lightweight pattern matcher for URL intent classification and eco scoring.

local M = {}

function M.classify(url)
  local lower = string.lower(url)
  if string.find(lower, "phoenix.gov") or string.find(lower, "phoenixopendata.com") then
    return "CITY_GIS"
  elseif string.find(lower, "maricopa.gov") or string.find(lower, "data-maricopa.opendata.arcgis.com") then
    return "COUNTY_GIS"
  elseif string.find(lower, "azgfd.com") then
    return "STATE_WILDLIFE"
  elseif string.find(lower, "fws.gov") or string.find(lower, "ecos.fws.gov") then
    if string.find(lower, "wetlands") then
      return "FED_WETLANDS"
    else
      return "FED_CRIT_HABITAT"
    end
  else
    return "UNKNOWN"
  end
end

function M.eco_score(kind)
  if kind == "CITY_GIS" then
    return 0.70
  elseif kind == "COUNTY_GIS" then
    return 0.75
  elseif kind == "STATE_WILDLIFE" then
    return 0.82
  elseif kind == "FED_CRIT_HABITAT" then
    return 0.90
  elseif kind == "FED_WETLANDS" then
    return 0.88
  else
    return 0.50
  end
end

return M
