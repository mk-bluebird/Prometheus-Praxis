#!/usr/bin/env python3
"""
File: city_os/phoenix/heat_island/tools/fit_uhi_params.py

Purpose:
    Fit a regularized regression model of Urban Heat Island (UHI) intensity
    against NDVI, NDBI, and NDWI for the Phoenix hex grid, using hex-level
    metrics exported from Google Earth Engine. Emit:
      - calibration_params.json (α, β, γ and metadata)
      - hex_metrics.json (per-hex UHI and feasibility fields)

Usage example:
    python tools/fit_uhi_params.py \
      --input-geojson data/phoenix_hex_landsat_stats_2020_summer.geojson \
      --output-calibration data/calibration_params.json \
      --output-hex-metrics data/hex_metrics.json \
      --season-label 2020-Summer

Dependencies:
    - geopandas
    - numpy
    - scikit-learn
"""

import argparse
import json
from pathlib import Path

import geopandas as gpd  # GeoJSON ingestion.[96][109]
import numpy as np
from sklearn.linear_model import Ridge  # L2-regularized regression.[95][108]
from sklearn.model_selection import train_test_split
from sklearn.metrics import r2_score, mean_squared_error


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Fit UHI regression and emit calibration_params.json + hex_metrics.json"
    )
    parser.add_argument(
        "--input-geojson",
        required=True,
        help="Path to GEE-exported hex stats GeoJSON",
    )
    parser.add_argument(
        "--output-calibration",
        required=True,
        help="Path to write calibration_params.json",
    )
    parser.add_argument(
        "--output-hex-metrics",
        required=True,
        help="Path to write hex_metrics.json",
    )
    parser.add_argument(
        "--season-label",
        default="unknown-season",
        help="Season label, e.g. 2020-Summer",
    )
    parser.add_argument(
        "--city-label",
        default="Phoenix-AZ",
        help="City label",
    )
    parser.add_argument(
        "--ridge-alpha",
        type=float,
        default=1.0,
        help="Ridge regularization strength (alpha)",
    )
    parser.add_argument(
        "--rural-ndbi-max",
        type=float,
        default=0.0,
        help="Max NDBI for rural selection",
    )
    parser.add_argument(
        "--rural-ndvi-min",
        type=float,
        default=0.1,
        help="Min NDVI for rural selection",
    )
    parser.add_argument(
        "--rural-ndvi-max",
        type=float,
        default=0.3,
        help="Max NDVI for rural selection",
    )
    return parser.parse_args()


def select_rural_hexes(
    df: gpd.GeoDataFrame,
    ndbi_col: str,
    ndvi_col: str,
    rural_ndbi_max: float,
    rural_ndvi_min: float,
    rural_ndvi_max: float,
) -> np.ndarray:
    """Return boolean mask for rural hexes based on NDBI/NDVI thresholds."""
    ndbi = df[ndbi_col].values
    ndvi = df[ndvi_col].values
    mask = (ndbi <= rural_ndbi_max) & (ndvi >= rural_ndvi_min) & (ndvi <= rural_ndvi_max)
    return mask


def compute_rural_reference_lst(df: gpd.GeoDataFrame, lst_col: str, rural_mask: np.ndarray) -> float:
    """Compute rural reference LST (median) in Celsius."""
    rural_lst = df.loc[rural_mask, lst_col].values
    if rural_lst.size == 0:
        raise ValueError("No rural hexes selected; adjust rural selection thresholds.")
    return float(np.median(rural_lst))


def fit_uhi_regression(
    df: gpd.GeoDataFrame,
    lst_col: str,
    ndvi_col: str,
    ndbi_col: str,
    ndwi_col: str,
    rural_mask: np.ndarray,
    ridge_alpha: float,
):
    """Fit UHI ~ NDVI + NDBI + NDWI using Ridge regression."""
    # Compute rural reference and UHI for each hex.
    rural_ref = compute_rural_reference_lst(df, lst_col, rural_mask)
    lst = df[lst_col].values
    uhi = lst - rural_ref

    # Prepare feature matrix.
    ndvi = df[ndvi_col].values
    ndbi = df[ndbi_col].values
    ndwi = df[ndwi_col].values
    X = np.column_stack([ndvi, ndbi, ndwi])
    y = uhi

    # Split train/validation.
    X_train, X_valid, y_train, y_valid = train_test_split(
        X, y, test_size=0.2, random_state=42
    )

    # Fit Ridge regression.[95][108]
    model = Ridge(alpha=ridge_alpha)
    model.fit(X_train, y_train)

    # Evaluate.
    y_pred = model.predict(X_valid)
    r2 = r2_score(y_valid, y_pred)
    rmse = np.sqrt(mean_squared_error(y_valid, y_pred))

    # Extract coefficients: intercept + θV, θB, θW.
    intercept = float(model.intercept_)
    theta_v, theta_b, theta_w = (float(c) for c in model.coef_)

    return {
        "model": model,
        "rural_ref": rural_ref,
        "uhi": uhi,
        "theta": {
            "theta_0": intercept,
            "theta_v": theta_v,
            "theta_b": theta_b,
            "theta_w": theta_w,
        },
        "metrics": {
            "r2": float(r2),
            "rmse_degC": float(rmse),
            "n_hex_train": int(X_train.shape[0]),
            "n_hex_valid": int(X_valid.shape[0]),
        },
    }


def build_calibration_json(
    city_label: str,
    season_label: str,
    ridge_alpha: float,
    rural_ref: float,
    theta_v: float,
    theta_b: float,
    theta_w: float,
    metrics: dict,
    rural_selection_rule: str,
) -> dict:
    """Assemble calibration_params.json content."""
    return {
        "city": city_label,
        "dataset": "LANDSAT/LC08/C02_T1_L2",
        "season": season_label,
        "hex_grid_ref": "city_os/phoenix/hex_grid/v1",
        "regression_method": f"ridge_UHI_vs_NDVI_NDBI_NDWI_alpha={ridge_alpha}",
        "parameters": {
            "alpha": {
                "estimate": theta_v,
                "ci_low": theta_v,   # CI placeholders; refine with bootstrap if desired.
                "ci_high": theta_v,
                "units": "degC_per_unit_NDVI",
            },
            "beta": {
                "estimate": theta_b,
                "ci_low": theta_b,
                "ci_high": theta_b,
                "units": "degC_per_unit_NDBI",
            },
            "gamma": {
                "estimate": theta_w,
                "ci_low": theta_w,
                "ci_high": theta_w,
                "units": "degC_per_unit_NDWI",
            },
        },
        "model_metrics": metrics,
        "rural_reference": {
            "lst_c_median": rural_ref,
            "selection_rule": rural_selection_rule,
        },
    }


def build_hex_metrics_json(
    df: gpd.GeoDataFrame,
    uhi: np.ndarray,
    hex_id_col: str,
    lst_mean_col: str,
    lst_median_col: str,
    lst_std_col: str,
    ndvi_mean_col: str,
    ndvi_median_col: str,
    ndvi_std_col: str,
    ndbi_mean_col: str,
    ndbi_median_col: str,
    ndbi_std_col: str,
    ndwi_mean_col: str,
    ndwi_median_col: str,
    ndwi_std_col: str,
) -> list:
    """
    Build hex_metrics.json array from GeoDataFrame and computed UHI.
    Feasibility factors (roof_area_fraction, feasible_tree_factor,
    hydrology_feasibility) are placeholders here and should be filled by
    joining city OS layers before calling this function.
    """
    hex_metrics = []
    for idx, row in df.iterrows():
        # Placeholder feasibility factors; in practice, join from other datasets.
        roof_area_fraction = 0.0
        feasible_tree_factor = 0.0
        hydrology_feasibility = 0.0

        entry = {
            "hex_id": str(row[hex_id_col]),
            "mean_lst_c": float(row.get(lst_mean_col, np.nan)),
            "median_lst_c": float(row.get(lst_median_col, np.nan)),
            "stddev_lst_c": float(row.get(lst_std_col, np.nan)),
            "mean_ndvi": float(row.get(ndvi_mean_col, np.nan)),
            "median_ndvi": float(row.get(ndvi_median_col, np.nan)),
            "stddev_ndvi": float(row.get(ndvi_std_col, np.nan)),
            "mean_ndbi": float(row.get(ndbi_mean_col, np.nan)),
            "median_ndbi": float(row.get(ndbi_median_col, np.nan)),
            "stddev_ndbi": float(row.get(ndbi_std_col, np.nan)),
            "mean_ndwi": float(row.get(ndwi_mean_col, np.nan)),
            "median_ndwi": float(row.get(ndwi_median_col, np.nan)),
            "stddev_ndwi": float(row.get(ndwi_std_col, np.nan)),
            "uhi": float(uhi[idx]),
            "roof_area_fraction": roof_area_fraction,
            "feasible_tree_factor": feasible_tree_factor,
            "hydrology_feasibility": hydrology_feasibility,
        }
        hex_metrics.append(entry)
    return hex_metrics


def main() -> None:
    args = parse_args()

    input_path = Path(args.input_geojson)
    output_calib_path = Path(args.output_calibration)
    output_hex_path = Path(args.output_hex_metrics)

    # 1. Load GeoJSON with geopandas.[96][100][109]
    df = gpd.read_file(input_path)

    # Map GEE field names to local variables.
    hex_id_col = "hex_id"
    lst_mean_col = "mean_LST_C"
    lst_median_col = "median_LST_C"
    lst_std_col = "stdDev_LST_C"
    ndvi_mean_col = "mean_NDVI"
    ndvi_median_col = "median_NDVI"
    ndvi_std_col = "stdDev_NDVI"
    ndbi_mean_col = "mean_NDBI"
    ndbi_median_col = "median_NDBI"
    ndbi_std_col = "stdDev_NDBI"
    ndwi_mean_col = "mean_NDWI"
    ndwi_median_col = "median_NDWI"
    ndwi_std_col = "stdDev_NDWI"

    # 2. Select rural hexes.
    rural_mask = select_rural_hexes(
        df=df,
        ndbi_col=ndbi_mean_col,
        ndvi_col=ndvi_mean_col,
        rural_ndbi_max=args.rural_ndbi_max,
        rural_ndvi_min=args.rural_ndvi_min,
        rural_ndvi_max=args.rural_ndvi_max,
    )

    rural_selection_rule = (
        f"NDBI <= {args.rural_ndbi_max} AND "
        f"NDVI between {args.rural_ndvi_min} and {args.rural_ndvi_max}"
    )

    # 3. Fit UHI regression.
    fit_result = fit_uhi_regression(
        df=df,
        lst_col=lst_mean_col,
        ndvi_col=ndvi_mean_col,
        ndbi_col=ndbi_mean_col,
        ndwi_col=ndwi_mean_col,
        rural_mask=rural_mask,
        ridge_alpha=args.ridge_alpha,
    )

    rural_ref = fit_result["rural_ref"]
    uhi = fit_result["uhi"]
    theta = fit_result["theta"]
    metrics = fit_result["metrics"]

    # 4. Build and write calibration_params.json.
    calib_json = build_calibration_json(
        city_label=args.city_label,
        season_label=args.season_label,
        ridge_alpha=args.ridge_alpha,
        rural_ref=rural_ref,
        theta_v=theta["theta_v"],
        theta_b=theta["theta_b"],
        theta_w=theta["theta_w"],
        metrics=metrics,
        rural_selection_rule=rural_selection_rule,
    )

    output_calib_path.parent.mkdir(parents=True, exist_ok=True)
    with output_calib_path.open("w", encoding="utf-8") as f:
        json.dump(calib_json, f, indent=2)

    # 5. Build and write hex_metrics.json.
    hex_metrics_json = build_hex_metrics_json(
        df=df,
        uhi=uhi,
        hex_id_col=hex_id_col,
        lst_mean_col=lst_mean_col,
        lst_median_col=lst_median_col,
        lst_std_col=lst_std_col,
        ndvi_mean_col=ndvi_mean_col,
        ndvi_median_col=ndvi_median_col,
        ndvi_std_col=ndvi_std_col,
        ndbi_mean_col=ndbi_mean_col,
        ndbi_median_col=ndbi_median_col,
        ndbi_std_col=ndbi_std_col,
        ndwi_mean_col=ndwi_mean_col,
        ndwi_median_col=ndwi_median_col,
        ndwi_std_col=ndwi_std_col,
    )

    output_hex_path.parent.mkdir(parents=True, exist_ok=True)
    with output_hex_path.open("w", encoding="utf-8") as f:
        json.dump(hex_metrics_json, f, indent=2)

    print(f"Written calibration params to: {output_calib_path}")
    print(f"Written hex metrics to:       {output_hex_path}")


if __name__ == "__main__":
    main()
