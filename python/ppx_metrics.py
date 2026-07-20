# filename: python/ppx_metrics.py
from __future__ import annotations
import time
from dataclasses import dataclass, field
from typing import Dict

@dataclass
class Counter:
    total: float = 0.0

    def inc(self, value: float = 1.0) -> None:
        self.total += value

@dataclass
class Timer:
    start_time: float = field(default_factory=time.time)
    elapsed: float = 0.0

    def stop(self) -> None:
        self.elapsed = time.time() - self.start_time

@dataclass
class MetricsRegistry:
    counters: Dict[str, Counter] = field(default_factory=dict)
    timers: Dict[str, Timer] = field(default_factory=dict)

    def counter(self, name: str) -> Counter:
        if name not in self.counters:
            self.counters[name] = Counter()
        return self.counters[name]

    def timer(self, name: str) -> Timer:
        if name not in self.timers:
            self.timers[name] = Timer()
        return self.timers[name]
