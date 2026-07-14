-- file: eco_restoration_shard/cyboquatic_progress/20260712/lua/cyboquatic_governance_particle.lua
local M = {}

local function clamp01(x)
  if x < 0 then return 0 end
  if x > 1 then return 1 end
  return x
end

function M.build_particle(did, crate_id, domain, subtask_id, evidence_hex, k, e, r, vt)
  return {
    did = did,
    crate_id = crate_id,
    domain = domain,
    subtask_id = subtask_id,
    evidence_hex = evidence_hex,
    k_score = clamp01(k),
    e_score = clamp01(e),
    r_score = clamp01(r),
    lyapunov_vt = math.max(0, vt),
    sql = string.format(
      "INSERT INTO daily_progress (yyyymmdd, crateid, domain, subtaskid, nodeid, sampleid, timestamputc, evidencehex, kfactor, efactor, rfactor, priorcrateid, didbound, vtafter) VALUES ('20260712','%s','%s','%s','PHX-GOV-NODE-01','PHX-GOV-SAMPLE-0001','2026-07-12T23:31:00Z','%s',%.6f,%.6f,%.6f,'cyboquatic_governance_particle_20260711','%s',%.6f);",
      crate_id, domain, subtask_id, evidence_hex, clamp01(k), clamp01(e), clamp01(r), did, math.max(0, vt)
    )
  }
end

return M
