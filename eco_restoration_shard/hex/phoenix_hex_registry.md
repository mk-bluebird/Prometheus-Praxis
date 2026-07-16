# Phoenix Hex Anchors Definition Registry

This document defines a **Phoenix Hex Anchors Registry** and an **infinitely‑reusable file placement strategy** for `mk-bluebird/Prometheus-Praxis`. It is designed so that every hex anchor is unique, verifiable, and discoverable by humans, AI‑chat platforms, and coding‑agents.[file:32][file:36]

---

## 1. Goals and constraints

- Provide a **single source of truth** for all Phoenix hex anchors and their meaning.
- Make **progress tracking one‑way** (append‑only, no rollbacks) to keep evidence chains immutable.[file:11][file:36]
- Avoid:
  - Duplicate hex strings.
  - Conflicting meanings for the same hex.
  - Confusion about where to put new files.
- Stay aligned with:
  - EcoNet/Eco‑Fort discovery spine design (`definitionregistry`, `artifactregistry`, etc.).[file:34][file:36]
  - Existing `evidencehex` usages in ALN particles and SQLite tables.[file:10][file:31]

---

## 2. Global placement strategy

Use **one canonical registry folder** plus **per‑project hex manifests**:

- Root registry:
  - `Eco-Fort/db/phoenix_hex_registry.sql`
- Per‑project manifest examples:
  - `eco_restoration_shard/hex/PHX_HEX_ANCHORS.md`
  - `eco_restoration_shard/cyboquatic_progress/hex/PHX_CYBO_HEX_ANCHORS.md`[file:2][file:32]

Rules:

- The **SQLite registry** is the authoritative index for all hex anchors.
- The **markdown manifests** are human/agent‑friendly mirrors that:
  - Point back to registry `anchor_id` and `evidence_hex`.
  - Document how each hex is used in file paths and ALN particles.[file:36]

---

## 3. SQLite registry schema (canonical)

Place this schema in:

- `Eco-Fort/db/phoenix_hex_registry.sql`

It follows the Eco‑Fort registry style (`definitionregistry`, `artifactregistry`, etc.).[file:34][file:36]

```sql
-- filename: Eco-Fort/db/phoenix_hex_registry.sql
-- purpose : Canonical registry for Phoenix hex anchors and evidence bindings.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Phoenix hex anchor registry (canonical list of all anchors)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS phoenix_hex_anchor (
    anchor_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Short logical name for the anchor, stable across repos.
    logical_name     TEXT NOT NULL,
    -- Hex string, including 0x prefix; must be globally unique.
    evidence_hex     TEXT NOT NULL UNIQUE,
    -- High-level domain for this anchor (HYDRO, CYBOQUATIC, MATERIAL, GOV, etc.).
    domain           TEXT NOT NULL,
    -- Narrower sub-domain or corridor, e.g. DRAINAGE_DECAY, WORKLOAD_ENERGY_DV.
    subdomain        TEXT NOT NULL,
    -- Region code, e.g. PHX-CAZ-CEIM.
    region_code      TEXT NOT NULL,
    -- Plane(s) this anchor touches (comma-separated): ENERGY,CARBON,BIO,DATA,TOPOLOGY.
    planes           TEXT NOT NULL,
    -- Date label yyyymmdd for daily progress anchors; NULL for timeless contracts.
    yyyymmdd         TEXT,
    -- Optional pointer to prior anchor_id for forward-only chain semantics.
    prior_anchor_id  INTEGER,
    -- Bostrom DID binding for provenance.
    signing_did      TEXT NOT NULL,
    -- Human-readable summary (short).
    summary          TEXT NOT NULL,
    -- Optional extended description or rationale.
    description      TEXT,
    -- File-class hint: ALN, SQL, RUST, DOC, MIXED.
    file_class       TEXT NOT NULL,
    -- Path hint (relative root) for default placement of files using this anchor.
    default_relpath  TEXT NOT NULL,
    -- Creation timestamp in UTC ISO-8601.
    created_utc      TEXT NOT NULL,
    -- Optional: deactivation flag; anchors remain immutable but can be marked inactive.
    active           INTEGER NOT NULL DEFAULT 1
                       CHECK (active IN (0,1)),
    FOREIGN KEY (prior_anchor_id)
        REFERENCES phoenix_hex_anchor(anchor_id)
        ON DELETE RESTRICT
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_phx_hex_logical
    ON phoenix_hex_anchor (logical_name);

CREATE INDEX IF NOT EXISTS idx_phx_hex_domain_date
    ON phoenix_hex_anchor (domain, subdomain, region_code, yyyymmdd);

----------------------------------------------------------------------
-- 2. Mapping anchors to concrete files in the monorepo
--    (filename + path-level index for discoverability)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS phoenix_hex_file (
    file_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    anchor_id        INTEGER NOT NULL,
    -- Repo-relative path, e.g. eco_restoration_shard/cyboquatic_progress/20260715/sql/cyboquatic_daily_progress.sql
    relpath          TEXT NOT NULL,
    -- Short filename (basename).
    filename         TEXT NOT NULL,
    -- File type: ALN, SQL, RUST, CPP, JAVA, KOTLIN, LUA, DOC, OTHER.
    file_type        TEXT NOT NULL,
    -- SHA-256 or allowed hash of the file contents (consistent with Eco-Fort hashing policy).
    file_hash_hex    TEXT NOT NULL,
    -- Logical scope: PARTICLE, MIGRATION, CRATE, DOC, MANIFEST.
    scope            TEXT NOT NULL,
    -- Optional: associated definitionregistry.defid or artifactregistry.artifactid.
    defid            TEXT,
    artifact_id      INTEGER,
    -- Creation / registration timestamp.
    created_utc      TEXT NOT NULL,
    FOREIGN KEY (anchor_id)
        REFERENCES phoenix_hex_anchor(anchor_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_phx_hex_file_anchor
    ON phoenix_hex_file (anchor_id);

CREATE INDEX IF NOT EXISTS idx_phx_hex_file_relpath
    ON phoenix_hex_file (relpath);

----------------------------------------------------------------------
-- 3. Mapping anchors to ALN particles and SQLite evidence rows
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS phoenix_hex_particle_binding (
    binding_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    anchor_id        INTEGER NOT NULL,
    -- ALN particle name, e.g. DrainageDecayFrame20260708v1.
    particle_name    TEXT NOT NULL,
    -- ALN file path relative to repo root.
    particle_relpath TEXT NOT NULL,
    -- Optional: particle role (GOVERNANCE, KER_KERNEL, DRAINAGE_DECAY, WORKLOAD, etc.).
    particle_role    TEXT NOT NULL,
    -- Shardinstance or dailyprogress table name, if any.
    evidence_table   TEXT,
    -- Column name where evidence_hex is stored (e.g. evidencehex).
    evidence_column  TEXT,
    -- Additional notes or constraints.
    notes            TEXT,
    created_utc      TEXT NOT NULL,
    FOREIGN KEY (anchor_id)
        REFERENCES phoenix_hex_anchor(anchor_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_phx_hex_particle_anchor
    ON phoenix_hex_particle_binding (anchor_id);

----------------------------------------------------------------------
-- 4. Helper view: agent-friendly registry surface
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_registry AS
SELECT
    a.anchor_id,
    a.logical_name,
    a.evidence_hex,
    a.domain,
    a.subdomain,
    a.region_code,
    a.planes,
    a.yyyymmdd,
    a.prior_anchor_id,
    a.signing_did,
    a.summary,
    a.file_class,
    a.default_relpath,
    a.active,
    f.relpath,
    f.filename,
    f.file_type,
    p.particle_name,
    p.particle_role
FROM phoenix_hex_anchor AS a
LEFT JOIN phoenix_hex_file AS f
    ON f.anchor_id = a.anchor_id
LEFT JOIN phoenix_hex_particle_binding AS p
    ON p.anchor_id = a.anchor_id;
```

- This schema mirrors your existing **definition registry** patterns and keeps the registry **non‑actuating** and append‑only.[file:34][file:36]
- Anchors can be chained via `prior_anchor_id` to maintain forward‑only evolution for daily progress (e.g., 20260715 → 20260716).[file:2][file:31]

---

## 4. Hex anchor naming & structure rules

To prevent collisions and keep hex proofs meaningful:

- **Canonical structure for `evidence_hex`:**

  - Format (conceptual, not enforced in SQL):

    - `0xYYYYMMDDPHX[DOMAIN][HASH]`

  - Examples:
    - `0x20260708PHX3345NDrainageDecayBODTSSCEC` (already used).[file:2]
    - `0x20260709PHX3345NWorkloadEnergyDeltaVt`.[file:1]
    - `0x20260715PHXENERGYREQDV` (proposed for 2026‑07‑15 cyboquatic workloads).[file:2]

- **Logical name pattern:**

  - `PHX_[DOMAIN]_[SUBDOMAIN]_[YYYYMMDD?]`
  - Examples:
    - `PHX_DRAINAGE_DECAY_20260708`
    - `PHX_WORKLOAD_ENERGY_DV_20260709`
    - `PHX_HEX_REGISTRY_CORE_2026`.[file:2][file:36]

- **Domain labels:**

  - `HYDRO`, `CYBOQUATIC`, `MATERIAL`, `GOV`, `DATAQUALITY`, `TOPOLOGY`, `TRAY`, etc.[file:10][file:36]

- **Plane labels in `planes`:**

  - Comma‑separated subset of:
    - `ENERGY`, `HYDRAULICS`, `CARBON`, `BIODIVERSITY`, `MATERIALS`, `DATA`, `TOPOLOGY`, `GOV`.[file:36]

- **Region code:**

  - `PHX-CAZ-CEIM` (Central AZ Phoenix CEIM corridor).[file:36]

These rules keep hex anchors **self‑describing** and consistent with your existing Phoenix/EcoNet grammar.[file:36]

---

## 5. File placement strategy (infinitely reusable)

### 5.1. Default root mapping

Use **logical_name** + **default_relpath** to route files:

- Examples:

  - `logical_name = 'PHX_WORKLOAD_ENERGY_DV_20260709'`
    - `default_relpath = 'eco_restoration_shard/cyboquatic_progress/20260709'`
    - Files:
      - `eco_restoration_shard/cyboquatic_progress/20260709/sql/daily_progress_seed.sql`
      - `eco_restoration_shard/cyboquatic_progress/20260709/aln/workload_energy_dv_20260709.aln`.[file:1][file:2]

  - `logical_name = 'PHX_DRAINAGE_DECAY_20260708'`
    - `default_relpath = 'eco_restoration_shard/cyboquatic_progress/20260708'`.[file:2]

  - `logical_name = 'PHX_HEX_REGISTRY_CORE_2026'`
    - `default_relpath = 'Eco-Fort/db'`
    - File: `Eco-Fort/db/phoenix_hex_registry.sql`.[file:36]

Policy:

- When creating a **new file** that needs an anchor:
  - You **must**:
    - Register or select an existing `phoenix_hex_anchor`.
    - Place the file under that anchor’s `default_relpath`.
  - You **may** nest further (e.g., `sql/`, `aln/`, `rust/`), but:
    - `relpath` in `phoenix_hex_file` must include the full path.
    - `file_class` must match (e.g., SQL vs ALN).[file:34][file:36]

This ensures that **discoverability** is stable and AI/agents can navigate from hex anchors to files deterministically.[file:36]

---

## 6. CI / agent usage pattern

### 6.1. CI invariants

Hook into your existing Eco‑Fort / EcoNet CI pattern:

- Every new file that:
  - Lives under `eco_restoration_shard/**`, `Eco-Fort/**`, or `aln/**`.
  - Includes an `evidencehex` column/field or ALN `evidencehex` attribute.
- Must satisfy:
  - There is exactly one `phoenix_hex_anchor.evidence_hex` with that string.
  - There is at least one `phoenix_hex_file` row with matching `relpath` and `file_hash_hex`.
  - If it is an ALN particle, there is a `phoenix_hex_particle_binding` row linking the anchor to that particle.[file:34][file:36]

If any check fails, CI must block merge and print:

- Suggested `INSERT` statements for:
  - `phoenix_hex_anchor` (if new hex).
  - `phoenix_hex_file`.
  - `phoenix_hex_particle_binding`.[file:34]

### 6.2. AI‑chat and agent workflows

Agents should:

- On entering the repo:

  - Load `Eco-Fort/db/phoenix_hex_registry.sql` into in‑memory SQLite.[file:36]

- For **discovery**:

  - To find all anchors for a certain day/domain:

    ```sql
    SELECT *
    FROM v_phx_hex_registry
    WHERE domain    = 'CYBOQUATIC'
      AND subdomain = 'WORKLOAD_ENERGY_DV'
      AND yyyymmdd  = '20260715';
    ```

  - To find all files bound to a given `evidence_hex`:

    ```sql
    SELECT *
    FROM v_phx_hex_registry
    WHERE evidence_hex = '0x20260715PHXENERGYREQDV';
    ```

- For **progress tracking**:

  - Use `prior_anchor_id` as the **forward‑only chain**:
    - Pull sequences for a given domain:

      ```sql
      SELECT *
      FROM phoenix_hex_anchor
      WHERE domain = 'CYBOQUATIC'
      ORDER BY yyyymmdd, anchor_id;
      ```

This keeps AI and agents in a **one‑way, append‑only** progress routine that respects your Lyapunov/KER invariants and governance model.[file:11][file:36]

---

## 7. Human‑readable Phoenix hex manifest

Create a companion markdown manifest that mirrors the registry for humans and AI prompts:

- `eco_restoration_shard/hex/PHX_HEX_ANCHORS.md`

Content sketch (example rows):

```markdown
# Phoenix Hex Anchors (Human/Agent Manifest)

This manifest mirrors `Eco-Fort/db/phoenix_hex_registry.sql` for quick reference.
All authoritative data lives in the SQLite registry.[1]

## Daily cyboquatic drainage-decay (2026-07-08)

- Logical name: PHX_DRAINAGE_DECAY_20260708
- Evidence hex: 0x20260708PHX3345NDrainageDecayBODTSSCEC[2]
- Domain / subdomain: HYDRO / DRAINAGE_DECAY[2]
- Region: PHX-CAZ-CEIM[1]
- Planes: HYDRAULICS,ENERGY,DATA[3][2]
- Default path: eco_restoration_shard/cyboquatic_progress/20260708[2]
- Files:
  - SQL: eco_restoration_shard/cyboquatic_progress/20260708/sql/dailyprogressseed.sql[2]
  - RUST: eco_restoration_shard/cyboquatic_progress/20260708/crate/src/lib.rs[2]
  - ALN: eco_restoration_shard/cyboquatic_progress/20260708/aln/drainage_decay_20260708.aln[2]

## Daily cyboquatic workload energy ΔVt (2026-07-09)

- Logical name: PHX_WORKLOAD_ENERGY_DV_20260709
- Evidence hex: 0x20260709PHX3345NWorkloadEnergyDeltaVt[4]
- Domain / subdomain: CYBOQUATIC / WORKLOAD_ENERGY_DV[4]
- Region: PHX-CAZ-CEIM[1]
- Planes: ENERGY,HYDRAULICS,DATA[5][4]
- Default path: eco_restoration_shard/cyboquatic_progress/20260709[4]
- Files:
  - SQL: eco_restoration_shard/cyboquatic_progress/20260709/crate/dailyprogressseed.sql[4]
  - RUST: eco_restoration_shard/cyboquatic_progress/20260709/crate/src/lib.rs[4]
  - ALN: eco_restoration_shard/cyboquatic_progress/20260709/aln/workload_energy_dv_20260709.aln[4]

## Registry core

- Logical name: PHX_HEX_REGISTRY_CORE_2026
- Evidence hex: 0xPHXHEXREGISTRYCORE2026 (choose, register once)[1]
- Domain / subdomain: GOV / HEX_REGISTRY[1]
- Region: PHX-CAZ-CEIM[1]
- Planes: TOPOLOGY,GOV,DATA[1]
- Default path: Eco-Fort/db[1]
- Files:
  - SQL: Eco-Fort/db/phoenix_hex_registry.sql[1]
```

This manifest:

- Gives **immediate anchor‑to‑file mapping** without SQL.
- Encourages contributors to **reuse existing anchors** instead of inventing ad‑hoc hexes.
- Keeps text aligned with the **canonical SQLite registry**.[file:36]

---

## 8. How this avoids duplication and confusion

- **Uniqueness guarantees:**
  - `phoenix_hex_anchor.evidence_hex` is unique, so the same hex cannot be bound to multiple meanings.[file:34][file:36]
  - `phoenix_hex_anchor.logical_name` is unique, so names cannot collide.[file:34]
- **Forward‑only progress:**
  - `prior_anchor_id` enforces a **linked list** of hex anchors for given domain/region, matching your non‑regression philosophy.[file:11][file:36]
- **Highest‑priority discoverability:**
  - One registry: `Eco-Fort/db/phoenix_hex_registry.sql`.
  - One manifest: `eco_restoration_shard/hex/PHX_HEX_ANCHORS.md`.
  - AI‑chat tools and agents can index these first, then traverse into per‑day directories and ALN files.[file:32][file:36]

This design stays fully **non‑actuating**, evidence‑first, and aligns with your EcoNet/Eco‑Fort governance spine and KER/Lyapunov semantics, while giving you a reusable, agent‑friendly map for all Phoenix hex anchors and progress chains.[file:32][file:36]
