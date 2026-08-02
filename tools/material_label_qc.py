# File: tools/material_label_qc.py
import json
import statistics
from collections import defaultdict

def load_labels(path):
    with open(path) as f:
        return [json.loads(line) for line in f]

def compute_rater_stats(labels):
    by_rater = defaultdict(list)
    for lab in labels:
        by_rater[lab["annotator"]].append(lab["k_material"])
    for r, ks in by_rater.items():
        print(r, "mean K:", statistics.mean(ks), "stdev:", statistics.pstdev(ks))

def find_inconsistent(labels, k_threshold=0.8):
    suspicious = []
    for lab in labels:
        evd = lab["evidential_score"]
        quant = lab["quantified_score"]
        k = lab["k_material"]
        if k > k_threshold and (evd + quant) < 0.6:
            suspicious.append(lab)
    return suspicious
