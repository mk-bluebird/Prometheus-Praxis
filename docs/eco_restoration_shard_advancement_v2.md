# Advancing `eco_restoration_shard`: 50 Research Questions, Definitions, Detail Queries, and Objections

This document extends the original 50‑item roadmap with 50 new, tightly scoped, and immediately actionable items. Each includes a full answer or solution that directly improves code quality, documentation, cross‑platform acceptance, AI‑chat integration, and the non‑financial asset economy of EcoNet. The numbering continues from the previous set.

---

## Research Questions (RQ)

**51. RQ:** How can we embed a formal verification harness (e.g., using `creusot` or `krml`) for the non‑actuating Rust governance spine to prove that all read‑only API endpoints never issue write transactions?  
- **Answer:**  
  1. Annotate the core library’s `restorationindextool` functions with `#[ensures(...)]` contracts stating that the connection was opened with `SQLITE_OPEN_READONLY` and no write calls were made.  
  2. Write a Kani/Verus harness that symbolically executes all public API functions (`list_contracts`, `list_planes`, etc.) and asserts that the database file’s modification time and content hash remain unchanged.  
  3. Integrate the proof into CI so that any code change that introduces a potential write path is caught before merge.

**52. RQ:** What is the optimal SQLite extension or virtual table design to expose the `blastradiusindex` as a geospatial network, enabling AI‑chat to ask “what are the 5 most impactful restoration nodes within radius X of location Y”?  
- **Answer:**  
  Extend `blastradiusindex` with a `geopoly` blob or use the `spatialite` extension. Create a virtual table `vblastradius_geo` that stores node polygons. Provide a read‑only view that joins the spatial index with `blastradiusindex` metrics. This allows agents to issue spatial queries with `ST_Distance` without leaving SQLite, maintaining the non‑actuating requirement.

**53. RQ:** How can we make the `config.toml` of `restorationindextool` self‑describing so that AI‑chat agents can dynamically discover available governance views and their parameters without hardcoded knowledge?  
- **Answer:**  
  Include a `[discovery]` section in `config.toml` pointing to a `.json` endpoint (or an SQLite table `agent_discovery`) that returns a list of all `vagent*` views with their column schemas, supported region filters, and allowed lane bands. The Rust binary exposes this as a read‑only CLI flag `--discover-views` outputting JSON, fulfilling the non‑actuating contract.

**54. RQ:** What is the safest method to introduce a `restoration_governance_graph` that models contracts, identity bindings, and shard dependencies as a directed acyclic graph, enabling impact analysis of contract deprecations?  
- **Answer:**  
  Add a `governance_dependency` table linking `source_artifact` and `target_artifact` (both FK to `repofile`). Create a materialized DAG view that computes transitive dependencies and a `vblastradius_deprecation_impact` view that shows all downstream artifacts affected by a contract status change. Use recursive CTEs to traverse the graph, all read‑only.

**55. RQ:** How can we introduce a proof‑of‑eco‑contribution ZK circuit that allows off‑chain restoration actions to be verified on‑chain without revealing the underlying sensitive healthcare or location data?  
- **Answer:**  
  Design a Plonkish circuit that takes as private witness the CEIM mass/karma deltas, the `artifactregistry` content hash, and the Bostrom identity binding; the public output is a commitment to the action and a proof that it meets KER thresholds and non‑rollback invariants. Store the proof and verification key in `artifactprovenance`, allowing on‑chain contracts to accept the proof without seeing raw data.

**56. RQ:** What is the minimal addition to the CI/CD pipeline to auto‑generate a `restorationindex.sqlite3` binary for each release and make it available as a GitHub Release artifact, enabling downstream agents to bootstrap without building from SQL seeds?  
- **Answer:**  
  Add a CI job that runs all `db/*.sql` seeds into a fresh database, runs consistency checks (`vfilewiring_missing_*`), and then compresses the file. Publish it as a release asset with a semantic version tag. The `restorationindextool` then accepts a `--release-db-url` flag to download and verify the binary against a known hash registered in DefinitionRegistry.

**57. RQ:** How can we embed a lightweight “eco‑memory” time‑series database inside SQLite to track eco‑per‑joule, restoration mass deltas, and neuroethic radii over time, enabling AI‑chat to generate historical trend analyses without external tools?  
- **Answer:**  
  Use SQLite’s `WITH RECURSIVE` and generate a `metrics_timeseries` table keyed by `(nodeid, metric_name, bucket_ts)`. Create views that resample data to hourly/daily resolution using window functions, and expose a `vagent_eco_history` view. Provide a template prompt for AI that includes example SQL to compute moving averages, enabling rich dialogue.

**58. RQ:** What approach ensures that the `virtual_security_backup` table’s Merkle roots can be verified by third‑party auditors without requiring them to run the full Rust/C toolchain?  
- **Answer:**  
  Provide a small, standalone verification script (shell + sqlite3) that recomputes the Merkle root from a CSV export of the relevant snapshot data. The script is registered in `repofile` as `TOOLING`. CI runs the script against test data to ensure the verification algorithm matches the Rust implementation. Auditors can then verify with only SQLite and the CSV export, no Rust compilation needed.

---

## Definition Requests (DR)

**51. DR:** Provide a full DDL specification for a `restoration_did_registry` table that registers Bostrom‑bound DIDs and their corresponding public keys, enabling off‑chain signature verification for identity bindings without exposing private keys.  
- **Answer:**  
  ```sql
  CREATE TABLE did_registry (
      did_id INTEGER PRIMARY KEY,
      bostrom_address TEXT NOT NULL,
      did TEXT NOT NULL UNIQUE,            -- e.g., did:eco:phoenix:...
      public_key_hex TEXT NOT NULL,
      key_type TEXT NOT NULL DEFAULT 'ed25519',
      created_utc TEXT NOT NULL,
      active INTEGER NOT NULL DEFAULT 1,
      FOREIGN KEY (bostrom_address) REFERENCES bostromaddress(bostromaddress)
  );
  ```
  A view `vactive_dids` lists only active DIDs. CI verifies that every identity binding references a DID present here and that the `evidencehex` field is a valid signature over the referenced artifact’s hash using that public key.

**52. DR:** Define the schema for `cross_platform_acceptance_audit` that logs every read request from external AI‑chat platforms to governance views, including timestamp, query fingerprint, and platform identity, to support governance and accountability.  
- **Answer:**  
  ```sql
  CREATE TABLE cross_platform_acceptance_audit (
      audit_id INTEGER PRIMARY KEY,
      acceptance_id INTEGER NOT NULL,
      query_fingerprint TEXT NOT NULL,  -- hash of query text + params
      platform_name TEXT NOT NULL,
      access_utc TEXT NOT NULL,
      response_rows INTEGER,
      FOREIGN KEY (acceptance_id) REFERENCES cross_platform_acceptance(acceptance_id)
  );
  CREATE VIEW vplatform_usage_stats AS
  SELECT platform_name, COUNT(*) as queries, MIN(access_utc), MAX(access_utc)
  FROM cross_platform_acceptance_audit GROUP BY platform_name;
  ```
  The tool that serves queries appends a row for each request; no writes to governance tables, only to this audit log.

**53. DR:** Provide the exact columns and constraints for a `restoration_deliverable` table that tracks tangible eco‑restoration outputs (e.g., “tree planting Q1 2026”) and links them to on‑chain proofs, contract bindings, and KER‑validated metrics.  
- **Answer:**  
  ```sql
  CREATE TABLE restoration_deliverable (
      deliverable_id INTEGER PRIMARY KEY,
      logicalname TEXT NOT NULL,  -- e.g., restoration.deliverable.treeplanting.phoenix.2026q1
      contract_id TEXT NOT NULL,
      address_id INTEGER NOT NULL,
      region TEXT NOT NULL,
      metric_mass_kg REAL,
      metric_karma REAL,
      evidence_artifact_id INTEGER,  -- FK to artifactregistry
      onchain_tx_hash TEXT,
      ker_proof_hex TEXT,           -- zk‑proof that KER thresholds were met
      created_utc TEXT NOT NULL,
      status TEXT NOT NULL CHECK (status IN ('PROPOSED','ACTIVE','VERIFIED')),
      FOREIGN KEY (contract_id) REFERENCES restorationcontract(contractid),
      FOREIGN KEY (address_id) REFERENCES bostromaddress(addressid),
      FOREIGN KEY (evidence_artifact_id) REFERENCES artifactregistry(artifactid)
  );
  ```

**54. DR:** Specify the structure of a `restoration_research_document` table that indexes all `.pdf` and `.md` research files committed to the repo, with metadata enabling semantic search by AI‑chat.  
- **Answer:**  
  ```sql
  CREATE TABLE research_document (
      doc_id INTEGER PRIMARY KEY,
      repofile_id INTEGER NOT NULL,
      title TEXT,
      abstract TEXT,
      author_bostrom TEXT,
      logicalname TEXT,  -- e.g., doc.restoration.hydrology.phoenix.2026v1
      tags TEXT,         -- comma-separated
      created_utc TEXT,
      content_hash TEXT,
      FOREIGN KEY (repofile_id) REFERENCES repofile(fileid)
  );
  CREATE VIRTUAL TABLE IF NOT EXISTS research_document_fts USING fts5(
      title, abstract, tags, content='research_document', content_rowid='doc_id'
  );
  ```
  CI triggers a rebuild of the FTS index on new doc commits, enabling full‑text search from AI‑chat.

**55. DR:** Define the `config.toml` section for a future `agent-api-server` that exposes the governance views over HTTP/QUIC, with explicit read‑only mode, CORS, and rate‑limiting settings.  
- **Answer:**  
  ```toml
  [agent_api]
  enabled = false                # set to true to activate server
  listen = "127.0.0.1:8080"
  read_only = true
  tls_cert = "certs/agent.crt"
  tls_key = "certs/agent.key"
  cors_allowed_origins = ["https://perplexity.ai"]
  rate_limit_requests_per_sec = 10
  max_response_rows = 1000
  ```
  When `enabled` is true, the binary uses `actix-web` or `warp` to serve the defined agent views; otherwise it remains CLI‑only.

---

## Detail Queries (DQ)

**51. DQ:** Write the SQL query that detects any `restorationcontract` whose logicalname is not referenced by any `definitionregistryrestoration` row, indicating a contract that is defined but never wired to a concrete governance artifact.  
- **Answer:**  
  ```sql
  SELECT rc.contractid, rc.logicalname, rc.status
  FROM restorationcontract rc
  LEFT JOIN definitionregistryrestoration dr
    ON dr.logicalname = rc.logicalname
  WHERE dr.logicalname IS NULL;
  ```
  CI should fail if this returns rows for contracts with status `ACTIVE`.

**52. DQ:** Provide the query to list all `blastradiusindex` nodes in Phoenix whose `deltakarmawindow` is negative, indicating net ecological harm despite `restorationok = 1`, to catch data inconsistencies.  
- **Answer:**  
  ```sql
  SELECT region, domain, scopeid, restorationradiusm, deltakarmawindow, restorationok
  FROM blastradiusindex
  WHERE region = 'Phoenix-AZ'
    AND restorationok = 1
    AND deltakarmawindow < 0;
  ```
  Since `vrestorationnodesphx` filters `deltakarmawindow >= 0`, this should return zero rows; if not, a governance warning is raised.

**53. DQ:** Query to generate a “health check” report summarizing: count of missing identity bindings, files without repofile entries, and views with broken dependencies, suitable for a dashboard.  
- **Answer:**  
  ```sql
  SELECT 'missing_identity_bindings' AS check_name, COUNT(*) FROM dq27_missing_identity_bindings
  UNION ALL
  SELECT 'dangling_identity_bindings', COUNT(*) FROM ci_identitybinding_missing_files
  UNION ALL
  SELECT 'missing_file_wiring', COUNT(*) FROM vfilewiring_missing_definition;
  ```
  The Rust CLI can format this as JSON for monitoring tools.

**54. DQ:** Write a recursive CTE that starts from a given `restorationcontract` and traverses `governance_dependency` to list all artifacts (repofile entries) that directly or transitively depend on it, generating a deprecation impact report.  
- **Answer:**  
  ```sql
  WITH RECURSIVE dep_chain AS (
      SELECT target_repofile_id, 1 AS depth
      FROM governance_dependency
      WHERE source_contract_id = ?
      UNION ALL
      SELECT gd.target_repofile_id, dc.depth+1
      FROM dep_chain dc
      JOIN governance_dependency gd ON gd.source_repofile_id = dc.target_repofile_id
  )
  SELECT DISTINCT rf.relpath, rf.purpose, dc.depth
  FROM dep_chain dc
  JOIN repofile rf ON rf.fileid = dc.target_repofile_id
  ORDER BY dc.depth;
  ```
  This can be called by `restorationindextool --deprecation-impact <contractid>`.

**55. DQ:** Query to compute the “eco‑restoration efficiency” per node as `deltakarmawindow / (deltamasswindowkg + 1)`, ranking Phoenix nodes from most to least efficient, for agent dialogue highlighting high‑impact interventions.  
- **Answer:**  
  ```sql
  SELECT domain, scopeid,
         deltamasswindowkg, deltakarmawindow,
         deltakarmawindow / (deltamasswindowkg + 1.0) AS karma_per_kg
  FROM vrestorationnodesphx
  ORDER BY karma_per_kg DESC;
  ```
  The Rust tool can expose this as a dedicated view `vagent_eco_efficiency_phx`.

---

## Objection Identifiers (OI)

**51. OI:** Flag that the `config.toml` currently has no schema validation, and a malformed config could cause the Rust tool to fail silently or connect with unintended permissions. Propose a JSON Schema for the TOML file and a CI validation step.  
- **Answer:**  
  Provide a `config_schema.json` that describes the TOML structure, and add a CI step that uses `check-jsonschema` (or `taplo`) to validate the `config.toml` against it. The Rust binary also validates the config at startup and exits with a clear error. This ensures AI‑chat and CI always operate on a valid configuration.

**52. OI:** Object that the `virtual_security_backup` table does not track which snapshot version a Merkle root belongs to, making it impossible to verify historical snapshots after updates.  
- **Answer:**  
  Add a `snapshot_version` column (monotonic integer) and a UNIQUE constraint on `(artifact_id, snapshot_version)`. Each time a new backup is created for the same artifact, version increments; the Merkle root and depth correspond to that version. The view `vactive_backup` filters for the latest version per artifact.

**53. OI:** Highlight that the `cross_platform_acceptance` table grants read access based on a plain `bostromaddress`, but doesn’t enforce that the platform indeed controls that address, risking impersonation.  
- **Answer:**  
  Require that each platform entry includes a `challenge_response` field: the platform must sign a nonce with the Bostrom private key and store the signature in `credential_hash`. The agent API server verifies this signature on each connection before serving data. Without it, access is denied. This turns `cross_platform_acceptance` from a static whitelist into a cryptographic access control layer.

**54. OI:** Point out that the current `dbfilewiring.sql` schema does not record the “intended consumer” (human, CI, AI‑chat) of each wired file, which could lead to AI‑chat mistakenly using files not intended for agent consumption.  
- **Answer:**  
  Add a `consumer_kind` column to `file_wiring` with values like `HUMAN`, `CI`, `AGENT_API`, `AI_CHAT`. Then define agent‑facing views (`vagentrestorationnodesphx`, etc.) to only include files where `consumer_kind` includes `AI_CHAT`. This prevents AI from querying raw governance schemas directly.

**55. OI:** Raise the objection that the repo lacks a `SECURITY.md` file describing the threat model and vulnerability disclosure process, which is critical when the governance spine carries health‑ and identity‑related bindings.  
- **Answer:**  
  Add a `SECURITY.md` (registered as `DOC_SPEC` in `repofile`) outlining the trust boundaries: the SQLite file is public, no PII is stored in plain text, identity bindings are hashed or zero‑knowledge, and all write operations are performed by a separate, air‑gapped process. This document should also describe how to report vulnerabilities via a dedicated email or keybase. Once committed, CI enforces that any schema change does not introduce columns that store raw identity data unless they are hashed.
