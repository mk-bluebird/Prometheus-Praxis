// File: ecorestorationshard/src/canal_tile_optimization.hpp

#pragma once

#include <vector>
#include <functional>

struct TileField {
    // Discrete approximation of u(x) along canal segment.
    std::vector<double> u;  // tile density per cell, 0..1
    double dx;              // cell length [m]
};

struct CanalState {
    double length_m;
    double discharge_m3s;
    double head_m;
};

struct RiskCoords {
    double r_hyd;
    double r_bio;
    double r_mat;
};

struct LyapunovWeights {
    double w_hyd;
    double w_bio;
    double w_mat;
};

struct TileOptimizationResult {
    TileField optimalField;
    double energyReqJ;
    double residualVt;
};

class CanalTileOptimizer {
public:
    CanalTileOptimizer(
        const CanalState &state,
        const LyapunovWeights &weights,
        double vtSafeBand
    )
    : state_(state), weights_(weights), vtSafeBand_(vtSafeBand) {}

    // User supplies hydraulic and risk models as functionals.
    void setEnergyDensityModel(
        const std::function<double(double,double)> &model
    ) {
        // model(x,u) -> energy density [J/m]
        energyDensity_ = model;
    }

    void setRiskModel(
        const std::function<RiskCoords(double,double)> &model
    ) {
        // model(x,u) -> risk coordinates for that cell.
        riskModel_ = model;
    }

    TileOptimizationResult optimize(std::size_t nCells) const {
        TileField field;
        field.u.assign(nCells, 0.0);
        field.dx = state_.length_m / static_cast<double>(nCells);

        // Simple heuristic optimizer: increase tile density where
        // energy density is high and risk penalty is low, subject
        // to global Vt constraint.
        double vt = 0.0;
        double energy = 0.0;

        for (std::size_t i = 0; i < nCells; ++i) {
            double x = (i + 0.5) * field.dx;
            // Start from a baseline tile density, e.g. 0.5.
            double u = 0.5;
            RiskCoords rc = riskModel_(x, u);
            double vt_cell = weights_.w_hyd * rc.r_hyd * rc.r_hyd
                           + weights_.w_bio * rc.r_bio * rc.r_bio
                           + weights_.w_mat * rc.r_mat * rc.r_mat;
            double e_cell = energyDensity_(x, u);

            // Simple constraint check: if vt_cell too high, reduce tiles.
            if (vt + vt_cell > vtSafeBand_) {
                u = 0.0; // no tiles in high-risk zones
                rc = riskModel_(x, u);
                vt_cell = weights_.w_hyd * rc.r_hyd * rc.r_hyd
                        + weights_.w_bio * rc.r_bio * rc.r_bio
                        + weights_.w_mat * rc.r_mat * rc.r_mat;
                e_cell = energyDensity_(x, u);
            }

            field.u[i] = u;
            vt += vt_cell * field.dx;
            energy += e_cell * field.dx;
        }

        TileOptimizationResult res;
        res.optimalField = field;
        res.energyReqJ   = energy;
        res.residualVt   = vt;
        return res;
    }

private:
    CanalState state_;
    LyapunovWeights weights_;
    double vtSafeBand_;
    std::function<double(double,double)> energyDensity_;
    std::function<RiskCoords(double,double)> riskModel_;
};
