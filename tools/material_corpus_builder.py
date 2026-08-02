# File: tools/material_corpus_builder.py
import re
from dataclasses import dataclass
from typing import List

@dataclass
class MaterialText:
    id: str
    source: str
    text: str

ISO_PATTERN = re.compile(r"\bISO\s*\d{4,5}\b", re.IGNORECASE)
ASTM_PATTERN = re.compile(r"\bASTM\s+[A-Z]?\d+\b", re.IGNORECASE)
LCA_PATTERN = re.compile(r"\bEPD\b|\bLCA\b", re.IGNORECASE)
GREENWASH_PATTERN = re.compile(
    r"\beco[-\s]?friendly\b|\bsustainable\b|\bgreen\b",
    re.IGNORECASE,
)

def score_pre_label_priority(text: str) -> float:
    """Heuristic score for 'worth labeling'."""
    score = 0.0
    if ISO_PATTERN.search(text):
        score += 0.2
    if ASTM_PATTERN.search(text):
        score += 0.2
    if LCA_PATTERN.search(text):
        score += 0.2
    if GREENWASH_PATTERN.search(text):
        score += 0.1
    # Length penalty: very short texts give little signal
    length = len(text.split())
    if length < 10:
        score *= 0.3
    return min(score, 1.0)

def build_corpus(raw_rows) -> List[MaterialText]:
    corpus = []
    for row in raw_rows:
        text = row["description"]
        priority = score_pre_label_priority(text)
        if priority >= 0.2:  # discard low-value entries
            corpus.append(MaterialText(
                id=row["id"],
                source=row["source"],
                text=text,
            ))
    return corpus
