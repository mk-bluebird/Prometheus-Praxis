package prometheus_praxis.safety

default allow = false

# Input example:
# {
#   "lane": "PRODUCTION",
#   "kernel": {"nonactuating": true},
#   "proofofresearch": true,
#   "psat": true,
#   "kani_verified": true,
#   "action": {"type": "governance_decision"},
#   "origin": {"repo": "eco_restoration_shard"}
# }

nonactuating_kernel {
  input.kernel.nonactuating == true
}

production_safe {
  input.lane == "PRODUCTION"
  input.proofofresearch == true
  input.psat == true
  input.kani_verified == true
}

# Deny any direct actuation through this repo
is_actuation_request {
  input.action.type == "actuate_hardware"
}

# Main allow rule: governance decisions only, under safety conditions
allow {
  nonactuating_kernel
  not is_actuation_request

  # Research / Pilot lanes: nonactuating kernel is enough
  input.lane == "RESEARCH"
}

allow {
  nonactuating_kernel
  not is_actuation_request

  input.lane == "PILOT"
}

allow {
  nonactuating_kernel
  not is_actuation_request

  # Production lane: requires extra proofs
  production_safe
}
