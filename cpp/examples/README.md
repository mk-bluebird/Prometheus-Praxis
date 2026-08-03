# C++ Eco-Restoration Examples

This folder contains small programs demonstrating how to call individual eco-restoration modules:

- `material_eco_example.cpp`:
  - Uses `eco_restoration::MaterialTestParams` and `compute_material_eco_impact_cpp` to compute KER and biodegradability scores for a test material, matching ISO/OECD metrics in `eco_material_catalog`.[59][66][72]

- `pfas_corridor_example.cpp`:
  - Demonstrates a single PFAS corridor step using `eco_pfas::PFASState` and `step_pfas_corridor`, aligned with qpudatashard PFAS fate semantics and the recursive SQLite PFAS corridor.[59]

- `blast_radius_example.cpp`:
  - Calls `phoenix_canal::run_blast_radius_step` to obtain normalized blast-radius risk coordinates for hydraulics, energy, and topology, suitable for insertion into KER/Lyapunov risk vectors for Phoenix hex simulations.[59]

- `ker_example.cpp`:
  - Shows KER/Lyapunov coupling by computing `V_t = sum_j w_j r_j^2` and `s = k * e - r_max` for a representative hex, following the KER window semantics and non-offsettable plane rules from the governance markdown.[59][78]

These examples are intended as minimal, compilable entry points for new coders and AI agents to understand and exercise eco-restoration C++ modules with sample configurations and small synthetic datasets.
