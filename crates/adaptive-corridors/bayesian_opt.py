#!/usr/bin/env python3
"""
Bayesian Optimization for Adaptive Corridor Learning

Uses Gaussian Process-based optimization to find optimal corridor configurations
that maximize energy efficiency (E) while keeping risk (R) ≤ 0.13.

Repository: mk-bluebird/eco_restoration_shard
Path: crates/adaptive-corridors/bayesian_opt.py
Bostrom anchor: 0xECO_2026_RESTORATION_SHARD_bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
"""

import numpy as np
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass, field
from skopt import gp_minimize
from skopt.space import Real
from skopt.utils import use_named_args
import json
from pathlib import Path
from datetime import datetime


@dataclass
class CorridorConfig:
    coord_name: str
    safe: float
    gold: float
    hard: float
    
    def validate(self) -> bool:
        return self.safe < self.gold < self.hard
    
    def to_dict(self) -> Dict[str, float]:
        return {
            'safe': float(self.safe),
            'gold': float(self.gold),
            'hard': float(self.hard)
        }


@dataclass
class MissionOutcome:
    mission_id: str
    energy_efficiency: float
    risk_score: float
    corridor_config: Dict[str, CorridorConfig]
    timestamp: int
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'MissionOutcome':
        corridors = {
            name: CorridorConfig(name, cfg['safe'], cfg['gold'], cfg['hard'])
            for name, cfg in data.get('corridor_config', {}).items()
        }
        return cls(
            mission_id=data['mission_id'],
            energy_efficiency=data['energy_efficiency'],
            risk_score=data['risk_score'],
            corridor_config=corridors,
            timestamp=data['timestamp']
        )


@dataclass
class OptimizationResult:
    optimal_corridors: Dict[str, CorridorConfig]
    final_score: float
    avg_energy_efficiency: float
    max_risk_score: float
    n_iterations: int
    convergence_history: List[float] = field(default_factory=list)


class BayesianCorridorOptimizer:
    MAX_RISK = 0.13
    PENALTY_MULTIPLIER = 1000.0
    DEFAULT_COORDINATES = ['PFAS', 'CEC', 'HLR', 'T90', 'Energy']
    
    def __init__(self, coordinate_names: Optional[List[str]] = None):
        self.coordinate_names = coordinate_names or self.DEFAULT_COORDINATES
        self.search_space = self._build_search_space()
        self.historical_outcomes: List[MissionOutcome] = []
        self.convergence_history: List[float] = []
        
    def _build_search_space(self) -> List[Real]:
        space = []
        
        coordinate_ranges = {
            'PFAS': [(0.010, 0.030), (0.030, 0.060), (0.060, 0.100)],
            'CEC': [(5.0, 15.0), (15.0, 60.0), (60.0, 120.0)],
            'HLR': [(0.05, 0.20), (0.20, 0.70), (0.70, 1.50)],
            'T90': [(0.3, 0.7), (0.7, 2.5), (2.5, 6.0)],
            'Turbidity': [(1.0, 5.0), (5.0, 15.0), (15.0, 30.0)],
            'Phosphorus': [(0.05, 0.15), (0.15, 0.40), (0.40, 0.80)],
            'Nitrogen': [(2.0, 5.0), (5.0, 15.0), (15.0, 30.0)],
            'Energy': [(300.0, 700.0), (700.0, 2500.0), (2500.0, 6000.0)],
        }
        
        default_ranges = [(0.1, 0.5), (0.5, 1.0), (1.0, 2.0)]
        
        for coord in self.coordinate_names:
            ranges = coordinate_ranges.get(coord, default_ranges)
            space.extend([
                Real(ranges[0][0], ranges[0][1], name=f'{coord}_safe'),
                Real(ranges[1][0], ranges[1][1], name=f'{coord}_gold'),
                Real(ranges[2][0], ranges[2][1], name=f'{coord}_hard'),
            ])
        
        return space
    
    def load_historical_outcomes(self, outcomes: List[MissionOutcome]):
        self.historical_outcomes = outcomes
    
    def load_historical_from_json(self, filepath: str):
        with open(filepath, 'r') as f:
            data = json.load(f)
        
        outcomes = [MissionOutcome.from_dict(item) for item in data.get('outcomes', [])]
        self.load_historical_outcomes(outcomes)
    
    def _params_to_corridors(self, params: List[float]) -> Dict[str, CorridorConfig]:
        corridors = {}
        
        for i, coord in enumerate(self.coordinate_names):
            safe = params[i * 3]
            gold = params[i * 3 + 1]
            hard = params[i * 3 + 2]
            
            corridors[coord] = CorridorConfig(
                coord_name=coord,
                safe=safe,
                gold=gold,
                hard=hard
            )
        
        return corridors
    
    def simulate_missions(
        self,
        corridors: Dict[str, CorridorConfig],
        num_simulations: int = 100
    ) -> List[Dict[str, float]]:
        if self.historical_outcomes:
            return self._simulate_from_historical(corridors)
        else:
            return self._simulate_synthetic(corridors, num_simulations)
    
    def _simulate_from_historical(
        self,
        corridors: Dict[str, CorridorConfig]
    ) -> List[Dict[str, float]]:
        outcomes = []
        
        for historical in self.historical_outcomes:
            normalized_risk = 0.0
            valid_coords = 0
            
            for coord_name, corridor in corridors.items():
                if coord_name not in historical.corridor_config:
                    continue
                
                hist_corridor = historical.corridor_config[coord_name]
                value = (hist_corridor.safe + hist_corridor.hard) / 2
                
                if value > corridor.gold:
                    risk_contrib = (value - corridor.gold) / (corridor.hard - corridor.gold)
                    normalized_risk += min(risk_contrib, 1.0)
                
                valid_coords += 1
            
            avg_risk = normalized_risk / max(valid_coords, 1)
            
            tightness_penalty = sum(
                1.0 - (c.hard - c.safe) / (c.hard + c.safe)
                for c in corridors.values()
            ) / len(corridors)
            
            energy_eff = historical.energy_efficiency * (1.0 - 0.2 * tightness_penalty)
            
            outcomes.append({
                'risk_score': min(avg_risk, 1.0),
                'energy_efficiency': max(energy_eff, 0.0)
            })
        
        return outcomes
    
    def _simulate_synthetic(
        self,
        corridors: Dict[str, CorridorConfig],
        num_simulations: int
    ) -> List[Dict[str, float]]:
        outcomes = []
        
        for _ in range(num_simulations):
            normalized_risk = 0.0
            energy_efficiency = 1.0
            
            for coord_name, corridor in corridors.items():
                value = np.random.uniform(corridor.safe, corridor.hard)
                
                if value > corridor.gold:
                    normalized_risk += (value - corridor.gold) / (corridor.hard - corridor.gold)
                
                tightness = corridor.hard / max(corridor.gold * 2, 0.001)
                energy_efficiency *= (1.0 - 0.1 * min(tightness, 1.0))
            
            avg_risk = normalized_risk / len(corridors)
            
            outcomes.append({
                'risk_score': min(avg_risk, 1.0),
                'energy_efficiency': max(energy_efficiency, 0.0)
            })
        
        return outcomes
    
    def objective_function(self, params: List[float]) -> float:
        corridors = self._params_to_corridors(params)
        
        for corridor in corridors.values():
            if not corridor.validate():
                return self.PENALTY_MULTIPLIER
        
        num_sims = 100 if self.historical_outcomes else 50
        outcomes = self.simulate_missions(corridors, num_simulations=num_sims)
        
        if not outcomes:
            return self.PENALTY_MULTIPLIER
        
        avg_E = np.mean([o['energy_efficiency'] for o in outcomes])
        max_R = np.max([o['risk_score'] for o in outcomes])
        mean_R = np.mean([o['risk_score'] for o in outcomes])
        
        penalty = 0.0
        if max_R > self.MAX_RISK:
            penalty += self.PENALTY_MULTIPLIER * (max_R - self.MAX_RISK)
        
        if mean_R > self.MAX_RISK * 0.8:
            penalty += 100.0 * (mean_R - self.MAX_RISK * 0.8)
        
        score = -avg_E + penalty
        self.convergence_history.append(score)
        
        return score
    
    def optimize(
        self,
        n_calls: int = 100,
        random_state: int = 42,
        verbose: bool = True
    ) -> OptimizationResult:
        self.convergence_history = []
        
        result = gp_minimize(
            func=self.objective_function,
            dimensions=self.search_space,
            n_calls=n_calls,
            random_state=random_state,
            verbose=verbose,
            n_initial_points=max(20, n_calls // 5),
            acq_func='EI'
        )
        
        optimal_corridors = self._params_to_corridors(result.x)
        
        final_outcomes = self.simulate_missions(optimal_corridors, num_simulations=200)
        avg_E = np.mean([o['energy_efficiency'] for o in final_outcomes])
        max_R = np.max([o['risk_score'] for o in final_outcomes])
        
        return OptimizationResult(
            optimal_corridors=optimal_corridors,
            final_score=result.fun,
            avg_energy_efficiency=avg_E,
            max_risk_score=max_R,
            n_iterations=n_calls,
            convergence_history=self.convergence_history
        )
    
    def export_to_json(self, corridors: Dict[str, CorridorConfig], filepath: str):
        output = {
            'bostrom_anchor': '0xECO_2026_RESTORATION_SHARD_bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
            'optimization_timestamp': int(datetime.now().timestamp()),
            'max_risk_constraint': self.MAX_RISK,
            'coordinate_names': self.coordinate_names,
            'corridors': {
                coord_name: corridor.to_dict()
                for coord_name, corridor in corridors.items()
            }
        }
        
        Path(filepath).parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'w') as f:
            json.dump(output, f, indent=2)
    
    def export_optimization_result(self, result: OptimizationResult, filepath: str):
        output = {
            'bostrom_anchor': '0xECO_2026_RESTORATION_SHARD_bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
            'optimization_timestamp': int(datetime.now().timestamp()),
            'max_risk_constraint': self.MAX_RISK,
            'coordinate_names': self.coordinate_names,
            'performance': {
                'avg_energy_efficiency': float(result.avg_energy_efficiency),
                'max_risk_score': float(result.max_risk_score),
                'final_objective_score': float(result.final_score),
                'n_iterations': result.n_iterations,
                'roh_compliant': result.max_risk_score <= 0.30
            },
            'corridors': {
                coord_name: corridor.to_dict()
                for coord_name, corridor in result.optimal_corridors.items()
            },
            'convergence_history': [float(x) for x in result.convergence_history]
        }
        
        Path(filepath).parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'w') as f:
            json.dump(output, f, indent=2)


def main():
    coordinates = ['PFAS', 'CEC', 'HLR', 'T90', 'Energy']
    
    optimizer = BayesianCorridorOptimizer(coordinates)
    
    print("Starting Bayesian corridor optimization...")
    print(f"Coordinates: {coordinates}")
    print(f"Max risk constraint: R ≤ {optimizer.MAX_RISK}")
    print(f"RoH ceiling: ≤ 0.30")
    
    result = optimizer.optimize(n_calls=100, random_state=42)
    
    print("\n=== Optimization Results ===")
    print(f"Final objective score: {result.final_score:.6f}")
    print(f"Average energy efficiency: {result.avg_energy_efficiency:.4f}")
    print(f"Maximum risk score: {result.max_risk_score:.4f}")
    print(f"RoH compliant: {result.max_risk_score <= 0.30}")
    
    print("\n=== Optimal Corridors ===")
    for coord_name, corridor in result.optimal_corridors.items():
        print(f"\n{coord_name}:")
        print(f"  Safe:  {corridor.safe:.4f}")
        print(f"  Gold:  {corridor.gold:.4f}")
        print(f"  Hard:  {corridor.hard:.4f}")
    
    output_dir = Path('output')
    output_dir.mkdir(exist_ok=True)
    
    optimizer.export_optimization_result(
        result,
        'output/optimal_corridors.json'
    )
    
    print("\n✓ Exported to output/optimal_corridors.json")


if __name__ == '__main__':
    main()
