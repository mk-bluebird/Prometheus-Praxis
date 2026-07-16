# SQL Schemas for Governance, Ecosafety, and Audit

This directory contains SQLite schema files that support the Prometheus‑Praxis ecosafety spine, KER scoring, and non‑actuating governance patterns. All files are intended to be applied directly with `sqlite3` and to remain under ALN control (no ad‑hoc DDL).

The default assumption is:

- License: MIT OR Apache‑2.0 (dual‑license)
- Target engine: SQLite 3.x
- Foreign keys: `PRAGMA foreign_keys = ON;`

---

## Directory purpose

The `sql/` directory is the canonical place for:

- **Shard‑backed schemas** that mirror ALN particles (e.g., response shards, governance meta).
- **Audit trails** for safety‑critical operations (e.g., actuator attempts, lane changes).
- **Index definitions** that make KER backfill and governance queries efficient.
- **Non‑actuating helpers** (e.g., backfill meta, corridor version tables) that never drive hardware directly.

Rust and ALN code should treat these `.sql` files as the only source of truth for database structure; migrations are applied by including or executing these scripts, not by generating DDL at runtime.

---

## Files

### `actuator_audit_schema.sql`

Defines the `actuator_audit` table and index used by the non‑actuating safety interlock (fuse‑box) for actuators.

**Table:**

```sql
CREATE TABLE IF NOT EXISTS actuator_audit (
    audit_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_utc       TEXT NOT NULL,
    deployment_id       TEXT NOT NULL,
    actuator_id         TEXT NOT NULL,
    command             TEXT NOT NULL,
    outcome             TEXT NOT NULL, -- e.g., ALLOW, DENY_NON_ACTUATING_ONLY
    non_actuating_only  INTEGER NOT NULL CHECK (non_actuating_only IN (0,1)),
    evidence_hex        TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_actuator_audit_deployment
    ON actuator_audit(deployment_id, timestamp_utc);
```

**Semantics:**

- **`timestamp_utc`**  
  ISO‑8601 UTC timestamp when the actuation attempt was evaluated by the fuse‑box.

- **`deployment_id`**  
  Logical deployment identifier (matches ALN / shard deployment IDs).

- **`actuator_id`**  
  Hardware or logical actuator identifier (e.g., `valve:drainage_outlet`).

- **`command`**  
  High‑level command string (e.g., `OPEN`, `CLOSE`, `SET_FLOW:0.3`).

- **`outcome`**  
  Result of the fuse‑box decision, typically:
  - `ALLOW`
  - `DENY_NON_ACTUATING_ONLY`
  - `DENY_POLICY`
  - `DENY_ERROR`

- **`non_actuating_only`**  
  Governance flag snapshot (0/1) at the time of the decision; when `1`, any actuation attempt must be denied and logged.

- **`evidence_hex`**  
  Hex‑encoded governance hash (ALN spec + corridors + KER invariants) that justified the decision at the time.

**Usage:**

- **C++ fuse‑box**  
  - On every call to `requestActuation`, writes one row into `actuator_audit`.
  - Uses prepared statements and a single connection per process to avoid corruption.

- **Watchdog / governance backfill**  
  - Daily jobs scan `actuator_audit` to:
    - Detect any violation of `non_actuating_only`.
    - Compute KER windows and residual risk from actuator behavior.
    - Surface violations as `BlockedByRisk` or `BlockedByCalib` in higher‑level shards.

---

## Conventions

### File naming

- Use lower‑case, snake‑case names ending in `_schema.sql`, for example:
  - `actuator_audit_schema.sql`
  - `response_shard_schema.sql`
  - `corridor_bands_schema.sql`

This makes it easy to discover schemas by table family and keeps diffs small and predictable.

### Structure

Each `.sql` file should:

1. Enable foreign keys explicitly:
   ```sql
   PRAGMA foreign_keys = ON;
   ```
2. Wrap changes in a transaction:
   ```sql
   BEGIN TRANSACTION;
   -- DDL here
   COMMIT;
   ```
3. Declare all `CHECK` constraints and indexes needed for governance queries.
4. Avoid application‑specific data inserts (no seed data here).

### Governance and KER

- Each table should be traceable to:
  - An ALN particle (spec file in `aln/`),
  - A Rust struct in the corresponding crate (e.g., `crates/...`),
  - And a KER role (which K, E, R coordinates it contributes to).

- `evidence_hex` columns are mandatory wherever decisions or metrics are logged; they anchor rows back to ALN governance specs and proofs.

---

## Applying schemas

Initialize or update a SQLite database:

```sh
# Example: initialize fuse-box audit DB
sqlite3 path/to/fusebox.db < sql/actuator_audit_schema.sql
```

Recommended pattern:

- Application startup:
  - Ensure required schemas are present by running the corresponding `.sql` scripts on a fresh DB.
- CI:
  - Run `sqlite3 :memory: < sql/actuator_audit_schema.sql` as part of tests to validate DDL.
  - Optionally inspect the resulting schema with `PRAGMA table_info(...)` to keep Rust/ALN bindings in sync.

---

## Integration points

- **C++ fuse‑box / watchdog**  
  - Uses `actuator_audit` to log every actuation attempt and its outcome.
  - Relies on this schema for crash‑safe, append‑only auditing.

- **ALN governance**  
  - Treats `actuator_audit` as a non‑actuating evidence shard.
  - Uses `evidence_hex` to tie audit rows back to specific governance versions.

- **KER backfill**  
  - Loads `actuator_audit` into response shards or aggregate views.
  - Computes residual scores and KER windows that influence deployment decisions.

---

## Extending this directory

When adding new schemas:

1. Create a new `_schema.sql` file with:
   - Clear table and index definitions.
   - Comments documenting purpose and field semantics.
2. Update this `README.md`:
   - Add a section for the new file.
   - Describe how it participates in governance and KER.
3. Keep the schema in lock‑step with:
   - ALN spec definitions.
   - Rust struct definitions.
   - Any backfill or CI tools that ingest these tables.

This keeps `sql/` as a reliable fuse‑box for data structures: easy to discover, easy to inspect, and tightly coupled to the ALN‑governed ecosafety grammar.
