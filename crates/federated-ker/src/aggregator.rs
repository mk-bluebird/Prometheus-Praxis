// crates/federated-ker/src/aggregator.rs
use differential_privacy::*;

struct FederatedKERAggregator {
    epsilon: f64,  // Privacy budget
    delta: f64,
    regional_contributions: Vec<RegionalKER>
}

impl FederatedKERAggregator {
    fn aggregate_with_privacy(&self) -> PrivateKERReport {
        // Laplace mechanism for differential privacy
        let sensitivity = 1.0 / self.regional_contributions.len() as f64;
        let noise_scale = sensitivity / self.epsilon;
        
        let mut aggregated_K = 0.0;
        let mut aggregated_E = 0.0;
        let mut aggregated_R = 0.0;
        
        for region in &self.regional_contributions {
            aggregated_K += region.K;
            aggregated_E += region.E;
            aggregated_R += region.R;
        }
        
        let n = self.regional_contributions.len() as f64;
        
        PrivateKERReport {
            K: (aggregated_K / n) + sample_laplace(noise_scale),
            E: (aggregated_E / n) + sample_laplace(noise_scale),
            R: (aggregated_R / n) + sample_laplace(noise_scale),
            epsilon: self.epsilon,
            participant_count: n as u32,
            bostrom_hash: self.publish_to_chain()
        }
    }
    
    fn publish_to_chain(&self) -> String {
        // Hash KER commitment to Bostrom blockchain
        let commitment = format!(
            "KER-AGG-{}-{}-participants:{}",
            Utc::now().timestamp(),
            self.epsilon,
            self.regional_contributions.len()
        );
        
        let hash = sha256(&commitment);
        
        // TODO: Submit transaction to Bostrom via cosmos-sdk
        // bostrom_client.submit_cyberlink(hash, "eco-restoration-agg")
        
        hex::encode(hash)
    }
}
