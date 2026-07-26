// src/uint128.rs

#![forbid(unsafe_code)]

use cosmwasm_std::Uint128;

/// Re-export of CosmWasm's Uint128 for Prometheus-Praxis contracts.
///
/// This module provides a stable, project-local path that AI agents and
/// other crates can depend on without importing cosmwasm_std directly.
pub type EcoUint128 = Uint128;

/// Convenience constructors and helpers around Uint128.
pub fn eco_uint128_from_u128(value: u128) -> EcoUint128 {
    EcoUint128::from(value)
}

pub fn eco_uint128_zero() -> EcoUint128 {
    EcoUint128::zero()
}

pub fn eco_uint128_is_zero(value: EcoUint128) -> bool {
    value.is_zero()
}
