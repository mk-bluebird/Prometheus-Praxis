-- File: sql/eco_restoration/threshold_timelock_governance.sql

CREATE TABLE IF NOT EXISTS governance_signer (
    signer_did TEXT PRIMARY KEY,
    active INTEGER NOT NULL CHECK(active IN (0,1))
) STRICT;

CREATE TABLE IF NOT EXISTS threshold_change_proposal (
    proposal_id INTEGER PRIMARY KEY,
    proposer_did TEXT NOT NULL REFERENCES governance_signer(signer_did),
    k_min REAL NOT NULL CHECK(k_min BETWEEN 0 AND 1),
    e_min REAL NOT NULL CHECK(e_min BETWEEN 0 AND 1),
    r_max REAL NOT NULL CHECK(r_max BETWEEN 0 AND 1),
    announced_unix_s INTEGER NOT NULL,
    executable_unix_s INTEGER NOT NULL,
    state TEXT NOT NULL CHECK(state IN ('PENDING','EXECUTED','REJECTED')),
    CHECK(executable_unix_s = announced_unix_s + 86400)
) STRICT;

CREATE TABLE IF NOT EXISTS threshold_change_approval (
    proposal_id INTEGER NOT NULL REFERENCES threshold_change_proposal(proposal_id),
    signer_did TEXT NOT NULL REFERENCES governance_signer(signer_did),
    approved_unix_s INTEGER NOT NULL,
    PRIMARY KEY(proposal_id, signer_did)
) STRICT;

CREATE TRIGGER IF NOT EXISTS threshold_proposal_active_proposer
BEFORE INSERT ON threshold_change_proposal
FOR EACH ROW
WHEN COALESCE((SELECT active FROM governance_signer WHERE signer_did = NEW.proposer_did), 0) <> 1
BEGIN
    SELECT RAISE(ABORT, 'proposal requires active signer');
END;

CREATE TRIGGER IF NOT EXISTS threshold_approval_active_signer
BEFORE INSERT ON threshold_change_approval
FOR EACH ROW
WHEN COALESCE((SELECT active FROM governance_signer WHERE signer_did = NEW.signer_did), 0) <> 1
BEGIN
    SELECT RAISE(ABORT, 'approval requires active signer');
END;

CREATE TRIGGER IF NOT EXISTS threshold_proposal_immutable
BEFORE UPDATE OF k_min, e_min, r_max, announced_unix_s, executable_unix_s ON threshold_change_proposal
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'announced threshold proposal is immutable');
END;

CREATE TRIGGER IF NOT EXISTS threshold_execution_guard
BEFORE UPDATE OF state ON threshold_change_proposal
FOR EACH ROW
WHEN NEW.state = 'EXECUTED'
BEGIN
    SELECT CASE
        WHEN OLD.state <> 'PENDING'
        THEN RAISE(ABORT, 'only pending proposals may execute')
    END;
    SELECT CASE
        WHEN unixepoch() < OLD.executable_unix_s
        THEN RAISE(ABORT, 'threshold proposal remains time-locked')
    END;
    SELECT CASE
        WHEN (
            SELECT COUNT(*)
            FROM threshold_change_approval AS approval
            JOIN governance_signer AS signer ON signer.signer_did = approval.signer_did
            WHERE approval.proposal_id = OLD.proposal_id AND signer.active = 1
        ) < 2
        THEN RAISE(ABORT, 'two active approvals are required')
    END;
END;
