use anyhow::Result;
use school_zone_mapper::map_schools_to_shard;
use std::fs::File;

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <nces_school_file.csv> <output_shard.csv>", args[0]);
        std::process::exit(1);
    }
    let input = File::open(&args[1])?;
    let output = File::create(&args[2])?;
    map_schools_to_shard(input, output, "2026-07-01T00:00:00Z", "2026-07-31T23:59:59Z")?;
    Ok(())
}
