# cyboquatic_index

`cyboquatic_index` is a small, focused crate for managing rusqlite‑backed indices over cyboquatic ecosafety shards inside the Prometheus‑Praxis workspace.

It is designed to stay non‑networked and deterministic, so it can be safely used by migration tools, CI checks, and offline analysis.

---

## Features

- SQLite (rusqlite + bundled) index over ecosafety and provenance shards.
- Library (`rlib`) for embedding index logic in other Rust crates.
- Optional `cdylib` target for FFI or external tooling.
- Companion `cyboquatic_migrate` binary for schema creation and migrations.

---

## Crate layout

- `src/lib.rs`
  - Core index types (connection helpers, query helpers, schema versioning).
  - Safe wrappers around `rusqlite` for cyboquatic shard use cases.
- `src/bin/migrate.rs`
  - Command‑line migration tool that:
    - Ensures the index database exists.
    - Applies schema migrations in order.
    - Can be wired into CI or local scripts.

---

## Building

From the workspace root:

```bash
cargo build -p cyboquatic_index
```

Build only the migration binary:

```bash
cargo build -p cyboquatic_index --bin cyboquatic_migrate
```

---

## Running migrations

The binary is expected to receive a path to the SQLite database; individual projects can decide where to store it:

```bash
cargo run -p cyboquatic_index --bin cyboquatic_migrate -- \
  --db ./data/cyboquatic_index.sqlite
```

A typical migration flow:

- If the database file does not exist, it is created.
- The current schema version is read from the meta table.
- Pending migrations are applied in order.
- The tool exits with a non‑zero code on any failure.

---

## Integration pattern

In other crates (for example, ecosafety pipelines or governance tools):

- Depend on `cyboquatic_index` as a library.
- Use its connection helpers to open the same SQLite file used by `cyboquatic_migrate`.
- Store or query derived indices that summarize ecosafety metrics, provenance, or governance state.

This keeps the heavy CSV/ALN shards as the source of truth while allowing fast indexed lookups for interactive tools and batch analyses.

---

## License

Dual‑licensed under MIT and Apache‑2.0, at your option.
