// File: cyboquatic-core/src/thresholds.rs
use std::{env, fs, path::Path};

#[derive(Clone, Copy)]
pub struct Thresholds {
    pub delta_vt_max: f64,
    pub fog_min_confidence: f64,
    pub eco_impact_min: f64,
    pub knowledge_factor_min: f64,
}

impl Default for Thresholds {
    fn default() -> Self {
        Self {
            delta_vt_max: 0.35,
            fog_min_confidence: 0.75,
            eco_impact_min: 0.60,
            knowledge_factor_min: 0.75,
        }
    }
}

impl Thresholds {
    pub fn load() -> Result<Self, String> {
        let path = env::var("CYBOQUATIC_THRESHOLDS")
            .unwrap_or_else(|_| "config/thresholds.toml".into());
        let source = fs::read_to_string(Path::new(&path))
            .map_err(|error| format!("cannot read {path}: {error}"))?;
        let mut value = Self::default();

        for line in source.lines() {
            let line = line.split('#').next().unwrap_or("").trim();
            if line.is_empty() {
                continue;
            }
            let (key, raw) = line.split_once('=')
                .ok_or_else(|| format!("invalid threshold line: {line}"))?;
            let number = raw.trim().parse::<f64>()
                .map_err(|_| format!("invalid threshold for {}", key.trim()))?;
            if !number.is_finite() || !(0.0..=1.0).contains(&number) {
                return Err(format!("threshold outside [0,1]: {}", key.trim()));
            }
            match key.trim() {
                "delta_vt_max" => value.delta_vt_max = number,
                "fog_min_confidence" => value.fog_min_confidence = number,
                "eco_impact_min" => value.eco_impact_min = number,
                "knowledge_factor_min" => value.knowledge_factor_min = number,
                unknown => return Err(format!("unknown threshold: {unknown}")),
            }
        }
        Ok(value)
    }
}
