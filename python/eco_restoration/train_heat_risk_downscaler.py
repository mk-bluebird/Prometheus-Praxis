# File: python/eco_restoration/train_heat_risk_downscaler.py
import argparse
import json
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error
from skl2onnx import to_onnx
from skl2onnx.common.data_types import FloatTensorType

parser = argparse.ArgumentParser()
parser.add_argument("training_csv")
parser.add_argument("onnx_output")
parser.add_argument("metadata_output")
args = parser.parse_args()

data = pd.read_csv(args.training_csv).dropna(
    subset=["coarse_lst_c", "ndvi", "albedo", "elevation_m", "hex_lst_c"]
)
features = ["coarse_lst_c", "ndvi", "albedo", "elevation_m"]
x = data[features].astype("float32")
y = data["hex_lst_c"].astype("float32")

x_train, x_test, y_train, y_test = train_test_split(
    x, y, test_size=0.20, random_state=42
)
model = RandomForestRegressor(
    n_estimators=300, min_samples_leaf=3, max_features=1.0,
    random_state=42, n_jobs=-1
).fit(x_train, y_train)

prediction = model.predict(x_test)
metadata = {
    "features": features,
    "validation_mae_c": float(mean_absolute_error(y_test, prediction)),
    "training_rows": int(len(x_train)),
    "validation_rows": int(len(x_test)),
}
with open(args.metadata_output, "w", encoding="utf-8") as output:
    json.dump(metadata, output, indent=2)

onnx = to_onnx(model, initial_types=[("features", FloatTensorType([None, 4]))])
with open(args.onnx_output, "wb") as output:
    output.write(onnx.SerializeToString())
