# File: tools/material_label_cli.py
from dataclasses import dataclass
from material_corpus_builder import MaterialText

@dataclass
class MaterialLabel:
    material_id: str
    evidential_score: float
    quantified_score: float
    transparency_score: float
    measurability_score: float
    k_material: float
    annotator: str

def aggregate_k(evd, quant, transp, meas) -> float:
    # Simple weighted average; weights can be tuned with experts.
    raw = 0.3 * evd + 0.3 * quant + 0.2 * transp + 0.2 * meas
    return max(0.0, min(raw, 1.0))

def label_one(mat: MaterialText, annotator: str) -> MaterialLabel:
    print("\n=== Material ID:", mat.id, "Source:", mat.source, "===\n")
    print(mat.text)
    print("\nEnter scores (0.0–1.0):")
    evd   = float(input("Evidence density: "))
    quant = float(input("Quantified eco attributes: "))
    transp= float(input("Risk transparency: "))
    meas  = float(input("Measurability: "))

    k = aggregate_k(evd, quant, transp, meas)
    print("Computed K_material =", k)

    return MaterialLabel(
        material_id=mat.id,
        evidential_score=evd,
        quantified_score=quant,
        transparency_score=transp,
        measurability_score=meas,
        k_material=k,
        annotator=annotator,
    )
