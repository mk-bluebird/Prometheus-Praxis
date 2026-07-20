// File: agents/prometheus_praxis/eco_restoration_workload_schedule_view.cpp
// Target repo: mk-bluebird/eco_restoration_shard
// Role: Companion utilities for eco_restoration_workload_mip.cpp
//       Builds per-node schedule views and performs lightweight SafeStep-style checks.
// License: MIT OR Apache-2.0

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <tuple>

#include "eco_restoration_workload_mip.hpp" // header exposing WorkloadTask, NodeCapacity, TimeSlot, WorkloadScheduleSolution

struct NodeDailyScheduleView {
    std::string node_id;
    double total_energy_J;
    double delta_Vt_node;
    std::vector<std::tuple<std::string, int>> tasks_slots; // (task_id, slot_index)
};

// Build per-node schedule views from the global solution.
std::map<std::string, NodeDailyScheduleView> build_node_schedule_views(
    const std::vector<WorkloadTask>& tasks,
    const WorkloadScheduleSolution& solution
) {
    std::map<std::string, NodeDailyScheduleView> views;

    // Index tasks by id for quick lookup.
    std::map<std::string, WorkloadTask> task_index;
    for (const auto& t : tasks) {
        task_index[t.id] = t;
    }

    for (const auto& [task_id, slot_idx] : solution.scheduled_task_slots) {
        auto it = task_index.find(task_id);
        if (it == task_index.end()) {
            continue; // Should not happen if inputs are consistent.
        }
        const WorkloadTask& task = it->second;
        auto& view = views[task.node_id];

        if (view.node_id.empty()) {
            view.node_id = task.node_id;
            view.total_energy_J = 0.0;
            view.delta_Vt_node = 0.0;
            view.tasks_slots.clear();
        }

        view.tasks_slots.emplace_back(task_id, slot_idx);
        view.total_energy_J += task.energy_req_J;
        view.delta_Vt_node += task.delta_Vt;
    }

    return views;
}

// Lightweight SafeStep-style check for a node schedule:
// - ΔVt_node <= 0
// - total_energy_J non-negative
// (Full SafeStepRule, K,E,R, non-offsettable planes are enforced by Rust governance guards.)
bool node_schedule_satisfies_basic_safestep(const NodeDailyScheduleView& view) {
    if (view.total_energy_J < 0.0) {
        return false;
    }
    if (view.delta_Vt_node > 0.0) {
        return false;
    }
    return true;
}

// Example usage: print per-node schedule view and basic checks.
void print_node_schedule_views(
    const std::vector<WorkloadTask>& tasks,
    const WorkloadScheduleSolution& solution
) {
    auto views = build_node_schedule_views(tasks, solution);

    for (const auto& [node_id, view] : views) {
        std::cout << "Node: " << node_id << "\n";
        std::cout << "  total_energy_J: " << view.total_energy_J << "\n";
        std::cout << "  delta_Vt_node: " << view.delta_Vt_node << " (<= 0 required)\n";
        std::cout << "  tasks:\n";
        for (const auto& [task_id, slot_idx] : view.tasks_slots) {
            std::cout << "    - " << task_id << " @ slot " << slot_idx << "\n";
        }
        bool ok = node_schedule_satisfies_basic_safestep(view);
        std::cout << "  basic_safe_step_ok: " << (ok ? "true" : "false") << "\n\n";
    }
}

// In a full Prometheus-Praxis integration:
//
// - This file's views would be serialized to ALNv2 schedule shards,
//   e.g., EcoNetDailyWorkloadSchedule.aln, with per-node fields:
//   node_id, total_energy_J, delta_Vt_node, tasks_slots, evidence_hex, signing_did.
// - Rust governance guards (e.g., LaneGuard / SafeStepRule) would be invoked via FFI
//   to enforce full K,E,R,Vt monotonicity and non-offsettable plane invariants.
// - The resulting shard would be registered in DefinitionRegistry and econet.agent.function.catalog,
//   making it discoverable and auditable by alnctl and AI tooling.
