// filename: eco_restoration_index/src/api.rs

use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlastRadiusLink {
    pub linkid: i64,
    pub sourcetype: String,
    pub sourceid: i64,
    pub targettype: String,
    pub targetid: String,
    pub impacttype: String,
    pub impactscore: f64,
    pub vtsensitivity: Option<f64>,
    pub notes: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadWindowSummary {
    pub nodeid: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub total_requests_j: f64,
    pub total_surplus_j: f64,
    pub accepted_requests_j: f64,
    pub rejected_requests_j: f64,
    pub rerouted_requests_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_rcarbon: Option<f64>,
    pub mean_rbiodiv: Option<f64>,
    pub accept_fraction: f64,
    pub mean_delta_vt: f64,
}

pub fn list_blast_radius_for_shard(
    conn: &Connection,
    shardid: i64,
) -> SqlResult<Vec<BlastRadiusLink>> {
    let mut stmt = conn.prepare(
        "SELECT linkid, sourcetype, sourceid, targettype, targetid,
                impacttype, impactscore, vtsensitivity, notes
         FROM blastradiuslink
         WHERE sourcetype = 'SHARD' AND sourceid = ?1
         ORDER BY impacttype, targettype, targetid",
    )?;

    let rows = stmt.query_map(params![shardid], |row| {
        Ok(BlastRadiusLink {
            linkid: row.get(0)?,
            sourcetype: row.get(1)?,
            sourceid: row.get(2)?,
            targettype: row.get(3)?,
            targetid: row.get(4)?,
            impacttype: row.get(5)?,
            impactscore: row.get(6)?,
            vtsensitivity: row.get(7)?,
            notes: row.get(8)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

pub fn summarize_workload_window(
    conn: &Connection,
    nodeid: &str,
    tstart_utc: &str,
    tend_utc: &str,
) -> SqlResult<WorkloadWindowSummary> {
    let mut stmt = conn.prepare(
        "SELECT ereqj, esurplusj, rcarbon, rbiodiv, vtbefore, vtafter, decision
         FROM workloadledger
         WHERE nodeid = ?1
           AND timestamputc >= ?2
           AND timestamputc <= ?3",
    )?;

    let rows = stmt.query_map(params![nodeid, tstart_utc, tend_utc], |row| {
        Ok((
            row.get::<_, f64>(0)?,
            row.get::<_, f64>(1)?,
            row.get::<_, Option<f64>>(2)?,
            row.get::<_, Option<f64>>(3)?,
            row.get::<_, f64>(4)?,
            row.get::<_, f64>(5)?,
            row.get::<_, String>(6)?,
        ))
    })?;

    let mut total_req = 0.0;
    let mut total_surplus = 0.0;
    let mut accepted_req = 0.0;
    let mut rejected_req = 0.0;
    let mut rerouted_req = 0.0;
    let mut sum_vt_before = 0.0;
    let mut sum_vt_after = 0.0;
    let mut count = 0_u64;
    let mut sum_rcarbon = 0.0;
    let mut sum_rbiodiv = 0.0;
    let mut count_rcarbon = 0_u64;
    let mut count_rbiodiv = 0_u64;
    let mut accept_count = 0_u64;

    for r in rows {
        let (ereqj, esurplusj, rcarbon, rbiodiv, vtbefore, vtafter, decision) = r?;

        total_req += ereqj;
        total_surplus += esurplusj;
        sum_vt_before += vtbefore;
        sum_vt_after += vtafter;
        count += 1;

        if let Some(rc) = rcarbon {
            sum_rcarbon += rc;
            count_rcarbon += 1;
        }
        if let Some(rb) = rbiodiv {
            sum_rbiodiv += rb;
            count_rbiodiv += 1;
        }

        match decision.as_str() {
            "ACCEPT" => {
                accepted_req += ereqj;
                accept_count += 1;
            }
            "REJECT" => {
                rejected_req += ereqj;
            }
            "REROUTE" => {
                rerouted_req += ereqj;
            }
            _ => {}
        }
    }

    if count == 0 {
        return Ok(WorkloadWindowSummary {
            nodeid: nodeid.to_string(),
            window_start_utc: tstart_utc.to_string(),
            window_end_utc: tend_utc.to_string(),
            total_requests_j: 0.0,
            total_surplus_j: 0.0,
            accepted_requests_j: 0.0,
            rejected_requests_j: 0.0,
            rerouted_requests_j: 0.0,
            mean_vt_before: 0.0,
            mean_vt_after: 0.0,
            mean_rcarbon: None,
            mean_rbiodiv: None,
            accept_fraction: 0.0,
            mean_delta_vt: 0.0,
        });
    }

    let mean_vt_before = sum_vt_before / (count as f64);
    let mean_vt_after = sum_vt_after / (count as f64);
    let mean_rcarbon = if count_rcarbon > 0 {
        Some(sum_rcarbon / (count_rcarbon as f64))
    } else {
        None
    };
    let mean_rbiodiv = if count_rbiodiv > 0 {
        Some(sum_rbiodiv / (count_rbiodiv as f64))
    } else {
        None
    };
    let accept_fraction = if total_req > 0.0 {
        accepted_req / total_req
    } else {
        0.0
    };
    let mean_delta_vt = mean_vt_after - mean_vt_before;

    Ok(WorkloadWindowSummary {
        nodeid: nodeid.to_string(),
        window_start_utc: tstart_utc.to_string(),
        window_end_utc: tend_utc.to_string(),
        total_requests_j: total_req,
        total_surplus_j: total_surplus,
        accepted_requests_j: accepted_req,
        rejected_requests_j: rejected_req,
        rerouted_requests_j: rerouted_req,
        mean_vt_before,
        mean_vt_after,
        mean_rcarbon,
        mean_rbiodiv,
        accept_fraction,
        mean_delta_vt,
    })
}
