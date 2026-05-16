
// filename: src/bci/metrics.rs
// destination: eco_restoration_shard/src/bci/metrics.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

#[derive(Clone, Debug)]
pub struct NeuroMetrics {
    pub fearrate: f32,
    pub coherence: f32,
}

pub trait BciInterface {
    fn verify_connection(&self) -> super::health::BciHealthStatus;
    fn read_neuro_metrics(&self) -> NeuroMetrics;
    fn clone_interface(&self) -> Box<dyn BciInterface + Send>;
}
