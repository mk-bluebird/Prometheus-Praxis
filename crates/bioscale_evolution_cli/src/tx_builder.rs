// filename: crates/bioscale_evolution_cli/src/tx_builder.rs
// destination: ecorestorationshard/crates/bioscale_evolution_cli/src/tx_builder.rs

use super::HealthcareUpgrade;
use cosmos_sdk_proto::cosmos::bank::v1beta1::MsgSend as MsgTransfer;
use cosmos_sdk_proto::Any;

const ECO_DAO_ADDRESS: &str = "bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc";
const MAX_R_CAP: f64 = 1.0;

pub struct TxBatch {
    pub msgs: Vec<Any>,
}

impl TxBatch {
    pub fn new() -> Self {
        Self { msgs: Vec::new() }
    }

    pub fn push(&mut self, msg: Any) {
        self.msgs.push(msg);
    }
}

pub fn append_upgrade_msgs(
    batch: &mut TxBatch,
    upgrade: &HealthcareUpgrade,
    eco_mult_base: f64,
    r_today: f64,
) -> Result<(), Box<dyn std::error::Error>> {
    // Always add the core upgrade message.
    batch.push(upgrade.msg_any.clone());

    // Only apply eco multiplier for healthcare eco_domain tags.
    if !upgrade.eco_domain.eq_ignore_ascii_case("healthcare") {
        return Ok(());
    }

    if eco_mult_base <= 0.0 || r_today <= 0.0 {
        return Ok(());
    }

    let gas_cost = upgrade.gas_cost as f64;
    let eco_multiplier = eco_mult_base;
    let r_ratio = (r_today / MAX_R_CAP).clamp(0.0, 1.0);

    let eco_contribution_boot = gas_cost * eco_multiplier * r_ratio;

    if eco_contribution_boot <= 0.0 {
        return Ok(());
    }

    let amount_str = format!("{:.0}", eco_contribution_boot.round());

    let msg_transfer = MsgTransfer {
        from_address: String::new(), // filled by signer from host address context
        to_address: ECO_DAO_ADDRESS.to_string(),
        amount: vec![cosmos_sdk_proto::cosmos::base::v1beta1::Coin {
            denom: "boot".to_string(),
            amount: amount_str,
        }],
    };

    let mut value = Vec::new();
    msg_transfer.encode(&mut value)?;

    let any = Any {
        type_url: "/cosmos.bank.v1beta1.MsgSend".to_string(),
        value,
    };

    batch.push(any);
    Ok(())
}
