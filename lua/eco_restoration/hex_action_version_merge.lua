-- File: lua/eco_restoration/hex_action_version_merge.lua

local sqlite3 = require("lsqlite3")

local function refresh_resolution(database, anchor)
    local actions = {}
    local statement = assert(database:prepare(
        "SELECT DISTINCT action FROM hex_action_concurrent_frontier WHERE hex_anchor=?"
    ))
    statement:bind_values(anchor)

    for row in statement:nrows() do
        actions[#actions + 1] = row.action
    end
    statement:finalize()

    local state = #actions == 1 and "RESOLVED" or "CONFLICT"
    local action = #actions == 1 and actions[1] or nil
    local resolution = assert(database:prepare(
        "INSERT INTO hex_action_resolution VALUES(?,?,?,unixepoch()) "
        "ON CONFLICT(hex_anchor) DO UPDATE SET action=excluded.action,state=excluded.state,"
        "resolved_unix_s=excluded.resolved_unix_s"
    ))
    resolution:bind_values(anchor, action, state)
    assert(resolution:step() == sqlite3.DONE)
    resolution:finalize()
end

local function merge(local_path, remote_path)
    local local_db = assert(sqlite3.open(local_path))
    assert(local_db:exec("ATTACH DATABASE '" .. remote_path:gsub("'", "''") .. "' AS remote;") == sqlite3.OK)
    assert(local_db:exec("BEGIN IMMEDIATE;") == sqlite3.OK)

    local import_changes = [[
        INSERT OR IGNORE INTO hex_action_change(hex_anchor,actor_did,counter,action,observed_unix_s)
        SELECT hex_anchor,actor_did,counter,action,observed_unix_s
        FROM remote.hex_action_change
        ORDER BY hex_anchor,actor_did,counter;
    ]]

    if local_db:exec(import_changes) ~= sqlite3.OK then
        local_db:exec("ROLLBACK;")
        local_db:close()
        error("hex action merge failed")
    end

    local anchors = {}
    for row in local_db:nrows("SELECT DISTINCT hex_anchor FROM hex_action_change;") do
        anchors[#anchors + 1] = row.hex_anchor
    end
    for _, anchor in ipairs(anchors) do
        refresh_resolution(local_db, anchor)
    end

    assert(local_db:exec("COMMIT;") == sqlite3.OK)
    local_db:close()
end

return { merge = merge }
