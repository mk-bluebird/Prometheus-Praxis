# File: python/eco_restoration/cmip6_hex_quantile_downscale.py

import argparse
import csv
import sqlite3
import numpy as np
import xarray as xr

parser = argparse.ArgumentParser()
parser.add_argument("model_baseline_netcdf")
parser.add_argument("observed_baseline_netcdf")
parser.add_argument("ssp585_netcdf")
parser.add_argument("hexes_csv")
parser.add_argument("output_sqlite")
args = parser.parse_args()

model_baseline = xr.open_dataset(args.model_baseline_netcdf)
observed_baseline = xr.open_dataset(args.observed_baseline_netcdf)
future = xr.open_dataset(args.ssp585_netcdf)

def temperature_c(values, units):
    return values - 273.15 if units.lower().startswith("k") else values

def precipitation_mm_day(values, units):
    return values * 86400.0 if "s-1" in units or "s^-1" in units else values

def quantile_map(values, model_reference, observed_reference):
    probabilities = np.linspace(0.0, 1.0, 101)
    model_quantiles = np.quantile(model_reference, probabilities)
    observed_quantiles = np.quantile(observed_reference, probabilities)
    return np.interp(values, model_quantiles, observed_quantiles)

with open(args.hexes_csv, newline="", encoding="utf-8") as file:
    hexes = list(csv.DictReader(file))

database = sqlite3.connect(args.output_sqlite)
database.execute("""
CREATE TABLE IF NOT EXISTS hex_climate_projection(
    hex_anchor INTEGER NOT NULL,
    scenario TEXT NOT NULL,
    target_year INTEGER NOT NULL,
    t_base_c REAL NOT NULL,
    t_range_c REAL NOT NULL CHECK(t_range_c >= 0),
    precipitation_mm_day REAL NOT NULL CHECK(precipitation_mm_day >= 0),
    PRIMARY KEY(hex_anchor, scenario, target_year)
) STRICT
""")

for hex_cell in hexes:
    anchor = int(hex_cell["hex_anchor"])
    latitude = float(hex_cell["latitude"])
    longitude = float(hex_cell["longitude"])

    model_tas = temperature_c(
        model_baseline["tas"].sel(lat=latitude, lon=longitude, method="nearest").values,
        model_baseline["tas"].attrs.get("units", "")
    )
    observed_tas = temperature_c(
        observed_baseline["tas"].sel(lat=latitude, lon=longitude, method="nearest").values,
        observed_baseline["tas"].attrs.get("units", "")
    )
    model_pr = precipitation_mm_day(
        model_baseline["pr"].sel(lat=latitude, lon=longitude, method="nearest").values,
        model_baseline["pr"].attrs.get("units", "")
    )
    observed_pr = precipitation_mm_day(
        observed_baseline["pr"].sel(lat=latitude, lon=longitude, method="nearest").values,
        observed_baseline["pr"].attrs.get("units", "")
    )

    future_tas = temperature_c(
        future["tas"].sel(lat=latitude, lon=longitude, method="nearest"),
        future["tas"].attrs.get("units", "")
    )
    future_pr = precipitation_mm_day(
        future["pr"].sel(lat=latitude, lon=longitude, method="nearest"),
        future["pr"].attrs.get("units", "")
    )

    corrected_tas = quantile_map(future_tas.values, model_tas, observed_tas)
    corrected_pr = np.maximum(0.0, quantile_map(future_pr.values, model_pr, observed_pr))
    years = future_tas["time"].dt.year.values

    for target_year in (2030, 2050, 2080):
        window = np.abs(years - target_year) <= 5
        if not np.any(window):
            continue
        values = corrected_tas[window]
        database.execute(
            "INSERT INTO hex_climate_projection VALUES(?,?,?,?,?,?) "
            "ON CONFLICT(hex_anchor,scenario,target_year) DO UPDATE SET "
            "t_base_c=excluded.t_base_c,t_range_c=excluded.t_range_c,"
            "precipitation_mm_day=excluded.precipitation_mm_day",
            (
                anchor, "ssp585", target_year,
                float(np.mean(values)),
                float(np.quantile(values, 0.95) - np.quantile(values, 0.05)),
                float(np.mean(corrected_pr[window]))
            )
        )

database.commit()
database.close()
