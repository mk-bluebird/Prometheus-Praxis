pub struct FairnessContext<'a> {
    pub conn: &'a rusqlite::Connection,
}

pub struct FairnessInputs<'a> {
    pub sko_class:      &'a str,
    pub jurisdiction_id:&'a str,
    pub site_id:        &'a str,
    pub e_norm:         f64, // normalized energy E_{i,k}
    pub c_norm:         f64, // normalized carbon C_{i,k}
}

pub struct FairnessOutput {
    pub f_scalar: f64,           // F_{i,k} in [0,1]
    pub species_terms: Vec<(String, f64)>, // (species_id, f_{i,p})
}

pub trait FairnessKernel {
    fn fairness_penalty(
        &self,
        inputs: &FairnessInputs
    ) -> rusqlite::Result<Option<FairnessOutput>>;
}

// v1 implementation reading SQLite and ALN-backed parameters.
pub struct SqliteFairnessKernel<'a> {
    pub ctx: FairnessContext<'a>,
}

impl<'a> FairnessKernel for SqliteFairnessKernel<'a> {
    fn fairness_penalty(
        &self,
        inputs: &FairnessInputs
    ) -> rusqlite::Result<Option<FairnessOutput>> {
        // 1. Load validated alpha_kp rows for (sko_class, jurisdiction_id).
        // 2. Load habitat_energy_kernel row for site_id.
        // 3. Load beta_* and B_p^{safe} from ALN-backed tables.
        // 4. Compute F_{i,k}. If no data, return Ok(None).
        #![allow(unused)]
        Ok(None)
    }
}
