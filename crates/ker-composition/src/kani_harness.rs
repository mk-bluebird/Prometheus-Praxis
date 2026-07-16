#![cfg(kani)]

use kani::any;
use crate::{RustKerParticle, ker_oplus_geom_min_max_rust};

fn bounded_unit(v: f32) -> f32 {
    let mut x: f32 = v;
    if x < 0.0 {
        x = 0.0;
    }
    if x > 1.0 {
        x = 1.0;
    }
    x
}

fn mk_particle(id_seed: u32, k: f32, e: f32, r: f32) -> RustKerParticle {
    let pid = format!("p{}", id_seed);
    RustKerParticle {
        particle_id: pid,
        topic_id: String::from("topic"),
        lane: String::from("RESEARCH"),
        k,
        e,
        r,
        evidencehex: String::from("00"),
        signinghex: String::from("00"),
    }
}

/// Proof: ker_oplus_geom_min_max_rust is commutative in K, E, R, and
/// canonical member ordering makes combined_id independent of operand order.
#[kani::proof]
fn proof_commutativity() {
    let k1: f32 = bounded_unit(any());
    let e1: f32 = bounded_unit(any());
    let r1: f32 = bounded_unit(any());

    let k2: f32 = bounded_unit(any());
    let e2: f32 = bounded_unit(any());
    let r2: f32 = bounded_unit(any());

    let p1 = mk_particle(1, k1, e1, r1);
    let p2 = mk_particle(2, k2, e2, r2);

    let comp12 = ker_oplus_geom_min_max_rust(&p1, &p2).unwrap();
    let comp21 = ker_oplus_geom_min_max_rust(&p2, &p1).unwrap();

    // Geometric mean, min, max should be symmetric.
    assert!(comp12.k_combined.to_bits() == comp21.k_combined.to_bits());
    assert!(comp12.e_combined.to_bits() == comp21.e_combined.to_bits());
    assert!(comp12.r_combined.to_bits() == comp21.r_combined.to_bits());

    // Canonical combined_id and members should be identical.
    assert!(comp12.combined_id == comp21.combined_id);
    assert!(comp12.members == comp21.members);
    assert!(comp12.rule_id == comp21.rule_id);
}

/// Proof: binary associativity of ker_oplus_geom_min_max_rust for K,E,R
/// over three particles in corridor [0,1].
#[kani::proof]
fn proof_associativity_three_particles() {
    let k1: f32 = bounded_unit(any());
    let e1: f32 = bounded_unit(any());
    let r1: f32 = bounded_unit(any());

    let k2: f32 = bounded_unit(any());
    let e2: f32 = bounded_unit(any());
    let r2: f32 = bounded_unit(any());

    let k3: f32 = bounded_unit(any());
    let e3: f32 = bounded_unit(any());
    let r3: f32 = bounded_unit(any());

    let p1 = mk_particle(1, k1, e1, r1);
    let p2 = mk_particle(2, k2, e2, r2);
    let p3 = mk_particle(3, k3, e3, r3);

    let c12 = ker_oplus_geom_min_max_rust(&p1, &p2).unwrap();
    let left = ker_oplus_geom_min_max_rust(
        &RustKerParticle {
            particle_id: c12.combined_id.clone(),
            topic_id: p1.topic_id.clone(),
            lane: p1.lane.clone(),
            k: c12.k_combined,
            e: c12.e_combined,
            r: c12.r_combined,
            evidencehex: String::from("00"),
            signinghex: String::from("00"),
        },
        &p3,
    ).unwrap();

    let c23 = ker_oplus_geom_min_max_rust(&p2, &p3).unwrap();
    let right = ker_oplus_geom_min_max_rust(
        &p1,
        &RustKerParticle {
            particle_id: c23.combined_id.clone(),
            topic_id: p2.topic_id.clone(),
            lane: p2.lane.clone(),
            k: c23.k_combined,
            e: c23.e_combined,
            r: c23.r_combined,
            evidencehex: String::from("00"),
            signinghex: String::from("00"),
        },
    ).unwrap();

    // For the algebra:
    // K: sqrt(K1*K2*K3) is unique irrespective of grouping.
    assert!(left.k_combined.to_bits() == right.k_combined.to_bits());

    // E: min(min(E1,E2),E3) == min(E1,min(E2,E3)).
    assert!(left.e_combined.to_bits() == right.e_combined.to_bits());

    // R: max(max(R1,R2),R3) == max(R1,max(R2,R3)).
    assert!(left.r_combined.to_bits() == right.r_combined.to_bits());
}
