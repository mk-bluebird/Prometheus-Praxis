use serde::{Deserialize, Serialize};
use veritas_chain_client::VeritasChainClient;
use bioscale_metrics::Bounded01;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DecisionLog {
    pub decision_id: String,
    pub task_id: String,
    pub allowed: bool,
    pub reasons: String,
    pub hextrace: String,
    pub timestamp_utc: String,
    pub k_e_r_vector: String,
}

pub struct DecisionLogger<C> {
    client: C,
}

impl<C: VeritasChainClient> DecisionLogger<C> {
    pub fn new(client: C) -> Self {
        Self { client }
    }

    pub fn log_decision(&self, log: &DecisionLog) -> anyhow::Result<()> {
        let payload = serde_json::to_vec(log)?;
        self.client.append(&payload)?;
        Ok(())
    }

    pub fn build_k_e_r_vector(k: Bounded01, e: Bounded01, r: Bounded01) -> String {
        format!("K:{:.3};E:{:.3};R:{:.3}", k.into_inner(), e.into_inner(), r.into_inner())
    }
}
