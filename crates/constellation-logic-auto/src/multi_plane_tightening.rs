// crates/constellation-logic-auto/src/multi_plane_tightening.rs
fn coordinate_multi_plane_tightening(
    breaches: &HashMap<CoordId, f32>,  // {PFAS: 1.8 (gold=1.0), HLR: 0.95 (gold=0.5)}
    corridors: &CorridorSet,
    weights: &CoordWeights,
    seasonal_history: &[TighteningEvent]
) -> Result<MultiPlaneTighteningProposal, TighteningError> {
    // Damping check: require ≥2 seasonal cycles since last tightening
    let last_tightening = seasonal_history.iter()
        .filter(|e| e.coords.iter().any(|c| breaches.contains_key(c)))
        .max_by_key(|e| e.timestamp)?;
    
    let seasons_elapsed = (Utc::now() - last_tightening.timestamp).num_days() / 90;
    if seasons_elapsed < 2 {
        return Err(TighteningError::DampingRequired {
            required_seasons: 2,
            elapsed_seasons: seasons_elapsed
        });
    }
    
    // Solve constrained optimization
    let mut optimizer = ConvexOptimizer::new();
    
    for (coord_id, breach_ratio) in breaches {
        let weight = weights.get(coord_id);
        let current_hard = corridors.get_hard(coord_id);
        
        // Tightening variable δ_j
        let delta = optimizer.add_variable(0.05, 0.15);
        
        // Objective: minimize weighted normalized breach
        let normalized_breach = (breach_ratio - corridors.get_gold(coord_id)) / 
                                (corridors.get_hard(coord_id) - corridors.get_gold(coord_id));
        optimizer.add_objective_term(weight * normalized_breach * delta);
        
        // Constraint: Lyapunov monotonicity
        optimizer.add_constraint(
            lyapunov_decrease_constraint(coord_id, delta, corridors)
        );
    }
    
    // Constraint: maintain operational threshold
    optimizer.add_constraint(
        operational_score(&corridors.apply_tightening(&optimizer.variables())) >= 0.75
    );
    
    let solution = optimizer.solve()?;
    
    Ok(MultiPlaneTighteningProposal {
        deltas: solution.deltas,
        lyapunov_proof: solution.lyapunov_certificate,
        simulation_results: run_monte_carlo_validation(&solution, 1000),
        seasonal_lock_until: Utc::now() + Duration::days(180)
    })
}
