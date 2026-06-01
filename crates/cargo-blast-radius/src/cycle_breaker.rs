// crates/cargo-blast-radius/src/cycle_breaker.rs
fn break_weakest_cycle_edge(cycle: &[ArtifactId], graph: &DependencyGraph) -> RefactoringProposal {
    let edges_in_cycle: Vec<_> = cycle.windows(2)
        .map(|pair| (pair[0], pair[1]))
        .chain(std::iter::once((cycle[cycle.len()-1], cycle[0])))
        .collect();
    
    // Find edge with minimum weight and impact
    let (source, target, weight) = edges_in_cycle.iter()
        .map(|(s, t)| {
            let edge_weight = graph.get_edge_weight(s, t);
            let impact_score = compute_refactoring_impact(s, t, graph);
            (s, t, edge_weight, impact_score)
        })
        .min_by_key(|(_, _, w, impact)| w * 10 + impact)
        .map(|(s, t, w, _)| (s, t, w))
        .unwrap();
    
    RefactoringProposal {
        edge_to_remove: (source.clone(), target.clone()),
        strategy: if weight <= 2 {
            RefactoringStrategy::InlineSmallDependency
        } else {
            RefactoringStrategy::ExtractSharedInterface
        },
        estimated_effort_hours: (weight * 4) as u32,
        automated_pr_available: weight <= 2
    }
}
