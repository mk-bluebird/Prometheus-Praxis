
use rusqlite::{Connection, Row, Result as SqlResult};
use ecosafety_core::hydraulics::{HydraulicRiskCoords, HydraulicWeights, HYDRAULIC_WEIGHTS_PHX_V1, vt_hydraulics};
use crate::reach_row::HydraulicsReachRow;

fn map_row(row: &Row) -> SqlResult<HydraulicsReachRow> {
    Ok(HydraulicsReachRow {
        reach_id:           row.get("reach_id")?,
        site_code:          row.get("site_code")?,
        lat_deg:            row.get("lat_deg")?,
        lon_deg:            row.get("lon_deg")?,
        q_m3s:              row.get("q_m3s")?,
        hlr_m_per_h:        row.get("hlr_m_per_h")?,
        surcharge_index:    row.get("surcharge_index")?,
        rhydraulics:        row.get("rhydraulics")?,
        rcalib:             row.get("rcalib")?,
        rsigma:             row.get("rsigma")?,
        vt_hydraulics:      row.get("vt_hydraulics")?,
        k_metric:           row.get("k_metric")?,
        e_metric:           row.get("e_metric")?,
        r_metric:           row.get("r_metric")?,
        corridor_id:        row.get("corridor_id")?,
        weights_profile_id: row.get("weights_profile_id")?,
        evidence_hex:       row.get("evidence_hex")?,
    })
}

pub fn load_reach_rows(conn: &Connection) -> SqlResult<Vec<HydraulicsReachRow>> {
    let mut stmt = conn.prepare(
        "SELECT * FROM cyboquatic_hydraulics_reach_2026"
    )?;

    let rows_iter = stmt.query_map([], map_row)?;

    let mut out = Vec::new();
    for row_res in rows_iter {
        let mut row = row_res?;

        // recompute Vt from risk coords and canonical weights
        let rc = row.risk_coords();
        let vt = vt_hydraulics(rc, HYDRAULIC_WEIGHTS_PHX_V1);

        // Optional: overwrite or assert consistency with stored vt_hydraulics
        row.vt_hydraulics = vt;

        out.push(row);
    }
    Ok(out)
}
