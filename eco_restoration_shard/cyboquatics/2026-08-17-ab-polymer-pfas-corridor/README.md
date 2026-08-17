# PHX-POLY-PFAS-CORRIDOR-20260817

Non-actuating ecological-restoration analysis set for:

- Biodegradable-polymer fragmentation under hydrolysis, photolysis, and surface erosion.
- PFAS precursor-chain clearance screening using first-order state dynamics.
- Cold-survival microbial-state validation using a practical Lyapunov bound.
- PFAS residual-corridor persistence using an exponentially contractive threshold.

Files:
- cpp/polymer_fragmentation_stiffness.cpp
- cpp/pfas_chain_eigenanalysis.cpp
- lua/cold_survival_lyapunov.lua
- lua/polymer_fragmentation_stiffness.lua
- sql/polymer_pfas_lyapunov_corridor.sql
- aln/polymer_pfas_lyapunov_corridor.aln2

Build and run:

c++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  cpp/polymer_fragmentation_stiffness.cpp -o polymer_fragmentation_stiffness

c++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  cpp/pfas_chain_eigenanalysis.cpp -o pfas_chain_eigenanalysis

lua lua/polymer_fragmentation_stiffness.lua \
  <k_hyd_per_day> <k_frag_per_day> <alpha> <initial_particle_count> <step_days> <steps>

lua lua/cold_survival_lyapunov.lua \
  <lambda_per_day> <epsilon_per_day> <initial_V> <temperature_C> [<temperature_C> ...]

Critical PFAS clearance result:

For the closed lower-bidiagonal chain:
C_i' = k_(i-1) C_(i-1) - k_i C_i,

the eigenvalues are -k_i. Therefore every modeled state clears asymptotically only when
every terminal loss rate is positive. A precursor half-life alone does not define a universal
accumulation threshold: a terminal product with k_terminal = 0 retains mass in the modeled
system regardless of the upstream precursor half-life.

For a finite management horizon T and desired retained fraction q in (0,1), a single
first-order terminal compartment must satisfy:
k_terminal >= -ln(q) / T
and
half_life_terminal <= T * ln(2) / -ln(q).

Bimodality screening result:

The scalar equation dN/dt = k_hyd*N - k_frag*N^alpha tracks total particle count and cannot
prove a bimodal size distribution by itself. A bimodal distribution requires at least two
size-resolved populations or bins. This bundle uses a two-bin conservative surrogate:
- hydrolysis converts coarse particles into fine particles;
- fragmentation transfers coarse particles to fine particles;
- surface erosion removes fine-particle mass/count from the retained solid pool.

The screening flag is raised only when both retained coarse and retained fine populations
exceed an operator-specified observability threshold. It is a signal to collect size-resolved
measurements, not a physical proof of bimodality.

All outputs are decision support, not validation of a remediation process, regulatory result,
or authorization for any field action.
