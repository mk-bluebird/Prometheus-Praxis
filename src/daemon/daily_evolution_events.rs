// filename: src/daemon/daily_evolution_events.rs
// destination: eco_restoration_shard/src/daemon/daily_evolution_events.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use std::process::Command;
use std::time::Duration;

use serde::Deserialize;
use tendermint_rpc::client::WebSocketClient;
use tendermint_rpc::query::Query;
use tendermint_rpc::SubscriptionClient;

#[derive(Debug, Deserialize)]
struct TxResult {
    pub tx: String,
}

pub struct DailyEvolutionDaemon {
    pub rpc_endpoint: String,
}

impl DailyEvolutionDaemon {
    pub fn run(&self) -> anyhow::Result<()> {
        let (mut client, driver) = WebSocketClient::new(self.rpc_endpoint.clone())?;
        std::thread::spawn(move || {
            if let Err(e) = driver.run() {
                eprintln!("Tendermint driver error: {e}");
            }
        });

        let query = Query::from("tm.event='NewBlock'");
        let mut subscription = client.subscribe(query)?;

        loop {
            if let Some(event) = futures::executor::block_on(subscription.next()) {
                if let Ok(ev) = event {
                    self.handle_new_block_event(ev.data)?;
                }
            }
        }
    }

    fn handle_new_block_event(
        &self,
        data: tendermint_rpc::event::EventData,
    ) -> anyhow::Result<()> {
        if let tendermint_rpc::event::EventData::NewBlock { block, .. } = data {
            for tx in block.data.txs.iter() {
                if self.tx_matches_sabotage_pattern(tx.as_bytes()) {
                    self.trigger_local_alarm();
                    self.run_emergency_patch()?;
                }
            }
        }
        Ok(())
    }

    fn tx_matches_sabotage_pattern(&self, tx_bytes: &[u8]) -> bool {
        let tx_string = base64::encode(tx_bytes);
        tx_string.contains("SabotageAlert")
    }

    fn trigger_local_alarm(&self) {
        let _ = Command::new("notify-send")
            .arg("Sabotage Alert")
            .arg("Potential sabotage detected in NewBlock event")
            .status();
    }

    fn run_emergency_patch(&self) -> anyhow::Result<()> {
        let status = Command::new("cargo")
            .arg("run")
            .arg("-p")
            .arg("bostrom-signer")
            .arg("--")
            .arg("emergency-patch")
            .status()?;

        if !status.success() {
            eprintln!("Failed to run emergency patch");
        }
        Ok(())
    }
}
