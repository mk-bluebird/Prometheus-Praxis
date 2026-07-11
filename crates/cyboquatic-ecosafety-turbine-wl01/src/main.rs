
#![forbid(unsafe_code)]

use std::fs::File;
use std::io::{BufRead, BufReader};

use cyboquatic_ecosafety_core::{LyapunovWeights, Residual};
use cyboquatic_ecosafety_turbine_wl01::{
    TurbineShard, aggregate_hydraulics, aggregate_habitat, evaluate_turbine_step,
};

fn main() {
    // Example static weights; in production, load from hex-stamped ALN corridor tables.
    let weights = LyapunovWeights::defaultcarbonnegative();

    // Read turbine shard CSV produced by pilots or design tools.
    let path = "output/phx_canal_node_wl01_turbine_shards.csv";
    let file = File::open(path).expect("cannot open turbine shard CSV");
    let reader = BufReader::new(file);

    // Skip header, iterate rows.
    let mut lines = reader.lines().skip(1);
    let mut residual_series: Vec<Residual> = Vec::new();
    let mut max_risks: Vec<f64> = Vec::new();

    // Simple previous residual; start at vt=0 for CI preview.
    let mut vt_prev = Residual { vt: 0.0 };

    while let Some(Ok(line)) = lines.next() {
        let cols: Vec<&str> = line.split(',').collect();
        if cols.len() < 24 {
            eprintln!("invalid row: {}", line);
            continue;
        }

        // Minimal parse; in real code, use serde + strong types.
        let rsurcharge = cols[2].parse::<f64>().unwrap_or(1.0);
        let rfishshear = cols[6].parse::<f64>().unwrap_or(1.0);
        let rramp = cols[10].parse::<f64>().unwrap_or(1.0);
        let rturbulence = cols[11].parse::<f64>().unwrap_or(1.0);
        let rhabitat = cols[12].parse::<f64>().unwrap_or(1.0);
        let rbiodiversity = cols[16].parse::<f64>().unwrap_or(1.0);
        let rpathogen = cols[17].parse::<f64>().unwrap_or(1.0);
        let renergy = cols[18].parse::<f64>().unwrap_or(1.0);
        let rcarbon = cols[19].parse::<f64>().unwrap_or(1.0);
        let rmaterials = cols[20].parse::<f64>().unwrap_or(1.0);
        let rsigma = cols[21].parse::<f64>().unwrap_or(1.0);

        let shard = TurbineShard {
            qm3s: cols[0].parse::<f64>().unwrap_or(0.0),
            hlrmperh: cols[1].parse::<f64>().unwrap_or(0.0),
            rsurcharge: RiskCoord::new(rsurcharge),
            rcavitation: RiskCoord::new(cols[3].parse::<f64>().unwrap_or(0.0)),
            roverpressure: RiskCoord::new(cols[4].parse::<f64>().unwrap_or(0.0)),
            renergy_hydraulic: RiskCoord::new(cols[5].parse::<f64>().unwrap_or(0.0)),
            headm: cols[7].parse::<f64>().unwrap_or(0.0),
            vtip_ms: cols[8].parse::<f64>().unwrap_or(0.0),
            shear_tau_pa: cols[9].parse::<f64>().unwrap_or(0.0),
            delta_p_pa: cols[22].parse::<f64>().unwrap_or(0.0),
            lethality_index: cols[23].parse::<f64>().unwrap_or(0.0),
            rfishshear: RiskCoord::new(rfishshear),
            ramp_rate_du_dt: cols[13].parse::<f64>().unwrap_or(0.0),
            turbulence_I: cols[14].parse::<f64>().unwrap_or(0.0),
            rramp: RiskCoord::new(rramp),
            rturbulence: RiskCoord::new(rturbulence),
            rhabitat: RiskCoord::new(rhabitat),
            rbiodiversity: RiskCoord::new(rbiodiversity),
            rpathogen: RiskCoord::new(rpathogen),
            renergy: RiskCoord::new(renergy),
            rcarbon: RiskCoord::new(rcarbon),
            rmaterials: RiskCoord::new(rmaterials),
            rsigma: RiskCoord::new(rsigma),
            vt: vt_prev.vt,
        };

        // Hard corridor checks: no-corridor-no-build + gold bands.
        let gold_rsurcharge = 0.7_f64; // load from PhoenixCanalHydraulics2026v1 in production
        let gold_rfishshear = 0.5_f64; // from PhoenixTurbineFishShear2026v1
        let gold_rhabitat = 0.5_f64;   // from PhoenixTurbineRampTurbulence2026v1
        let gold_rpathogen = 0.5_f64;  // from PhoenixPathogen2026v1

        if shard.rsurcharge.value > gold_rsurcharge
            || shard.rfishshear.value > gold_rfishshear
            || shard.rhabitat.value > gold_rhabitat
            || shard.rpathogen.value > gold_rpathogen
        {
            eprintln!("CI BLOCK: corridor violation in turbine shard row: {}", line);
            std::process::exit(1);
        }

        // Aggregate hydraulics and habitat for safestep evaluation.
        let rhydraulics = aggregate_hydraulics(
            &shard,
            HydraulicWeights {
                w_surcharge: 1.0,
                w_cavitation: 1.0,
                w_overpressure: 1.0,
                w_energy_hydraulic: 1.0,
            },
        );
        let rbiology = aggregate_habitat(
            &shard,
            HabitatWeights {
                w_ramp: 1.0,
                w_turbulence: 1.0,
            },
        );

        // Safestep: Vt_next <= Vt_prev and all coords <= 1.0.
        let (residual_next, decision) =
            evaluate_turbine_step(&shard, weights, vt_prev, rhydraulics, rbiology);

        if decision == SafeStepDecision::Reject {
            eprintln!("CI BLOCK: safestep violation for row: {}", line);
            std::process::exit(1);
        }

        residual_series.push(residual_next);
        let max_r = renergy
            .max(rhydraulics.value)
            .max(rbiology.value)
            .max(rbiodiversity)
            .max(rpathogen)
            .max(rsigma);
        max_risks.push(max_r);
        vt_prev = residual_next;
    }

    // Compute KER window for the shard series; enforce production band.
    if let Some(ker) = cyboquatic_ecosafety_turbine_wl01::ker_window_for_turbine(
        &residual_series,
        &max_risks,
    ) {
        if !ker.is_deployable() {
            eprintln!(
                "CI BLOCK: KER window outside production band K={:.3}, E={:.3}, R={:.3}",
                ker.k, ker.e, ker.r
            );
            std::process::exit(1);
        }
    } else {
        eprintln!("CI BLOCK: insufficient residual data for KER window");
        std::process::exit(1);
    }

    println!("PHX-CANAL-NODE-WL-01 turbine shards pass corridor and safestep checks.");
}
