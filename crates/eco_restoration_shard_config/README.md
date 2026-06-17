<!-- filename: eco_restoration_shard_config/README.md -->

# eco_restoration_shard_config

Non-actuating configuration loader for [`mk-bluebird/eco_restoration_shard`](https://github.com/mk-bluebird/eco_restoration_shard).

This crate provides:

- A typed `Config` and `ProviderConfig` model for `config.yaml`.
- Strict validation anchored to the canonical Bostrom DID.
- Machine-readable error reports suitable for CI and AI-chat tools.
- A small helper binary to print JSON validation reports.

## Config schema

The expected `config.yaml` structure:

```yaml
version: "2026.v1"
owner_did: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"
providers:
  - name: "qwen-research"
    endpoint: "https://api.qwen-research.example/v1/chat"
    apikey_env: "QWEN_RESEARCH_API_KEY"
    lane: "RESEARCH"
    non_actuating: true
```

Constraints:

- `owner_did` must equal the canonical Bostrom DID for this repo.
- `version` must be non-empty.
- `providers` must contain at least one entry.
- Each provider must supply non-empty `name`, `endpoint`, `apikey_env`, and `lane`.
- `non_actuating` must be `true` for every provider.

## Library usage

Add to your workspace dependencies:

```toml
eco_restoration_shard_config = { path = "eco_restoration_shard_config" }
```

Load and validate configuration:

```rust
use eco_restoration_shard_config::{load_config, ConfigError};

fn main() {
    match load_config() {
        Ok(cfg) => {
            println!("Loaded config version {}", cfg.version);
            for p in cfg.providers {
                println!("Provider {} at {}", p.name, p.endpoint);
            }
        }
        Err(err) => {
            eprintln!("Config error: {err}");
            std::process::exit(1);
        }
    }
}
```

## Binary usage

The crate includes a helper binary `eco_config_report` that prints a JSON validation report to stdout.

Build and run:

```bash
cargo build -p eco_restoration_shard_config --bin eco_config_report
cargo run -p eco_restoration_shard_config --bin eco_config_report
```

You can also pass an explicit path:

```bash
cargo run -p eco_restoration_shard_config --bin eco_config_report -- path/to/config.yaml
```

Exit codes:

- `0` when the config is valid.
- `1` when validation, parse, IO, or not-found errors occur.

## Non-actuating guarantees

This crate:

- Only reads local files, never performs network calls.
- Does not open any control channels or actuators.
- Encodes all validation logic in pure Rust with no side effects beyond error reporting.

It is safe to use in CI, governance checks, and AI-chat tooling that needs a stable view of configuration state.
