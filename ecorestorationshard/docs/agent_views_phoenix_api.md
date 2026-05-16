GET /api/agent/restoration/nodes/phx

Response 200 (application/json)

{
  "region": "Phoenix-AZ",
  "logical_name": "restoration.blastradius.phoenix.2026v1",
  "nodes": [
    {
      "node_id": "CEIM-PHX-MAR-01",
      "plane_id": "RESTORATION",
      "graph_id": "PHX-MAR-GRAPH-01",
      "restoration_radius_m": 850.0,
      "restoration_radius_hours": 720.0,
      "delta_mass_window_kg": 5400.0,
      "delta_karma_window": 3.8e8,
      "gw_risk_max": 0.9,
      "ker_band": "SAFE",
      "topology_grade": "A",
      "non_actuating": 1,
      "author_bostrom": "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
      "author_contract_id": "EcosafetyContinuityPhoenix2026v1",
      "created_utc": "2026-01-31T23:59:59Z"
    }
  ]
}

GET /api/agent/ecoperjoule/phx

Response 200 (application/json)

{
  "region": "Phoenix-AZ",
  "logical_name": "energy.ecoperjoule.policy.phoenix.2026v1",
  "windows": [
    {
      "node_id": "CQ-PHX-MAR-01",
      "domain": "water",
      "twindow_start": "2026-01-01T00:00:00Z",
      "twindow_end": "2026-01-31T23:59:59Z",
      "vt_residual": 0.13,
      "k_score": 0.94,
      "e_score": 0.91,
      "r_score": 0.13,
      "lane": "PROD",
      "ker_deployable": 1,
      "eco_per_joule": 3.17e-2,
      "theta_eco_min": 2.5e-2,
      "carbon_negative_ok": 1,
      "policy_author_bostrom": "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
      "policy_contract_id": "EcoplaneEnergyPhoenix2026v1"
    }
  ]
}

GET /api/agent/mt6883/continuity/phx

Response 200 (application/json)

{
  "region": "Phoenix-AZ",
  "logical_name": "mt6883.lane.continuity.phoenix.2026v1",
  "kernels": [
    {
      "kernel_id": "MT6883-PHX-K-001",
      "lane": "PROD",
      "k_score": 0.95,
      "e_score": 0.90,
      "r_score": 0.12,
      "vt_max": 0.13,
      "planes_ok": 1,
      "topology_ok": 1,
      "mt6883_registry_id": 42,
      "mt6883_ok": 1,
      "neuroethic_radius_hours": 24.0,
      "neuroethic_ok": 1,
      "author_bostrom": "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
      "author_contract_id": "Mt6883ContinuityPhoenix2026v1",
      "created_utc": "2026-01-15T12:00:00Z"
    }
  ]
}
