# PHX-CANAL-BR-20260816

Daily ecological-restoration artifact set for 2026-08-16.

Domain: (g) surcharge-breach blast-radius assessment.
Sub-task: Deterministic canal-node exposure zoning with conservative machinery
dispatch guidance, SQLite integrity constraints, and cross-language readers.

Files:
- sql/canal_surcharge_blast_radius_20260816.sql
- cpp/canal_blast_radius.cpp
- java/CanalBlastRadius.java
- kotlin/CanalBlastRadius.kt
- lua/canal_blast_radius.lua
- aln/canal_surcharge_blast_radius_20260816.aln

SQLite initialization:
sqlite3 canal_blast_radius.db < sql/canal_surcharge_blast_radius_20260816.sql

C++ build:
c++ -std=c++17 -O2 -Wall -Wextra -pedantic cpp/canal_blast_radius.cpp -o canal_blast_radius

Java build:
javac java/CanalBlastRadius.java

Kotlin build:
kotlinc kotlin/CanalBlastRadius.kt -include-runtime -d canal-blast-radius.jar

Lua run:
lua lua/canal_blast_radius.lua

All calculators accept:
<breach_flow_lps> <surcharge_duration_s> <bank_sensitivity_0_to_1>
<distance_m> <energyreqJ> <delta_vt>

The tools return SAFE, CAUTION, or EXCLUDE. EXCLUDE means machinery should not
enter or operate in the assessed zone without site-specific human approval.

This bundle is decision support only. It does not replace hydrologic modeling,
field inspection, permits, or emergency-response procedures.
