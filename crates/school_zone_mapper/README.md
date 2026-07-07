# School Zone Mapper

Generates a complete `TexasArizonaSchoolNanoCorridor2026v1.csv` shard from NCES public school data.

## Prerequisites

- Rust 1.85 or later (edition 2024)
- NCES Public School Universe CSV file (e.g., from [NCES CCD](https://nces.ed.gov/ccd/ccddata.asp))

## Usage

1. Download the latest NCES school file (e.g., `ccd_sch_029_2122_w_1a_083022.csv`).
2. Run the mapper:
   ```bash
   cargo run -- ccd_sch_029_2122_w_1a_083022.csv TexasArizonaSchoolNanoCorridor2026v1.csv
   ```
3. The output CSV can be opened directly in GitHub for a “beautiful and searchable” table.

## Output Format

The output follows the KER‑ready shard schema with exactly 46 columns per row. All risk and nano fields are initialized to safe, max‑uncertainty defaults; actual KER scoring must be applied later by the `school_zone_nanodefense` crate.

## Accuracy Notes

- County assignment is based on a hardcoded ZIP‑county lookup for known screwworm‑affected areas; other schools are marked as `OTHER`. For production use, join against the NCES LEA (district) file to obtain accurate counties.
- All schools in Arizona are currently set to `WATCH` screwworm zone due to proximity to the Texas outbreak.
```

---

These four files form a complete, compilable crate. After placing them under `eco_restoration_shard/crates/school_zone_mapper/`, you can build and run the mapper with:

```bash
cd eco_restoration_shard/crates/school_zone_mapper
cargo build --release
cargo run -- /path/to/nces_schools.csv TexasArizonaSchoolNanoCorridor2026v1.csv
