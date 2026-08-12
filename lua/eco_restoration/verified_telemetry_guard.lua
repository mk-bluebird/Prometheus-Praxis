-- File: lua/eco_restoration/verified_telemetry_guard.lua

local sqlite3 = require("lsqlite3")

local expected_did = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"

local function verified_frame(database_path, frame_id, owner_did)
    if type(frame_id) ~= "string" or frame_id == "" or owner_did ~= expected_did then
        return false, "invalid_frame_identity"
    end

    local database = assert(sqlite3.open(database_path))
    local statement = assert(database:prepare(
        "SELECT state FROM telemetry_verification_state " ..
        "WHERE frame_id=? AND owner_did=? LIMIT 1;"
    ))
    statement:bind_values(frame_id, owner_did)
    local row = statement:nrows()()
    statement:finalize()
    database:close()

    if not row or row.state ~= "VERIFIED" then
        return false, "verification_state_absent"
    end
    return true, "verified"
end

return { verified_frame = verified_frame }
