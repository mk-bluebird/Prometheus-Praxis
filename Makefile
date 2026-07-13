# Prometheus-Praxis governance Makefile
#
# Goals:
# - Run all Kani proofs for agent-registry, Lyapunov guard, Cyboquatic, Always-Improve.
# - Run a Cyboquatic stress scenario and fail if it does not pass.
# - Compute the latest Always-Improve window and fail if invariants are violated.
#
# Usage (local or in GitHub Actions):
#   make governance-check

CARGO ?= cargo
KANI ?= kani

# Number of epochs for the synthetic Cyboquatic scenario.
CYBO_EPOCHS ?= 128

# JSON file storing Always-Improve window metrics (git-tracked or artifact).
ALWAYS_IMPROVE_WINDOWS ?= governance/always_improve_windows.json

.PHONY: all governance-check kani-all cyboquatic-check always-improve-check \
        kani-agent-registry kani-lyapunov-guard kani-cyboquatic kani-always-improve-core

all: governance-check

############################################################
# Kani proofs
############################################################

kani-agent-registry:
	@echo "[KANI] agent-registry"
	@cd crates/agent-registry && $(KANI) tests/kani_role_band_and_shard.rs

kani-lyapunov-guard:
	@echo "[KANI] prometheus-praxis-lyapunov-guard"
	@cd crates/prometheus-praxis-lyapunov-guard && $(KANI) tests/kani_ker_guard.rs

kani-cyboquatic:
	@echo "[KANI] prometheus-praxis-cyboquatic"
	@cd crates/prometheus-praxis-cyboquatic && $(KANI) tests/kani_cyboquatic_guard.rs

kani-always-improve-core:
	@echo "[KANI] always-improve-core"
	@cd crates/always-improve-core && $(KANI) tests/kani_always_improve.rs

kani-all: kani-agent-registry kani-lyapunov-guard kani-cyboquatic kani-always-improve-core

############################################################
# Cyboquatic stress scenario
############################################################

# Build a tiny CLI binary wrapper for cyboquatic-runner under examples/
# (see Rust example stub below).
cyboquatic-check:
	@echo "[CYBO] Building cyboquatic-runner example"
	@$(CARGO) build --release -p cyboquatic-runner --example cyboquatic_scenario_cli
	@echo "[CYBO] Running cyboquatic scenario (epochs=$(CYBO_EPOCHS))"
	@./target/release/examples/cyboquatic_scenario_cli $(CYBO_EPOCHS)

############################################################
# Always-Improve meta-index check
############################################################

# This assumes a small CLI under examples/ that:
# - Reads $(ALWAYS_IMPROVE_WINDOWS)
# - Appends/updates the last window for this commit
# - Verifies monotonicity and minimum trend
always-improve-check:
	@echo "[AI-META] Building always-improve-core example"
	@$(CARGO) build --release -p always-improve-core --example always_improve_cli
	@echo "[AI-META] Updating and verifying Always-Improve windows"
	@./target/release/examples/always_improve_cli $(ALWAYS_IMPROVE_WINDOWS)

############################################################
# Composite governance check target
############################################################

governance-check: kani-all cyboquatic-check always-improve-check
	@echo "[GOVERNANCE] All proofs and operational checks passed."
