-- ALE-GOV-BOSTROM-CONTRACT-BINDING-001.sql
-- Bostrom DID binding to cybo_workload_window provenance.

CREATE TABLE IF NOT EXISTS brainidentitybinding2026v1 (
    bindingid        TEXT PRIMARY KEY,
    stakeholderid    TEXT NOT NULL,
    bostromdid       TEXT NOT NULL,
    identityhex      TEXT NOT NULL,
    brainbindinghex  TEXT NOT NULL,
    continuityhash   TEXT NOT NULL,
    rohanchorhex     TEXT NOT NULL,
    evidencehex      TEXT NOT NULL,
    signinghex       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS cybo_workload_window (
    windowid         TEXT PRIMARY KEY,
    nodeid           TEXT NOT NULL,
    region           TEXT NOT NULL,
    Ei               REAL NOT NULL,
    Ci               REAL NOT NULL,
    Si               REAL NOT NULL,
    Ki               REAL NOT NULL,
    evidencehex      TEXT NOT NULL,
    created_at_s     INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS bostromcontractbinding (
    bindingid              TEXT NOT NULL,
    stakeholderid          TEXT NOT NULL,
    workload_window_id     TEXT NOT NULL,
    cybo_workload_hashhex  TEXT NOT NULL,
    identityhex            TEXT NOT NULL,
    continuityhash         TEXT NOT NULL,
    evidencehex            TEXT NOT NULL,
    signinghex             TEXT NOT NULL,
    PRIMARY KEY (bindingid, workload_window_id),
    FOREIGN KEY (bindingid) REFERENCES brainidentitybinding2026v1(bindingid) ON DELETE RESTRICT,
    FOREIGN KEY (workload_window_id) REFERENCES cybo_workload_window(windowid) ON DELETE RESTRICT,
    CHECK (stakeholderid = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')
);

-- BEFORE INSERT: cryptographic anchoring checks.
CREATE TRIGGER IF NOT EXISTS trg_bostromcontractbinding_anchor
BEFORE INSERT ON bostromcontractbinding
FOR EACH ROW
BEGIN
    -- Verify cybo_workload_hashhex matches hash(workload_window row).
    -- (Assumes a user-defined function workload_hash(windowid) exposed from Rust.)
    IF NEW.cybo_workload_hashhex != workload_hash(NEW.workload_window_id) THEN
        SELECT RAISE(ABORT, 'Bostrom binding: workload hash mismatch');
    END IF;

    -- Verify identityhex and continuityhash align with brainidentitybinding2026v1.
    IF (SELECT identityhex FROM brainidentitybinding2026v1 WHERE bindingid = NEW.bindingid) != NEW.identityhex THEN
        SELECT RAISE(ABORT, 'Bostrom binding: identityhex mismatch');
    END IF;

    IF (SELECT continuityhash FROM brainidentitybinding2026v1 WHERE bindingid = NEW.bindingid) != NEW.continuityhash THEN
        SELECT RAISE(ABORT, 'Bostrom binding: continuityhash mismatch');
    END IF;
END;
