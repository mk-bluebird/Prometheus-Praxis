// crates/fog_router/src/gateway/gateway_deps_sql.rs
#![forbid(unsafe_code)]

use anyhow::{anyhow, Result};
use rusqlite::{params, Connection};
use serde_json::Value as Json;

use crate::gateway_deps::{
    ExternalKerContext,
    FogGuardVerdictAdapter,
    GatewayDeps,
    LocalEnvelopeContext,
    WorkflowRiskDiagnostics,
    WorkflowRiskVector,
};
use cyboquatic_ecosafety::{
    CyboLane,
    KERWindow,
    LyapunovResidual,
    LyapunovWeights,
    RiskCoord,
    RiskVector,
};
use fog_router_guard::FogRouteDecision;

/// EcoHamiltonian evaluator interface.
pub struct EcoHamiltonianEvaluator {
    pub w_energy: f64,
    pub w_carbon: f64,
    pub w_topology: f64,
    pub w_biodiv: f64,
}

impl EcoHamiltonianEvaluator {
    pub fn eval(&self, r: &WorkflowRiskVector) -> f64 {
        self.w_energy * r.r_energy * r.r_energy
            + self.w_carbon * r.r_carbon * r.r_carbon
            + self.w_topology * r.r_topology * r.r_topology
            + self.w_biodiv * r.r_biodiv * r.r_biodiv
    }
}

pub struct SqlGatewayDeps<'a> {
    pub conn: &'a Connection,
    pub eco_eval: EcoHamiltonianEvaluator,
    pub last_verdict: FogGuardVerdictAdapter,
}

impl<'a> SqlGatewayDeps<'a> {
    pub fn new(conn: &'a Connection, eco_eval: EcoHamiltonianEvaluator) -> Self {
        Self {
            conn,
            eco_eval,
            last_verdict: FogGuardVerdictAdapter {
                verdict: "Stop".to_string(),
            },
        }
    }
}

impl<'a> GatewayDeps for SqlGatewayDeps<'a> {
    fn fetch_external_context(
        &mut self,
        origin_constellation: &str,
        workflow_id: &str,
    ) -> Result<ExternalKerContext> {
        let mut stmt = self.conn.prepare(
            "SELECT lane, k, e, r, kerdeployable, corridor_id
             FROM cross_constellation_index
             WHERE origin_constellation = ?1 AND workflow_id = ?2
             ORDER BY updated_at DESC
             LIMIT 1",
        )?;

        let row = stmt
            .query_row(params![origin_constellation, workflow_id], |row| {
                Ok(ExternalKerContext {
                    origin_lane: row.get::<_, String>(0)?,
                    origin_k: row.get(1)?,
                    origin_e: row.get(2)?,
                    origin_r: row.get(3)?,
                    origin_kerdeployable: row.get(4)?,
                    origin_corridor_id: row.get(5)?,
                })
            })
            .map_err(|e| anyhow!("external context not found: {e}"))?;

        Ok(row)
    }

    fn fetch_local_envelope(
        &mut self,
        node_id: &str,
        family_id: &str,
    ) -> Result<LocalEnvelopeContext> {
        let mut stmt = self.conn.prepare(
            "SELECT lane,
                    risk_json,
                    weights_json,
                    residual_value,
                    k, e, r,
                    evidencehex,
                    did
             FROM cyboquatic_envelope
             WHERE node_id = ?1 AND family_id = ?2
             ORDER BY ts DESC
             LIMIT 1",
        )?;

        let row = stmt.query_row(params![node_id, family_id], |row| {
            let lane_str: String = row.get(0)?;
            let risk_json: String = row.get(1)?;
            let weights_json: String = row.get(2)?;
            let residual_value: f64 = row.get(3)?;
            let k: f64 = row.get(4)?;
            let e: f64 = row.get(5)?;
            let r: f64 = row.get(6)?;
            let evidencehex: String = row.get(7)?;
            let did: String = row.get(8)?;

            let lane = match lane_str.as_str() {
                "Research" => CyboLane::Research,
                "Pilot" => CyboLane::Pilot,
                "Production" => CyboLane::Production,
                _ => CyboLane::Research,
            };

            let risk_val: Json = serde_json::from_str(&risk_json)?;
            let weights_val: Json = serde_json::from_str(&weights_json)?;

            let risk = RiskVector {
                rcec: RiskCoord::new_clamped(risk_val["rcec"].as_f64().unwrap_or(0.0)),
                rsat: RiskCoord::new_clamped(risk_val["rsat"].as_f64().unwrap_or(0.0)),
                rsurcharge: RiskCoord::new_clamped(
                    risk_val["rsurcharge"].as_f64().unwrap_or(0.0),
                ),
                rbiodiv: RiskCoord::new_clamped(
                    risk_val["rbiodiv"].as_f64().unwrap_or(0.0),
                ),
                rvt: RiskCoord::new_clamped(risk_val["rvt"].as_f64().unwrap_or(0.0)),
                rgovernance: RiskCoord::new_clamped(
                    risk_val["rgovernance"].as_f64().unwrap_or(0.0),
                ),
            };

            let weights = LyapunovWeights {
                wcec: weights_val["wcec"].as_f64().unwrap_or(1.0),
                wsat: weights_val["wsat"].as_f64().unwrap_or(1.0),
                wsurcharge: weights_val["wsurcharge"].as_f64().unwrap_or(1.0),
                wbiodiv: weights_val["wbiodiv"].as_f64().unwrap_or(1.0),
                wvt: weights_val["wvt"].as_f64().unwrap_or(1.0),
                wgovernance: weights_val["wgovernance"].as_f64().unwrap_or(1.0),
            };

            let residual = LyapunovResidual {
                value: residual_value,
            };

            let mut ker = KERWindow::new();
            ker.steps = 1;
            ker.lyapsafesteps = (k * ker.steps as f64) as u64;
            ker.maxrthiswindow = r;
            ker.k = k;
            ker.e = e;
            ker.r = r;

            Ok(LocalEnvelopeContext {
                lane,
                risk,
                residual,
                ker,
                evidencehex,
                did,
            })
        })?;

        Ok(row)
    }

    fn compute_workflow_risk_and_gate(
        &mut self,
        external: &ExternalKerContext,
        local: &LocalEnvelopeContext,
    ) -> Result<WorkflowRiskDiagnostics> {
        let r_energy = external.origin_k.max(local.ker.k());
        let r_carbon = external.origin_e.max(local.ker.e());
        let r_topology = external.origin_r.max(local.ker.r());
        let r_biodiv = local.risk.rbiodiv.value();

        let r_w = WorkflowRiskVector {
            r_energy,
            r_carbon,
            r_topology,
            r_biodiv,
        };

        let r_w_norm = r_energy
            .max(r_carbon)
            .max(r_topology)
            .max(r_biodiv);

        let max_local_repo_risk: f64 = {
            let mut stmt = self.conn.prepare(
                "SELECT max_risk
                 FROM local_repo_risk
                 WHERE constellation = ?1",
            )?;
            stmt.query_row(params!["local"], |row| row.get(0))?
        };

        let r_before = WorkflowRiskVector {
            r_energy: external.origin_k,
            r_carbon: external.origin_e,
            r_topology: external.origin_r,
            r_biodiv,
        };

        let v_before = self.eco_eval.eval(&r_before);
        let v_after = self.eco_eval.eval(&r_w);
        let eco_h_delta = v_after - v_before;

        Ok(WorkflowRiskDiagnostics {
            r_w,
            r_w_norm,
            max_local_repo_risk,
            eco_h_delta,
        })
    }

    fn eco_hamiltonian_gate(
        &mut self,
        diag: &WorkflowRiskDiagnostics,
    ) -> Result<bool> {
        let eps_r = 1e-6;
        let eps_v = 1e-6;
        let bound_ok = diag.r_w_norm <= diag.max_local_repo_risk + eps_r;
        let potential_ok = diag.eco_h_delta <= eps_v;
        Ok(bound_ok && potential_ok)
    }

    fn corridor_present(
        &mut self,
        node_id: &str,
        family_id: &str,
    ) -> Result<bool> {
        let mut stmt = self.conn.prepare(
            "SELECT corridor_ok
             FROM vlaneadmissibility
             WHERE node_id = ?1 AND family_id = ?2
             ORDER BY ts DESC
             LIMIT 1",
        )?;

        let corridor_ok: bool = stmt.query_row(params![node_id, family_id], |row| {
            row.get(0)
        })?;

        Ok(corridor_ok)
    }

    fn last_fog_guard_verdict(&mut self) -> FogGuardVerdictAdapter {
        self.last_verdict.clone()
    }
}
