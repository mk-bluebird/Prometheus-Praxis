// filename: src/signer/panic_watchdog.rs
// destination: eco_restoration_shard/src/signer/panic_watchdog.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::bci::{BciInterface, NeuroMetrics};
use crate::rpc::bostrom_client::BostromClient;
use crate::safety::errors::SabotageError;

pub struct PanicWatchdog<B: BciInterface, C: BostromClient> {
    pub bci: B,
    pub client: C,
    pub host_address: String,
    pub neurochannel_id: String,
    pub panic_threshold_fearrate: f32,
    pub coherence_min: f32,
    pub running: Arc<Mutex<bool>>,
}

impl<B: BciInterface + Send + 'static, C: BostromClient + Send + 'static> PanicWatchdog<B, C> {
    pub fn start(&self) {
        let bci = self.bci.clone_interface();
        let client = self.client.clone_client();
        let host = self.host_address.clone();
        let channel = self.neurochannel_id.clone();
        let fear_limit = self.panic_threshold_fearrate;
        let coherence_min = self.coherence_min;
        let running = self.running.clone();

        thread::spawn(move || loop {
            {
                let flag = running.lock().unwrap();
                if !*flag {
                    break;
                }
            }

            let metrics = bci.read_neuro_metrics();
            if metrics.fearrate > fear_limit || metrics.coherence < coherence_min {
                if let Err(e) = send_force_close(&client, &host, &channel, &metrics) {
                    eprintln!("Failed to send MsgForceCloseNeuroChannelOnSuspicion: {e}");
                }
                break;
            }

            thread::sleep(Duration::from_millis(500));
        });
    }
}

fn send_force_close<C: BostromClient>(
    client: &C,
    host_address: &str,
    neurochannel_id: &str,
    metrics: &NeuroMetrics,
) -> Result<(), SabotageError> {
    let msg = client.build_force_close_neurochannel_msg(
        host_address,
        neurochannel_id,
        metrics.fearrate,
        metrics.coherence,
    );
    client.broadcast_tx(msg)?;
    Ok(())
}

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
