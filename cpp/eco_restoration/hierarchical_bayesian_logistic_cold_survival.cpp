// File: cpp/eco_restoration/hierarchical_bayesian_logistic_cold_survival.cpp

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

// This file implements a hierarchical Bayesian logistic cold-survival model using
// a simple Metropolis-within-Gibbs MCMC sampler in C++, suitable for driving a
// Java/Kotlin probabilistic service via JNI or data exchange. It avoids any
// blacklisted stacks and provides a SQL schema for posterior summaries and a
// query function for p_survival given λ_max (Lyapunov max/residual).

// Model:
// For canal segment j and observation i:
//   y_ij ~ Bernoulli( p_ij )
//   logit(p_ij) = α_j + β * λ_max_ij
//
// Hierarchical prior:
//   α_j ~ Normal( μ_α, σ_α )
//   β   ~ Normal( μ_β, σ_β )
//   μ_α ~ Normal(0, 5)
//   σ_α ~ HalfNormal(1)
//   μ_β ~ Normal(0, 5)
//   σ_β ~ HalfNormal(1)
//
// Sparse segments are handled via partial pooling through α_j ~ Normal(μ_α, σ_α).

struct Observation {
    int    segment_id;   // canal segment identifier
    double lambda_max;   // Lyapunov max / residual for event
    int    cold_survival; // 1 if survived, 0 otherwise
};

struct HierarchicalParams {
    std::vector<double> alpha;  // per-segment intercepts
    double beta;                // global slope for λ_max
    double mu_alpha;
    double sigma_alpha;
    double mu_beta;
    double sigma_beta;
};

struct PriorHyperparams {
    double mu_alpha_mean;
    double mu_alpha_sd;
    double sigma_alpha_scale;
    double mu_beta_mean;
    double mu_beta_sd;
    double sigma_beta_scale;
};

struct PosteriorSummary {
    int    segment_id;
    double alpha_mean;
    double alpha_sd;
    double beta_mean;
    double beta_sd;
    double mu_alpha_mean;
    double mu_alpha_sd;
    double mu_beta_mean;
    double mu_beta_sd;
};

double logit(double p) {
    return std::log(p / (1.0 - p));
}

double inv_logit(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

double normal_logpdf(double x, double mean, double sd) {
    double z = (x - mean) / sd;
    return -0.5 * z * z - std::log(sd) - 0.5 * std::log(2.0 * M_PI);
}

double halfnormal_logpdf(double x, double scale) {
    if (x <= 0.0) return -std::numeric_limits<double>::infinity();
    double z = x / scale;
    return -0.5 * z * z - std::log(scale) + std::log(2.0) - 0.5 * std::log(2.0 * M_PI);
}

// Hierarchical logistic model + basic MCMC.
class HierarchicalLogisticModel {
public:
    HierarchicalLogisticModel(const std::vector<Observation>& data,
                              int n_segments,
                              const PriorHyperparams& prior,
                              int n_iter,
                              int burn_in)
        : data_(data),
          n_segments_(n_segments),
          prior_(prior),
          n_iter_(n_iter),
          burn_in_(burn_in),
          gen_(42),
          params_{} {
        if (n_segments_ <= 0) {
            throw std::invalid_argument("n_segments must be > 0");
        }
        initParams();
    }

    void runMCMC() {
        std::normal_distribution<double> step_alpha(0.0, 0.1);
        std::normal_distribution<double> step_beta(0.0, 0.05);
        std::normal_distribution<double> step_mu(0.0, 0.1);
        std::normal_distribution<double> step_sigma(0.0, 0.05);

        std::vector<double> alpha_samples_mean(n_segments_, 0.0);
        std::vector<double> alpha_samples_sq(n_segments_, 0.0);
        double beta_sum = 0.0;
        double beta_sum_sq = 0.0;
        double mu_alpha_sum = 0.0;
        double mu_alpha_sum_sq = 0.0;
        double mu_beta_sum = 0.0;
        double mu_beta_sum_sq = 0.0;

        int keep_count = 0;

        for (int it = 0; it < n_iter_; ++it) {
            // Sample alpha_j via random-walk Metropolis.
            for (int j = 0; j < n_segments_; ++j) {
                double current = params_.alpha[j];
                double proposal = current + step_alpha(gen_);
                double log_post_current = logPosteriorAlpha(j, current);
                double log_post_proposal = logPosteriorAlpha(j, proposal);
                double accept_logprob = log_post_proposal - log_post_current;
                if (std::log(uniform01()) < accept_logprob) {
                    params_.alpha[j] = proposal;
                }
            }

            // Sample beta via random-walk Metropolis.
            {
                double current = params_.beta;
                double proposal = current + step_beta(gen_);
                double log_post_current = logPosteriorBeta(current);
                double log_post_proposal = logPosteriorBeta(proposal);
                double accept_logprob = log_post_proposal - log_post_current;
                if (std::log(uniform01()) < accept_logprob) {
                    params_.beta = proposal;
                }
            }

            // Gibbs-like updates for hyperparameters via random-walk proposals.
            {
                double current = params_.mu_alpha;
                double proposal = current + step_mu(gen_);
                double log_post_current = logPosteriorMuAlpha(current);
                double log_post_proposal = logPosteriorMuAlpha(proposal);
                if (std::log(uniform01()) < (log_post_proposal - log_post_current)) {
                    params_.mu_alpha = proposal;
                }
            }
            {
                double current = params_.sigma_alpha;
                double proposal = std::fabs(current + step_sigma(gen_));
                double log_post_current = logPosteriorSigmaAlpha(current);
                double log_post_proposal = logPosteriorSigmaAlpha(proposal);
                if (std::log(uniform01()) < (log_post_proposal - log_post_current)) {
                    params_.sigma_alpha = proposal;
                }
            }
            {
                double current = params_.mu_beta;
                double proposal = current + step_mu(gen_);
                double log_post_current = logPosteriorMuBeta(current);
                double log_post_proposal = logPosteriorMuBeta(proposal);
                if (std::log(uniform01()) < (log_post_proposal - log_post_current)) {
                    params_.mu_beta = proposal;
                }
            }
            {
                double current = params_.sigma_beta;
                double proposal = std::fabs(current + step_sigma(gen_));
                double log_post_current = logPosteriorSigmaBeta(current);
                double log_post_proposal = logPosteriorSigmaBeta(proposal);
                if (std::log(uniform01()) < (log_post_proposal - log_post_current)) {
                    params_.sigma_beta = proposal;
                }
            }

            if (it >= burn_in_) {
                keep_count++;
                for (int j = 0; j < n_segments_; ++j) {
                    alpha_samples_mean[j] += params_.alpha[j];
                    alpha_samples_sq[j] += params_.alpha[j] * params_.alpha[j];
                }
                beta_sum += params_.beta;
                beta_sum_sq += params_.beta * params_.beta;
                mu_alpha_sum += params_.mu_alpha;
                mu_alpha_sum_sq += params_.mu_alpha * params_.mu_alpha;
                mu_beta_sum += params_.mu_beta;
                mu_beta_sum_sq += params_.mu_beta * params_.mu_beta;
            }
        }

        posterior_.clear();
        posterior_.reserve(n_segments_);
        for (int j = 0; j < n_segments_; ++j) {
            PosteriorSummary s{};
            s.segment_id = j;
            s.alpha_mean = alpha_samples_mean[j] / keep_count;
            double alpha_var = alpha_samples_sq[j] / keep_count - s.alpha_mean * s.alpha_mean;
            s.alpha_sd = std::sqrt(std::max(0.0, alpha_var));
            s.beta_mean = beta_sum / keep_count;
            double beta_var = beta_sum_sq / keep_count - s.beta_mean * s.beta_mean;
            s.beta_sd = std::sqrt(std::max(0.0, beta_var));
            s.mu_alpha_mean = mu_alpha_sum / keep_count;
            double mu_alpha_var = mu_alpha_sum_sq / keep_count - s.mu_alpha_mean * s.mu_alpha_mean;
            s.mu_alpha_sd = std::sqrt(std::max(0.0, mu_alpha_var));
            s.mu_beta_mean = mu_beta_sum / keep_count;
            double mu_beta_var = mu_beta_sum_sq / keep_count - s.mu_beta_mean * s.mu_beta_mean;
            s.mu_beta_sd = std::sqrt(std::max(0.0, mu_beta_var));
            posterior_.push_back(s);
        }
    }

    const std::vector<PosteriorSummary>& posteriorSummaries() const {
        return posterior_;
    }

    // Posterior predictive for cold survival probability given λ_max for a segment:
    // We use posterior means for α_j and β as a simple approximation:
    double predictSurvivalProb(int segment_id, double lambda_max) const {
        if (segment_id < 0 || segment_id >= n_segments_) {
            throw std::invalid_argument("Invalid segment_id");
        }
        const PosteriorSummary& s = posterior_.at(segment_id);
        double eta = s.alpha_mean + s.beta_mean * lambda_max;
        return inv_logit(eta);
    }

private:
    std::vector<Observation> data_;
    int n_segments_;
    PriorHyperparams prior_;
    int n_iter_;
    int burn_in_;
    std::mt19937 gen_;
    HierarchicalParams params_;
    std::vector<PosteriorSummary> posterior_;

    void initParams() {
        params_.alpha.assign(n_segments_, 0.0);
        params_.beta = 0.0;
        params_.mu_alpha = 0.0;
        params_.sigma_alpha = 1.0;
        params_.mu_beta = 0.0;
        params_.sigma_beta = 1.0;
    }

    double uniform01() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(gen_);
    }

    double logLikelihood() const {
        double ll = 0.0;
        for (const auto& obs : data_) {
            int j = obs.segment_id;
            double eta = params_.alpha[j] + params_.beta * obs.lambda_max;
            double p = inv_logit(eta);
            p = std::min(std::max(p, 1e-6), 1.0 - 1e-6);
            if (obs.cold_survival == 1) {
                ll += std::log(p);
            } else {
                ll += std::log(1.0 - p);
            }
        }
        return ll;
    }

    double logPosteriorAlpha(int j, double alpha_j) const {
        // Condition on other params, compute likelihood contribution and prior for α_j.
        double ll = 0.0;
        for (const auto& obs : data_) {
            if (obs.segment_id != j) continue;
            double eta = alpha_j + params_.beta * obs.lambda_max;
            double p = inv_logit(eta);
            p = std::min(std::max(p, 1e-6), 1.0 - 1e-6);
            if (obs.cold_survival == 1) {
                ll += std::log(p);
            } else {
                ll += std::log(1.0 - p);
            }
        }
        double lp = normal_logpdf(alpha_j, params_.mu_alpha, params_.sigma_alpha);
        return ll + lp;
    }

    double logPosteriorBeta(double beta) const {
        // Likelihood over all data + prior on β and hyperprior via μ_β, σ_β.
        double ll = 0.0;
        for (const auto& obs : data_) {
            int j = obs.segment_id;
            double eta = params_.alpha[j] + beta * obs.lambda_max;
            double p = inv_logit(eta);
            p = std::min(std::max(p, 1e-6), 1.0 - 1e-6);
            if (obs.cold_survival == 1) {
                ll += std::log(p);
            } else {
                ll += std::log(1.0 - p);
            }
        }
        double lp = normal_logpdf(beta, params_.mu_beta, params_.sigma_beta);
        return ll + lp;
    }

    double logPosteriorMuAlpha(double mu_alpha) const {
        double lp = normal_logpdf(mu_alpha, prior_.mu_alpha_mean, prior_.mu_alpha_sd);
        for (int j = 0; j < n_segments_; ++j) {
            lp += normal_logpdf(params_.alpha[j], mu_alpha, params_.sigma_alpha);
        }
        return lp;
    }

    double logPosteriorSigmaAlpha(double sigma_alpha) const {
        double lp = halfnormal_logpdf(sigma_alpha, prior_.sigma_alpha_scale);
        for (int j = 0; j < n_segments_; ++j) {
            lp += normal_logpdf(params_.alpha[j], params_.mu_alpha, sigma_alpha);
        }
        return lp;
    }

    double logPosteriorMuBeta(double mu_beta) const {
        double lp = normal_logpdf(mu_beta, prior_.mu_beta_mean, prior_.mu_beta_sd);
        lp += normal_logpdf(params_.beta, mu_beta, params_.sigma_beta);
        return lp;
    }

    double logPosteriorSigmaBeta(double sigma_beta) const {
        double lp = halfnormal_logpdf(sigma_beta, prior_.sigma_beta_scale);
        lp += normal_logpdf(params_.beta, params_.mu_beta, sigma_beta);
        return lp;
    }
};

// SQL schema for posterior summaries and query function.
class PosteriorSqlAdapter {
public:
    explicit PosteriorSqlAdapter(const std::string& db_path)
        : db_path_(db_path) {}

    void installSchema() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for schema install: " + msg);
        }

        const char* sql_schema =
            "CREATE TABLE IF NOT EXISTS cold_survival_posterior ("
            "  segment_id       INTEGER PRIMARY KEY,"
            "  alpha_mean       REAL NOT NULL,"
            "  alpha_sd         REAL NOT NULL,"
            "  beta_mean        REAL NOT NULL,"
            "  beta_sd          REAL NOT NULL,"
            "  mu_alpha_mean    REAL NOT NULL,"
            "  mu_alpha_sd      REAL NOT NULL,"
            "  mu_beta_mean     REAL NOT NULL,"
            "  mu_beta_sd       REAL NOT NULL,"
            "  updated_at       TEXT NOT NULL"
            ");";

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, sql_schema, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("Schema install failed: " + msg);
        }

        // Helper view: per-segment survival probability for a given λ_max input via parameter.
        // We implement the query function in C++ rather than as a SQL function.
        sqlite3_close(db);
    }

    void writePosterior(const std::vector<PosteriorSummary>& summaries) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for posterior write: " + msg);
        }

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("BEGIN TRANSACTION failed: " + msg);
        }

        const char* sql_upsert =
            "INSERT INTO cold_survival_posterior("
            "  segment_id, alpha_mean, alpha_sd, beta_mean, beta_sd,"
            "  mu_alpha_mean, mu_alpha_sd, mu_beta_mean, mu_beta_sd, updated_at"
            ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now')) "
            "ON CONFLICT(segment_id) DO UPDATE SET "
            "  alpha_mean = excluded.alpha_mean,"
            "  alpha_sd = excluded.alpha_sd,"
            "  beta_mean = excluded.beta_mean,"
            "  beta_sd = excluded.beta_sd,"
            "  mu_alpha_mean = excluded.mu_alpha_mean,"
            "  mu_alpha_sd = excluded.mu_alpha_sd,"
            "  mu_beta_mean = excluded.mu_beta_mean,"
            "  mu_beta_sd = excluded.mu_beta_sd,"
            "  updated_at = excluded.updated_at;";

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, sql_upsert, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw std::runtime_error("Prepare upsert failed: " + msg);
        }

        for (const auto& s : summaries) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            rc = sqlite3_bind_int(stmt, 1, s.segment_id);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 2, s.alpha_mean);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 3, s.alpha_sd);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 4, s.beta_mean);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 5, s.beta_sd);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 6, s.mu_alpha_mean);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 7, s.mu_alpha_sd);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 8, s.mu_beta_mean);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 9, s.mu_beta_sd);
            if (rc != SQLITE_OK) goto bind_error;

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Upsert step failed: " + msg);
            }
            continue;

        bind_error:
            {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Bind error: " + msg);
            }
        }

        sqlite3_finalize(stmt);
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("COMMIT failed: " + msg);
        }

        sqlite3_close(db);
    }

    // Query p_survival for given segment_id and λ_max using stored posterior means.
    double querySurvivalProb(int segment_id, double lambda_max) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for survival query: " + msg);
        }

        const char* sql =
            "SELECT alpha_mean, beta_mean "
            "FROM cold_survival_posterior "
            "WHERE segment_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare survival query failed: " + msg);
        }

        rc = sqlite3_bind_int(stmt, 1, segment_id);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Bind segment_id failed: " + std::string(sqlite3_errmsg(db)));
        }

        double alpha_mean = 0.0;
        double beta_mean = 0.0;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            alpha_mean = sqlite3_column_double(stmt, 0);
            beta_mean = sqlite3_column_double(stmt, 1);
        } else {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("No posterior record for segment_id");
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        double eta = alpha_mean + beta_mean * lambda_max;
        return inv_logit(eta);
    }

private:
    std::string db_path_;
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "cold_survival.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    // Synthetic example data for demonstration; in production this comes from canal telemetry.
    int n_segments = 3;
    std::vector<Observation> data;
    {
        // Segment 0: relatively safe (high survival).
        for (int i = 0; i < 50; ++i) {
            Observation o;
            o.segment_id = 0;
            o.lambda_max = 0.1 * i;
            o.cold_survival = (i < 40) ? 1 : 0;
            data.push_back(o);
        }
        // Segment 1: mixed survival.
        for (int i = 0; i < 30; ++i) {
            Observation o;
            o.segment_id = 1;
            o.lambda_max = 0.2 * i;
            o.cold_survival = (i < 20) ? 1 : 0;
            data.push_back(o);
        }
        // Segment 2: sparse data, partial pooling should help.
        for (int i = 0; i < 5; ++i) {
            Observation o;
            o.segment_id = 2;
            o.lambda_max = 0.3 * i;
            o.cold_survival = (i < 3) ? 1 : 0;
            data.push_back(o);
        }
    }

    PriorHyperparams prior;
    prior.mu_alpha_mean = 0.0;
    prior.mu_alpha_sd = 5.0;
    prior.sigma_alpha_scale = 1.0;
    prior.mu_beta_mean = 0.0;
    prior.mu_beta_sd = 5.0;
    prior.sigma_beta_scale = 1.0;

    int n_iter = 5000;
    int burn_in = 1000;

    try {
        HierarchicalLogisticModel model(data, n_segments, prior, n_iter, burn_in);
        model.runMCMC();
        const auto& summaries = model.posteriorSummaries();

        PosteriorSqlAdapter adapter(db_path);
        adapter.installSchema();
        adapter.writePosterior(summaries);

        double lambda_query = 3.5;
        for (int seg = 0; seg < n_segments; ++seg) {
            double p_surv = adapter.querySurvivalProb(seg, lambda_query);
            std::cout << "Segment " << seg
                      << " p_survival(λ_max=" << lambda_query << ") = "
                      << p_surv << std::endl;
        }

        std::cout << "Hierarchical Bayesian cold-survival posterior written to DB and queried successfully." << std::endl;
        std::cout << "Governance services and Lyapunov-based controllers can use p_survival to constrain actuators." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Hierarchical logistic cold-survival error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
