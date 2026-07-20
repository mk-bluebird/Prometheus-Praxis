# filename: python/ppx_diagnostics.py
# destination: github.com/mk-bluebird/Prometheus-Praxis/python/ppx_diagnostics.py
#
# Unified diagnostics schema for Prometheus-style metrics and structured file deltas.
# Minimal comments, production-ready wiring for Rust/CPP/Python and local agents.

from __future__ import annotations

from enum import Enum
from typing import Any, Dict, Iterable, List, Optional, Tuple
from datetime import datetime

from pydantic import BaseModel, Field, field_validator


class ChangeTypeEnum(str, Enum):
    CONTEXT = "context"
    ADDITION = "addition"
    DELETION = "deletion"


class TelemetryPayloadLabel(BaseModel):
    name: str = Field(
        ...,
        min_length=1,
        max_length=64,
        description="Target Prometheus label key.",
    )
    value: str = Field(
        ...,
        description="Target dimension value.",
    )


class StructuredMetricData(BaseModel):
    name: str = Field(
        ...,
        pattern=r"^[a-zA-Z_:][a-zA-Z0-9_:]*$",
        description="Prometheus-compliant metric key.",
    )
    labels: List[TelemetryPayloadLabel] = Field(
        default_factory=list,
        description="Optional label dimensions for the metric.",
    )
    recorded_value: float = Field(
        ...,
        description="The observed numerical metric value.",
    )
    collection_epoch: datetime = Field(
        default_factory=datetime.utcnow,
        description="UTC timestamp when the metric was collected.",
    )

    def to_prometheus_sample(self) -> str:
        label_str = ""
        if self.labels:
            parts = [f'{lbl.name}="{lbl.value}"' for lbl in self.labels]
            label_str = "{" + ",".join(parts) + "}"
        ts = int(self.collection_epoch.timestamp())
        return f"{self.name}{label_str} {self.recorded_value} {ts}"


class ParsedCodeLine(BaseModel):
    old_line_number: Optional[int] = Field(
        None,
        description="Previous line offset.",
    )
    new_line_number: Optional[int] = Field(
        None,
        description="Updated line offset.",
    )
    modification_type: ChangeTypeEnum
    line_payload: str = Field(
        ...,
        description="Raw text of the parsed line.",
    )

    @field_validator("old_line_number", "new_line_number")
    @classmethod
    def validate_line_offsets(cls, val: Optional[int]) -> Optional[int]:
        if val is not None and val < -1:
            raise ValueError("Line numbers must be non-negative or -1.")
        return val


class ParsedDiffHunk(BaseModel):
    old_start: int
    old_count: int
    new_start: int
    new_count: int
    lines: List[ParsedCodeLine] = Field(
        default_factory=list,
        description="Parsed lines belonging to this hunk.",
    )


class StructuredFileDelta(BaseModel):
    origin_filepath: str
    target_filepath: str
    is_new_file: bool = False
    hunks: List[ParsedDiffHunk] = Field(
        default_factory=list,
        description="List of parsed diff hunks.",
    )

    def total_added_lines(self) -> int:
        return sum(
            1
            for h in self.hunks
            for line in h.lines
            if line.modification_type == ChangeTypeEnum.ADDITION
        )

    def total_deleted_lines(self) -> int:
        return sum(
            1
            for h in self.hunks
            for line in h.lines
            if line.modification_type == ChangeTypeEnum.DELETION
        )

    def to_summary_dict(self) -> Dict[str, Any]:
        return {
            "origin_filepath": self.origin_filepath,
            "target_filepath": self.target_filepath,
            "is_new_file": self.is_new_file,
            "hunks": len(self.hunks),
            "added_lines": self.total_added_lines(),
            "deleted_lines": self.total_deleted_lines(),
        }


class UnifiedSystemDiagnosticState(BaseModel):
    collected_metrics: List[StructuredMetricData] = Field(
        default_factory=list,
        description="Collected metric samples.",
    )
    detected_deltas: List[StructuredFileDelta] = Field(
        default_factory=list,
        description="Detected file-level changes.",
    )
    diagnostic_timestamp: datetime = Field(
        default_factory=datetime.utcnow,
        description="UTC timestamp when the snapshot was assembled.",
    )

    def to_json_dict(self) -> Dict[str, Any]:
        return self.model_dump()

    def add_metric(
        self,
        name: str,
        value: float,
        labels: Optional[Dict[str, str]] = None,
        epoch: Optional[datetime] = None,
    ) -> None:
        label_models: List[TelemetryPayloadLabel] = []
        if labels:
            label_models = [
                TelemetryPayloadLabel(name=k, value=v) for k, v in labels.items()
            ]
        metric = StructuredMetricData(
            name=name,
            labels=label_models,
            recorded_value=value,
            collection_epoch=epoch or datetime.utcnow(),
        )
        self.collected_metrics.append(metric)

    def add_file_delta(self, delta: StructuredFileDelta) -> None:
        self.detected_deltas.append(delta)

    def summarize(self) -> str:
        metric_count = len(self.collected_metrics)
        file_count = len(self.detected_deltas)
        additions = sum(d.total_added_lines() for d in self.detected_deltas)
        deletions = sum(d.total_deleted_lines() for d in self.detected_deltas)
        return (
            f"Metrics: {metric_count} samples, "
            f"Deltas: {file_count} files ({additions} additions, {deletions} deletions)"
        )

    def metrics_as_prometheus_text(self) -> str:
        lines = [m.to_prometheus_sample() for m in self.collected_metrics]
        return "\n".join(lines)

    def deltas_summary(self) -> List[Dict[str, Any]]:
        return [d.to_summary_dict() for d in self.detected_deltas]


def _parse_hunk_header(header_line: str) -> Tuple[int, int, int, int]:
    header = header_line.strip("@ ")
    parts = header.split(" ")
    old_spec = parts[0]
    new_spec = parts[1]

    def parse_spec(spec: str) -> Tuple[int, int]:
        spec = spec.lstrip("+-")
        if "," in spec:
            s, c = spec.split(",", 1)
            return int(s), int(c)
        return int(spec), 1

    old_start, old_count = parse_spec(old_spec)
    new_start, new_count = parse_spec(new_spec)
    return old_start, old_count, new_start, new_count


def parse_unified_diff(
    origin_path: str,
    target_path: str,
    diff_lines: Iterable[str],
    is_new_file: bool = False,
) -> StructuredFileDelta:
    hunks: List[ParsedDiffHunk] = []
    current_hunk: Optional[ParsedDiffHunk] = None
    old_line = 0
    new_line = 0

    for raw in diff_lines:
        line = raw.rstrip("\n")

        if line.startswith("@@"):
            old_start, old_count, new_start, new_count = _parse_hunk_header(line)
            current_hunk = ParsedDiffHunk(
                old_start=old_start,
                old_count=old_count,
                new_start=new_start,
                new_count=new_count,
                lines=[],
            )
            hunks.append(current_hunk)
            old_line = old_start
            new_line = new_start
            continue

        if current_hunk is None:
            continue

        if not line:
            current_hunk.lines.append(
                ParsedCodeLine(
                    old_line_number=old_line,
                    new_line_number=new_line,
                    modification_type=ChangeTypeEnum.CONTEXT,
                    line_payload="",
                )
            )
            old_line += 1
            new_line += 1
            continue

        prefix = line[0]
        payload = line[1:] if len(line) > 1 else ""

        if prefix == " ":
            current_hunk.lines.append(
                ParsedCodeLine(
                    old_line_number=old_line,
                    new_line_number=new_line,
                    modification_type=ChangeTypeEnum.CONTEXT,
                    line_payload=payload,
                )
            )
            old_line += 1
            new_line += 1
        elif prefix == "+":
            current_hunk.lines.append(
                ParsedCodeLine(
                    old_line_number=-1,
                    new_line_number=new_line,
                    modification_type=ChangeTypeEnum.ADDITION,
                    line_payload=payload,
                )
            )
            new_line += 1
        elif prefix == "-":
            current_hunk.lines.append(
                ParsedCodeLine(
                    old_line_number=old_line,
                    new_line_number=-1,
                    modification_type=ChangeTypeEnum.DELETION,
                    line_payload=payload,
                )
            )
            old_line += 1
        else:
            current_hunk.lines.append(
                ParsedCodeLine(
                    old_line_number=old_line,
                    new_line_number=new_line,
                    modification_type=ChangeTypeEnum.CONTEXT,
                    line_payload=line,
                )
            )
            old_line += 1
            new_line += 1

    return StructuredFileDelta(
        origin_filepath=origin_path,
        target_filepath=target_path,
        is_new_file=is_new_file,
        hunks=hunks,
    )
