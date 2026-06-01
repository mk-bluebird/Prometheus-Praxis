//! Automated Cycle Breaking for Dependency DAG Enforcement
//!
//! Analyzes circular dependencies in Cargo workspaces and proposes
//! minimal-impact refactoring strategies to restore DAG property.

#![forbid(unsafe_code)]
#![deny(warnings)]

use std::collections::{HashMap, HashSet, VecDeque};
use serde::{Deserialize, Serialize};
use petgraph::graph::{DiGraph, NodeIndex};
use petgraph::algo::tarjan_scc;
use petgraph::Direction;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum DependencyWeight {
    Critical = 4,
    High = 3,
    Medium = 2,
    Low = 1,
}

impl DependencyWeight {
    pub fn from_usage_analysis(import_count: usize, is_public_api: bool) -> Self {
        match (import_count, is_public_api) {
            (n, true) if n > 10 => Self::Critical,
            (n, true) if n > 5 => Self::High,
            (n, false) if n > 10 => Self::High,
            (n, _) if n > 3 => Self::Medium,
            _ => Self::Low,
        }
    }

    pub fn as_score(&self) -> u32 {
        *self as u32
    }

    pub fn effort_hours(&self) -> u32 {
        match self {
            Self::Low => 2,
            Self::Medium => 4,
            Self::High => 8,
            Self::Critical => 16,
        }
    }

    pub fn is_automatable(&self) -> bool {
        matches!(self, Self::Low | Self::Medium)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ArtifactId {
    pub name: String,
    pub version: String,
}

impl ArtifactId {
    pub fn new(name: impl Into<String>, version: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            version: version.into(),
        }
    }

    pub fn matches(&self, other: &Self) -> bool {
        self.name == other.name && self.version == other.version
    }
}

impl std::fmt::Display for ArtifactId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}@{}", self.name, self.version)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DependencyEdge {
    pub source: ArtifactId,
    pub target: ArtifactId,
    pub weight: DependencyWeight,
    pub import_count: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RefactoringStrategy {
    InlineSmallDependency {
        estimated_lines: usize,
    },
    ExtractSharedInterface {
        suggested_trait_name: String,
    },
    BreakIntoDualCrates {
        suggested_names: (String, String),
    },
    MoveToCommonParent {
        parent_crate_name: String,
    },
}

impl RefactoringStrategy {
    fn from_weight(weight: DependencyWeight, source: &ArtifactId, target: &ArtifactId) -> Self {
        match weight {
            DependencyWeight::Low | DependencyWeight::Medium => {
                Self::InlineSmallDependency {
                    estimated_lines: if weight == DependencyWeight::Low { 30 } else { 50 },
                }
            }
            DependencyWeight::High => {
                Self::ExtractSharedInterface {
                    suggested_trait_name: format!("{}Interface", 
                        target.name.trim_start_matches("crate-").replace("-", "_")),
                }
            }
            DependencyWeight::Critical => {
                Self::BreakIntoDualCrates {
                    suggested_names: (
                        format!("{}-core", source.name),
                        format!("{}-impl", source.name),
                    ),
                }
            }
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RefactoringProposal {
    pub edge_to_remove: (ArtifactId, ArtifactId),
    pub strategy: RefactoringStrategy,
    pub estimated_effort_hours: u32,
    pub automated_pr_available: bool,
    pub impact_score: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CycleAnalysis {
    pub cycles: Vec<Vec<ArtifactId>>,
    pub proposals: Vec<RefactoringProposal>,
    pub total_critical_edges: usize,
}

pub struct DependencyGraph {
    graph: DiGraph<ArtifactId, DependencyWeight>,
    artifact_to_node: HashMap<ArtifactId, NodeIndex>,
}

impl DependencyGraph {
    pub fn new() -> Self {
        Self {
            graph: DiGraph::new(),
            artifact_to_node: HashMap::new(),
        }
    }

    pub fn add_artifact(&mut self, artifact: ArtifactId) -> NodeIndex {
        if let Some(&idx) = self.artifact_to_node.get(&artifact) {
            idx
        } else {
            let idx = self.graph.add_node(artifact.clone());
            self.artifact_to_node.insert(artifact, idx);
            idx
        }
    }

    pub fn add_dependency(
        &mut self,
        source: &ArtifactId,
        target: &ArtifactId,
        weight: DependencyWeight,
    ) {
        let source_idx = self.add_artifact(source.clone());
        let target_idx = self.add_artifact(target.clone());
        self.graph.add_edge(source_idx, target_idx, weight);
    }

    pub fn get_edge_weight(&self, source: &ArtifactId, target: &ArtifactId) -> Option<DependencyWeight> {
        let source_idx = self.artifact_to_node.get(source)?;
        let target_idx = self.artifact_to_node.get(target)?;
        
        self.graph
            .find_edge(*source_idx, *target_idx)
            .and_then(|edge| self.graph.edge_weight(edge).copied())
    }

    pub fn compute_blast_radius(&self, artifact: &ArtifactId) -> usize {
        let node_idx = match self.artifact_to_node.get(artifact) {
            Some(&idx) => idx,
            None => return 0,
        };

        let mut visited = HashSet::new();
        let mut queue = VecDeque::new();
        queue.push_back(node_idx);
        visited.insert(node_idx);

        while let Some(current) = queue.pop_front() {
            for neighbor in self.graph.neighbors_directed(current, Direction::Incoming) {
                if visited.insert(neighbor) {
                    queue.push_back(neighbor);
                }
            }
        }

        visited.len().saturating_sub(1)
    }

    pub fn find_cycles(&self) -> Vec<Vec<ArtifactId>> {
        let sccs = tarjan_scc(&self.graph);
        
        sccs.into_iter()
            .filter(|scc| scc.len() > 1)
            .map(|scc| {
                scc.into_iter()
                    .map(|idx| self.graph[idx].clone())
                    .collect()
            })
            .collect()
    }

    pub fn compute_refactoring_impact(
        &self,
        source: &ArtifactId,
        target: &ArtifactId,
    ) -> u32 {
        let source_br = self.compute_blast_radius(source) as u32;
        let target_br = self.compute_blast_radius(target) as u32;
        let weight = self.get_edge_weight(source, target)
            .map(|w| w.as_score())
            .unwrap_or(1);
        
        weight.saturating_mul(10).saturating_add(source_br).saturating_add(target_br)
    }

    pub fn artifact_count(&self) -> usize {
        self.artifact_to_node.len()
    }

    pub fn edge_count(&self) -> usize {
        self.graph.edge_count()
    }
}

impl Default for DependencyGraph {
    fn default() -> Self {
        Self::new()
    }
}

pub fn break_weakest_cycle_edge(
    cycle: &[ArtifactId],
    graph: &DependencyGraph,
) -> RefactoringProposal {
    if cycle.is_empty() {
        panic!("Cannot break edge in empty cycle");
    }

    let edges_in_cycle: Vec<_> = cycle
        .windows(2)
        .map(|pair| (pair[0].clone(), pair[1].clone()))
        .chain(std::iter::once((
            cycle[cycle.len() - 1].clone(),
            cycle[0].clone(),
        )))
        .collect();

    let (source, target, weight, impact) = edges_in_cycle
        .iter()
        .map(|(s, t)| {
            let weight = graph.get_edge_weight(s, t).unwrap_or(DependencyWeight::Low);
            let impact = graph.compute_refactoring_impact(s, t);
            (s.clone(), t.clone(), weight, impact)
        })
        .min_by_key(|(_, _, w, impact)| {
            w.as_score().saturating_mul(10).saturating_add(*impact)
        })
        .expect("Cycle must have at least one edge");

    let strategy = RefactoringStrategy::from_weight(weight, &source, &target);
    let estimated_effort_hours = weight.effort_hours();
    let automated_pr_available = weight.is_automatable();

    RefactoringProposal {
        edge_to_remove: (source, target),
        strategy,
        estimated_effort_hours,
        automated_pr_available,
        impact_score: impact,
    }
}

pub fn analyze_cycles(graph: &DependencyGraph) -> CycleAnalysis {
    let cycles = graph.find_cycles();
    
    let proposals: Vec<RefactoringProposal> = cycles
        .iter()
        .map(|cycle| break_weakest_cycle_edge(cycle, graph))
        .collect();

    let total_critical_edges = cycles
        .iter()
        .flat_map(|cycle| {
            cycle.windows(2)
                .chain(std::iter::once([&cycle[cycle.len() - 1], &cycle[0]].as_slice()))
                .filter_map(|pair| {
                    graph.get_edge_weight(&pair[0], &pair[1])
                })
        })
        .filter(|w| matches!(w, DependencyWeight::Critical))
        .count();

    CycleAnalysis {
        cycles,
        proposals,
        total_critical_edges,
    }
}

pub fn generate_ci_report(analysis: &CycleAnalysis) -> String {
    use std::fmt::Write;
    
    let mut report = String::new();
    
    writeln!(report, "# Dependency DAG Analysis Report").unwrap();
    writeln!(report).unwrap();
    
    if analysis.cycles.is_empty() {
        writeln!(report, "✅ No circular dependencies detected. DAG property enforced.").unwrap();
        return report;
    }
    
    writeln!(report, "❌ Found {} circular dependencies", analysis.cycles.len()).unwrap();
    writeln!(report, "⚠️  {} critical edges involved", analysis.total_critical_edges).unwrap();
    writeln!(report).unwrap();
    
    for (idx, cycle) in analysis.cycles.iter().enumerate() {
        writeln!(report, "## Cycle {}", idx + 1).unwrap();
        writeln!(report).unwrap();
        writeln!(report, "Path:").unwrap();
        for artifact in cycle {
            writeln!(report, "  → {}", artifact).unwrap();
        }
        writeln!(report, "  → {} (completes cycle)", cycle[0]).unwrap();
        writeln!(report).unwrap();
    }
    
    writeln!(report, "## Refactoring Proposals").unwrap();
    writeln!(report).unwrap();
    
    for (idx, proposal) in analysis.proposals.iter().enumerate() {
        writeln!(report, "### Proposal {}", idx + 1).unwrap();
        writeln!(report).unwrap();
        writeln!(
            report,
            "**Edge to remove:** {} → {}",
            proposal.edge_to_remove.0,
            proposal.edge_to_remove.1
        ).unwrap();
        writeln!(report, "**Estimated effort:** {} hours", proposal.estimated_effort_hours).unwrap();
        writeln!(report, "**Automated PR available:** {}", proposal.automated_pr_available).unwrap();
        writeln!(report, "**Impact score:** {}", proposal.impact_score).unwrap();
        writeln!(report).unwrap();
        
        match &proposal.strategy {
            RefactoringStrategy::InlineSmallDependency { estimated_lines } => {
                writeln!(report, "**Strategy:** Inline small dependency (~{} lines)", estimated_lines).unwrap();
            }
            RefactoringStrategy::ExtractSharedInterface { suggested_trait_name } => {
                writeln!(report, "**Strategy:** Extract shared trait `{}`", suggested_trait_name).unwrap();
            }
            RefactoringStrategy::BreakIntoDualCrates { suggested_names } => {
                writeln!(
                    report,
                    "**Strategy:** Split into `{}` and `{}`",
                    suggested_names.0,
                    suggested_names.1
                ).unwrap();
            }
            RefactoringStrategy::MoveToCommonParent { parent_crate_name } => {
                writeln!(report, "**Strategy:** Move to common parent `{}`", parent_crate_name).unwrap();
            }
        }
        writeln!(report).unwrap();
    }
    
    report
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cycle_detection() {
        let mut graph = DependencyGraph::new();
        
        let a = ArtifactId::new("crate-a", "0.1.0");
        let b = ArtifactId::new("crate-b", "0.1.0");
        let c = ArtifactId::new("crate-c", "0.1.0");
        
        graph.add_dependency(&a, &b, DependencyWeight::Medium);
        graph.add_dependency(&b, &c, DependencyWeight::Low);
        graph.add_dependency(&c, &a, DependencyWeight::Low);
        
        let cycles = graph.find_cycles();
        assert_eq!(cycles.len(), 1);
        assert_eq!(cycles[0].len(), 3);
    }

    #[test]
    fn test_no_cycles() {
        let mut graph = DependencyGraph::new();
        
        let a = ArtifactId::new("crate-a", "0.1.0");
        let b = ArtifactId::new("crate-b", "0.1.0");
        let c = ArtifactId::new("crate-c", "0.1.0");
        
        graph.add_dependency(&a, &b, DependencyWeight::Medium);
        graph.add_dependency(&b, &c, DependencyWeight::Low);
        
        let cycles = graph.find_cycles();
        assert_eq!(cycles.len(), 0);
    }

    #[test]
    fn test_blast_radius() {
        let mut graph = DependencyGraph::new();
        
        let a = ArtifactId::new("crate-a", "0.1.0");
        let b = ArtifactId::new("crate-b", "0.1.0");
        let c = ArtifactId::new("crate-c", "0.1.0");
        
        graph.add_dependency(&a, &c, DependencyWeight::Medium);
        graph.add_dependency(&b, &c, DependencyWeight::Low);
        
        let blast_radius = graph.compute_blast_radius(&c);
        assert_eq!(blast_radius, 2);
    }

    #[test]
    fn test_weakest_edge_selection() {
        let mut graph = DependencyGraph::new();
        
        let a = ArtifactId::new("crate-a", "0.1.0");
        let b = ArtifactId::new("crate-b", "0.1.0");
        let c = ArtifactId::new("crate-c", "0.1.0");
        
        graph.add_dependency(&a, &b, DependencyWeight::Critical);
        graph.add_dependency(&b, &c, DependencyWeight::Low);
        graph.add_dependency(&c, &a, DependencyWeight::Medium);
        
        let cycle = vec![a.clone(), b.clone(), c.clone()];
        let proposal = break_weakest_cycle_edge(&cycle, &graph);
        
        assert_eq!(proposal.edge_to_remove.0.name, "crate-b");
        assert_eq!(proposal.edge_to_remove.1.name, "crate-c");
        assert!(proposal.automated_pr_available);
    }

    #[test]
    fn test_analyze_cycles_complete() {
        let mut graph = DependencyGraph::new();
        
        let a = ArtifactId::new("eco-safety", "0.1.0");
        let b = ArtifactId::new("corridor-logic", "0.1.0");
        let c = ArtifactId::new("ker-metrics", "0.1.0");
        
        graph.add_dependency(&a, &b, DependencyWeight::High);
        graph.add_dependency(&b, &c, DependencyWeight::Medium);
        graph.add_dependency(&c, &a, DependencyWeight::Low);
        
        let analysis = analyze_cycles(&graph);
        
        assert_eq!(analysis.cycles.len(), 1);
        assert_eq!(analysis.proposals.len(), 1);
        assert_eq!(analysis.total_critical_edges, 0);
        
        let proposal = &analysis.proposals[0];
        assert_eq!(proposal.edge_to_remove.0.name, "ker-metrics");
        assert_eq!(proposal.edge_to_remove.1.name, "eco-safety");
    }

    #[test]
    fn test_dependency_weight_scoring() {
        assert_eq!(DependencyWeight::Low.as_score(), 1);
        assert_eq!(DependencyWeight::Medium.as_score(), 2);
        assert_eq!(DependencyWeight::High.as_score(), 3);
        assert_eq!(DependencyWeight::Critical.as_score(), 4);
    }

    #[test]
    fn test_refactoring_strategy_generation() {
        let source = ArtifactId::new("source-crate", "1.0.0");
        let target = ArtifactId::new("target-crate", "1.0.0");
        
        let low_strategy = RefactoringStrategy::from_weight(
            DependencyWeight::Low, 
            &source, 
            &target
        );
        assert!(matches!(low_strategy, RefactoringStrategy::InlineSmallDependency { .. }));
        
        let critical_strategy = RefactoringStrategy::from_weight(
            DependencyWeight::Critical,
            &source,
            &target
        );
        assert!(matches!(critical_strategy, RefactoringStrategy::BreakIntoDualCrates { .. }));
    }
}
