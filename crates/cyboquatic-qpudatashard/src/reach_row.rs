
#[derive(Debug, Clone)]
pub struct HydraulicsReachRow {
    pub reach_id:           i64,
    pub site_code:          String,
    pub lat_deg:            f64,
    pub lon_deg:            f64,

    pub q_m3s:              f64,
    pub hlr_m_per_h:        f64,
    pub surcharge_index:    f64,

    pub rhydraulics:        f32,
    pub rcalib:             f32,
    pub rsigma:             f32,

    pub vt_hydraulics:      f64,
    pub k_metric:           f32,
    pub e_metric:           f32,
    pub r_metric:           f32,

    pub corridor_id:        String,
    pub weights_profile_id: String,
    pub evidence_hex:       String,
}

impl HydraulicsReachRow {
    pub fn risk_coords(&self) -> HydraulicRiskCoords {
        HydraulicRiskCoords {
            rhydraulics: self.rhydraulics,
            rcalib:      self.rcalib,
            rsigma:      self.rsigma,
        }
    }
}
