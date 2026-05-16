// filename: x/evolutionwindow/agent_gas.rs
// destination: eco_restoration_shard/x/evolutionwindow/agent_gas.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use cosmwasm_std::Uint128;

pub struct AgentGasStore<'a> {
    pub store: &'a dyn KVStore,
}

impl<'a> AgentGasStore<'a> {
    pub fn key(ticket_id: &str) -> Vec<u8> {
        let mut k = Vec::new();
        k.extend_from_slice(b"agent_gas_used/");
        k.extend_from_slice(ticket_id.as_bytes());
        k
    }

    pub fn get_gas_used(&self, ticket_id: &str) -> Uint128 {
        if let Some(bz) = self.store.get(&Self::key(ticket_id)) {
            Uint128::from(u128::from_le_bytes(Self::bytes_to_u128(&bz)))
        } else {
            Uint128::zero()
        }
    }

    pub fn set_gas_used(&self, ticket_id: &str, value: Uint128) {
        let mut buf = [0u8; 16];
        buf.copy_from_slice(&value.u128().to_le_bytes());
        self.store.set(&Self::key(ticket_id), &buf);
    }

    fn bytes_to_u128(bz: &[u8]) -> [u8; 16] {
        let mut out = [0u8; 16];
        let len = bz.len().min(16);
        out[..len].copy_from_slice(&bz[..len]);
        out
    }
}
