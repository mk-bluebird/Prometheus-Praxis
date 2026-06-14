-- filename: lua/econet_overlay.lua
-- destination: ecorestoration_shard/lua/econet_overlay.lua
-- Non-actuating Lua overlay using tables and metatables for smart-simplicity summaries.

local EconetOverlay = {}

local RepoMethods = {}

function RepoMethods:to_chat_summary()
    local planes = ""
    if self.planes and #self.planes > 0 then
        planes = table.concat(self.planes, ", ")
    end
    local k = self.kerbandk or 0.0
    local e = self.kerbande or 0.0
    local r = self.kerbandr or 0.0
    local nonactuating = self.nonactuating and "true" or "false"

    return string.format(
        "Artifact %s in repo %s is a %s in role band %s for lane %s in region %s. " ..
        "Its KER targets are K=%.2f, E=%.2f, R=%.2f, and it touches planes [%s]. " ..
        "The artifact is marked non-actuating=%s and authored by %s.",
        self.logicalname or "",
        self.repotarget or "",
        self.artifactkind or "",
        self.roleband or "",
        self.lanedefault or "",
        self.regionscope or "",
        k,
        e,
        r,
        planes,
        nonactuating,
        self.authorbostrom or ""
    )
end

local RepoMeta = { __index = RepoMethods }

function EconetOverlay.new_repo_summary(row)
    local repo = {
        filename = row.filename,
        destination = row.destination,
        repotarget = row.repotarget,
        roleband = row.roleband,
        lanedefault = row.lanedefault,
        regionscope = row.regionscope,
        planes = row.planes or {},
        logicalname = row.logicalname,
        artifactkind = row.artifactkind,
        econscope = row.econscope,
        nonactuating = row.nonactuating == 1 or row.nonactuating == true,
        kerbandk = row.kerbandk or 0.0,
        kerbande = row.kerbande or 0.0,
        kerbandr = row.kerbandr or 0.0,
        authorbostrom = row.authorbostrom,
    }
    return setmetatable(repo, RepoMeta)
end

function EconetOverlay.rank_artifacts(rows, limit)
    table.sort(rows, function(a, b)
        if a.ecoimpactvalue == b.ecoimpactvalue then
            if a.riskofharmvalue == b.riskofharmvalue then
                return (a.carbon_impact_sum or 0.0) < (b.carbon_impact_sum or 0.0)
            end
            return (a.riskofharmvalue or 0.0) < (b.riskofharmvalue or 0.0)
        end
        return (a.ecoimpactvalue or 0.0) > (b.ecoimpactvalue or 0.0)
    end)
    local out = {}
    local n = limit or #rows
    if n > #rows then
        n = #rows
    end
    for i = 1, n do
        out[i] = rows[i]
    end
    return out
end

return EconetOverlay
