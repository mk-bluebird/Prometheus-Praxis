// filename src/rtopology.rs
// destination Eco-Fort/src/rtopology.rs

use rusqlite::{params, Connection, Result as SqlResult};

pub fn compute_rtopology(conn: &Connection) -> SqlResult<f64> {
    let mut n_repos_checked: i64 = 0;
    let mut n_missing_manifest: i64 = 0;
    let mut n_mislabel_role: i64 = 0;
    let mut n_mislabel_nonactuating: i64 = 0;

    let mut stmt = conn.prepare(
        "SELECT r.repoid, r.name, r.roleband, r.visibility
         FROM repo r",
    )?;
    let mut rows = stmt.query([])?;

    while let Some(row) = rows.next()? {
        let repoid: i64 = row.get(0)?;
        let name: String = row.get(1)?;
        let roleband: String = row.get(2)?;
        let _visibility: String = row.get(3)?;
        n_repos_checked += 1;

        let has_manifest: bool = conn
            .prepare(
                "SELECT COUNT(1)
                 FROM econetrepoindex
                 WHERE reponame = ?1",
            )?
            .query_row(params![name.clone()], |r| r.get::<_, i64>(0))
            .map(|c| c > 0)
            .unwrap_or(false);

        if !has_manifest {
            n_missing_manifest += 1;
            continue;
        }

        let (lane_nonactuatingonly,): (i64,) = conn
            .prepare(
                "SELECT COALESCE(nonactuatingonly, 0)
                 FROM econetrepoindex
                 WHERE reponame = ?1",
            )?
            .query_row(params![name.clone()], |r| r.get(0))
            .unwrap_or((0,));

        let mut layers_stmt = conn.prepare(
            "SELECT layername, layertier
             FROM econetlayer
             WHERE reponame = ?1",
        )?;
        let mut layers_rows = layers_stmt.query(params![name.clone()])?;

        let mut has_kernel_layer = false;
        while let Some(lrow) = layers_rows.next()? {
            let layertier: String = lrow.get(1)?;
            if layertier.eq_ignore_ascii_case("KERNEL") {
                has_kernel_layer = true;
            }
        }

        let roleband_upper = roleband.to_uppercase();
        let is_engine_band = roleband_upper == "ENGINE";
        let nonactuating_flagged = lane_nonactuatingonly != 0;

        if is_engine_band && nonactuating_flagged {
            n_mislabel_nonactuating += 1;
        }

        if !is_engine_band && has_kernel_layer {
            n_mislabel_role += 1;
        }
    }

    let w_missing = 0.6_f64;
    let w_mislabel_role = 0.3_f64;
    let w_mislabel_nonact = 0.1_f64;

    let itopology = w_missing * n_missing_manifest as f64
        + w_mislabel_role * n_mislabel_role as f64
        + w_mislabel_nonact * n_mislabel_nonactuating as f64;

    if itopology <= 0.0 {
        return Ok(0.0);
    }

    let max_itopology = if n_repos_checked > 0 {
        w_missing * n_repos_checked as f64
    } else {
        1.0
    };

    let r_raw = itopology / max_itopology;
    let r_clamped = if r_raw < 0.0 {
        0.0
    } else if r_raw > 1.0 {
        1.0
    } else {
        r_raw
    };

    let rtopology = if r_clamped <= 0.1 {
        r_clamped / 0.1
    } else if r_clamped <= 0.3 {
        1.0
    } else if r_clamped <= 0.6 {
        1.0 + (r_clamped - 0.3) / 0.3
    } else {
        2.0
    };

    Ok(rtopology)
}
