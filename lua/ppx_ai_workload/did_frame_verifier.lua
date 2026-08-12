-- File: lua/ppx_ai_workload/did_frame_verifier.lua
local openssl = require("openssl")
local pkey = openssl.pkey

local governance_did = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"

local function safe_text(value)
    return type(value) == "string" and value ~= "" and
        not value:find("[\r\n|]")
end

local function canonical_frame(frame)
    local numeric = {"k_knowledge", "e_eco_impact", "r_risk", "roh", "vt_current", "vt_next"}
    for _, key in ipairs(numeric) do
        if type(frame[key]) ~= "number" then return nil end
    end
    for _, key in ipairs({"frame_id", "owner_did", "timestamp_utc", "hex_anchor"}) do
        if not safe_text(tostring(frame[key])) then return nil end
    end
    return table.concat({
        "frame_id=" .. frame.frame_id,
        "owner_did=" .. frame.owner_did,
        "timestamp_utc=" .. frame.timestamp_utc,
        "hex_anchor=" .. frame.hex_anchor,
        string.format("K=%.17g", frame.k_knowledge),
        string.format("E=%.17g", frame.e_eco_impact),
        string.format("R=%.17g", frame.r_risk),
        string.format("RoH=%.17g", frame.roh),
        string.format("Vt=%.17g", frame.vt_current),
        string.format("VtNext=%.17g", frame.vt_next)
    }, "|")
end

local function verify_frame_signature(frame, signature, did_public_key)
    if type(frame) ~= "table" or frame.owner_did ~= governance_did or
        type(signature) ~= "string" or signature == "" or
        type(did_public_key) ~= "string" or did_public_key == "" then
        return false, "invalid_governance_frame"
    end

    local serialized = canonical_frame(frame)
    if not serialized then return false, "invalid_canonical_frame" end

    local key, key_error = pkey.read(did_public_key, true)
    if not key then return false, key_error or "invalid_public_key" end

    local verified, verification_error = pkey.verify(key, serialized, signature)
    if verified ~= true then return false, verification_error or "signature_verification_failed" end
    return true, "verified_governance_frame"
end

return {
    canonical_frame = canonical_frame,
    verify_frame_signature = verify_frame_signature
}
