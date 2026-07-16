# filename: .tools/ci_ai_plane_guard.sh
# purpose : CI entrypoint for Object 8 - reject AI-plane .aln/.sql not registered
#          against PHX_AI_DC_GOV_FRAMEWORK_20260716.

set -eu

REPO_ROOT="${REPO_ROOT:-$(pwd)}"
PHOENIX_HEX_DB="${PHOENIX_HEX_DB:?must point to phoenix_hex_registry.sqlite}"

export REPO_ROOT
export PHOENIX_HEX_DB

cargo run \
  --manifest-path "eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/crates/ai_plane_ci_guard/Cargo.toml" \
  --release
