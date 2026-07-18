-- filename: ecorestoration_shard/cyboquatic_progress/20260717/lua/fog_router_predicates_20260717.lua
-- destination: ecorestoration_shard/cyboquatic_progress/20260717/lua/fog_router_predicates_20260717.lua
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

-- Cyboquatic FOG-router predicates for unmodeled media (Phoenix, 2026-07-17).
-- Non-actuating: computes classifications and diagnostics only. [file:2][file:12]

local json = {}

function json.encode(tbl)
  local function escape_str(s)
    s = s:gsub("\\", "\\\\")
    s = s:gsub("\"", "\\\"")
    s = s:gsub("\n", "\\n")
    s = s:gsub("\r", "\\r")
    return s
  end

  local function encode_value(v)
    local t = type(v)
    if t == "table" then
      local is_array = true
      local max_index = 0
      for k, _ in pairs(v) do
        if type(k) ~= "number" then
          is_array = false
          break
        else
          if k > max_index then
            max_index = k
          end
        end
      end
      if is_array then
        local parts = {}
        for i = 1, max_index do
          parts[#parts + 1] = encode_value(v[i])
        end
        return "[" .. table.concat(parts, ",") .. "]"
      else
        local parts = {}
        for k, vv in pairs(v) do
          parts[#parts + 1] = "\"" .. escape_str(tostring(k)) .. "\":" .. encode_value(vv)
        end
        return "{" .. table.concat(parts, ",") .. "}"
      end
    elseif t == "string" then
      return "\"" .. escape_str(v) .. "\""
    elseif t == "number" then
      return tostring(v)
    elseif t == "boolean" then
      return v and "true" or "false"
    elseif v == nil then
      return "null"
    else
      return "\"" .. escape_str(tostring(v)) .. "\""
    end
  end

  return encode_value(tbl)
end

-- Risk normalization helpers (0..1). Corridors informed by existing BOD/TSS/CEC work. [file:12]

local function clamp01(x)
  if x < 0.0 then return 0.0 end
  if x > 1.0 then return 1.0 end
  return x
end

local function normalize_bod(bod_mg_l)
  -- Example corridor bands:
  -- 0-150 mg/L: low risk (domestic graywater).
  -- 150-300 mg/L: corridor band.
  -- >300 mg/L: high risk. [file:12]
  if bod_mg_l <= 150.0 then
    return 0.1 * (bod_mg_l / 150.0)
  elseif bod_mg_l <= 300.0 then
    return 0.1 + 0.4 * ((bod_mg_l - 150.0) / 150.0)
  else
    local excess = bod_mg_l - 300.0
    return clamp01(0.5 + 0.5 * (excess / (excess + 300.0)))
  end
end

local function normalize_tss(tss_mg_l)
  -- 0-100 mg/L: low risk; 100-500 mg/L: corridor; >500 mg/L: high. [file:12]
  if tss_mg_l <= 100.0 then
    return 0.1 * (tss_mg_l / 100.0)
  elseif tss_mg_l <= 500.0 then
    return 0.1 + 0.5 * ((tss_mg_l - 100.0) / 400.0)
  else
    local excess = tss_mg_l - 500.0
    return clamp01(0.6 + 0.4 * (excess / (excess + 400.0)))
  end
end

local function normalize_cec(cec_index)
  -- CEC index from 0 (no concern) to e.g., 10 (high PFAS / pharmaceuticals). [file:12]
  if cec_index <= 1.0 then
    return 0.05 * cec_index
  elseif cec_index <= 5.0 then
    return 0.05 + 0.45 * ((cec_index - 1.0) / 4.0)
  else
    local excess = cec_index - 5.0
    return clamp01(0.5 + 0.5 * (excess / (excess + 5.0)))
  end
end

local function normalize_pfas(pfas_ng_l)
  -- PFAS tail-plane inspired by PFBS corridor work in Phoenix shards. [file:12][file:21]
  if pfas_ng_l <= 4.0 then
    return 0.1 * (pfas_ng_l / 4.0)
  elseif pfas_ng_l <= 20.0 then
    return 0.1 + 0.6 * ((pfas_ng_l - 4.0) / 16.0)
  else
    local excess = pfas_ng_l - 20.0
    return clamp01(0.7 + 0.3 * (excess / (excess + 20.0)))
  end
end

local function normalize_data_quality(rcalib, rsigma)
  -- Data-quality plane integrated into r vector (sensor health, calibration). [file:21]
  local r_c = clamp01(rcalib)
  local r_s = clamp01(rsigma)
  return clamp01(0.5 * r_c + 0.5 * r_s)
end

local function vt_residual(r_bod, r_tss, r_cec, r_pfas, r_dataq)
  -- Simple convex residual: Vt = Σ w_j * r_j^2 with heavier weight on PFAS and CEC. [file:2][file:21]
  local w_bod = 0.15
  local w_tss = 0.15
  local w_cec = 0.25
  local w_pfas = 0.30
  local w_dq = 0.15
  return w_bod * r_bod * r_bod
    + w_tss * r_tss * r_tss
    + w_cec * r_cec * r_cec
    + w_pfas * r_pfas * r_pfas
    + w_dq * r_dataq * r_dataq
end

local function classify_fog_segment(seg)
  -- seg: table with fields bod_mg_l, tss_mg_l, cec_index, pfas_ng_l, rcalib, rsigma, energy_req_j, vt_prev. [file:12][file:13]
  local r_bod = normalize_bod(seg.bod_mg_l or 0.0)
  local r_tss = normalize_tss(seg.tss_mg_l or 0.0)
  local r_cec = normalize_cec(seg.cec_index or 0.0)
  local r_pfas = normalize_pfas(seg.pfas_ng_l or 0.0)
  local r_dataq = normalize_data_quality(seg.rcalib or 0.0, seg.rsigma or 0.0)

  local vt = vt_residual(r_bod, r_tss, r_cec, r_pfas, r_dataq)
  local energy_req_j = seg.energy_req_j or 0.0

  local category
  local ker_band

  if vt <= 0.10 and energy_req_j <= 1.0e5 then
    category = "FOG_SAFE_CORRIDOR"
    ker_band = "K_HIGH_E_HIGH_R_LOW"
  elseif vt <= 0.25 and energy_req_j <= 5.0e5 then
    category = "FOG_MONITORED"
    ker_band = "K_MEDIUM_E_MEDIUM_R_MEDIUM"
  else
    category = "FOG_UNSAFE_DIAGNOSTIC_ONLY"
    ker_band = "K_MEDIUM_E_LOW_R_HIGH"
  end

  return {
    r_bod = r_bod,
    r_tss = r_tss,
    r_cec = r_cec,
    r_pfas = r_pfas,
    r_dataq = r_dataq,
    vt_residual = vt,
    energy_req_j = energy_req_j,
    category = category,
    ker_band = ker_band
  }
end

local function read_number(arg)
  local n = tonumber(arg)
  if n == nil then
    return 0.0
  end
  return n
end

local function main(args)
  if #args < 7 then
    io.stderr:write("Usage: lua fog_router_predicates_20260717.lua <bod_mg_l> <tss_mg_l> <cec_index> <pfas_ng_l> <rcalib> <rsigma> <energy_req_j>\n")
    os.exit(1)
  end

  local seg = {
    bod_mg_l = read_number(args[1]),
    tss_mg_l = read_number(args[2]),
    cec_index = read_number(args[3]),
    pfas_ng_l = read_number(args[4]),
    rcalib = clamp01(read_number(args[5])),
    rsigma = clamp01(read_number(args[6])),
    energy_req_j = read_number(args[7]),
    vt_prev = 0.0
  }

  local res = classify_fog_segment(seg)
  print(json.encode(res))
end

if pcall(debug.getlocal, 4, 1) == false then
  main(arg)
end
