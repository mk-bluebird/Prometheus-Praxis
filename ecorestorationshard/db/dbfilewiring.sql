-- filename: dbfilewiring.sql
-- destination: ecorestorationshard/db/dbfilewiring.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS file_wiring (
    wiring_id        INTEGER PRIMARY KEY AUTOINCREMENT,

    repofile_id      INTEGER NOT NULL,
    logicalname      TEXT    NOT NULL,
    definition_id    TEXT,

    lane_default     TEXT    NOT NULL CHECK (lane_default IN ('RESEARCH','EXPPROD','PROD')),
    ker_band_default TEXT    NOT NULL CHECK (ker_band_default IN ('SAFE','GUARDED','BLOCKED')),

    role_band        TEXT    NOT NULL,
    db_role          TEXT    NOT NULL,
    region           TEXT    NOT NULL,
    scope            TEXT    NOT NULL,

    author_bostrom   TEXT    NOT NULL,
    contract_id      TEXT    NOT NULL,

    created_utc      TEXT    NOT NULL,
    updated_utc      TEXT    NOT NULL,

    FOREIGN KEY (repofile_id)
        REFERENCES repofile(fileid)
        ON DELETE CASCADE,

    FOREIGN KEY (definition_id)
        REFERENCES definitionregistryrestoration(defid)
        ON DELETE SET NULL,

    UNIQUE (repofile_id, logicalname)
);

CREATE INDEX IF NOT EXISTS idx_file_wiring_logical
    ON file_wiring (logicalname, region, scope);

CREATE INDEX IF NOT EXISTS idx_file_wiring_lane
    ON file_wiring (lane_default, ker_band_default, db_role);


CREATE VIEW IF NOT EXISTS vfilewiring_consistency AS
SELECT
    fw.wiring_id,
    fw.logicalname,
    fw.region,
    fw.scope,
    fw.db_role,
    fw.lane_default,
    fw.ker_band_default,
    rf.relpath         AS file_path,
    rf.purpose         AS file_purpose,
    dr.status          AS definition_status,
    dr.versiontag      AS definition_version,
    rib.bostromaddress AS bound_bostromaddress,
    rib.contractid     AS bound_contractid
FROM file_wiring AS fw
JOIN repofile AS rf
  ON rf.fileid = fw.repofile_id
LEFT JOIN definitionregistryrestoration AS dr
  ON dr.logicalname = fw.logicalname
LEFT JOIN restorationidentitybinding AS rib
  ON rib.logicalname = fw.logicalname
 AND rib.filepath    = rf.relpath;


CREATE VIEW IF NOT EXISTS vfilewiring_missing_definition AS
SELECT *
FROM vfilewiring_consistency
WHERE definition_status IS NULL;


CREATE VIEW IF NOT EXISTS vfilewiring_missing_identity AS
SELECT *
FROM vfilewiring_consistency
WHERE bound_bostromaddress IS NULL;
