// File: cpp/tools/gaussian_elimination_augmented_matrix_demo.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>

namespace eco {

// Construct an augmented matrix [A | b] for a linear system A x = b,
// then solve it via Gaussian elimination (no pivoting).
class LinearSystemSolver {
public:
    // Solve A x = b for x, using an augmented matrix [A | b].
    static std::vector<double> solve(const std::vector<std::vector<double>>& A,
                                     const std::vector<double>& b) {
        std::size_t n = A.size();
        if (n == 0 || A[0].size() != n || b.size() != n) {
            throw std::runtime_error("Dimension mismatch in LinearSystemSolver::solve");
        }

        // Build augmented matrix M of size n x (n+1): [A | b]
        std::vector<std::vector<double>> M(n, std::vector<double>(n + 1, 0.0));
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                M[i][j] = A[i][j];
            }
            M[i][n] = b[i];
        }

        // Forward elimination
        for (std::size_t k = 0; k < n; ++k) {
            // Optional: pivoting could be added here.
            double pivot = M[k][k];
            if (std::fabs(pivot) < 1e-12) {
                // Degenerate pivot; in production, we would pivot or regularize.
                continue;
            }
            // Normalize pivot row
            for (std::size_t j = k; j < n + 1; ++j) {
                M[k][j] /= pivot;
            }
            // Eliminate below and above
            for (std::size_t i = 0; i < n; ++i) {
                if (i == k) continue;
                double factor = M[i][k];
                for (std::size_t j = k; j < n + 1; ++j) {
                    M[i][j] -= factor * M[k][j];
                }
            }
        }

        // Extract solution x from last column.
        std::vector<double> x(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = M[i][n];
        }
        return x;
    }

    static void print_augmented(const std::vector<std::vector<double>>& A,
                                const std::vector<double>& b) {
        std::size_t n = A.size();
        std::cout << "Augmented matrix [A | b]:\n";
        for (std::size_t i = 0; i < n; ++i) {
            std::cout << "  ";
            for (std::size_t j = 0; j < n; ++j) {
                std::cout << A[i][j] << " ";
            }
            std::cout << "| " << b[i] << "\n";
        }
    }
};

} // namespace eco

int main() {
    using namespace eco;

    // Example symmetric positive-definite system A x = b
    std::vector<std::vector<double>> A = {
        {4.0, 1.0, 0.5},
        {1.0, 3.0, 0.0},
        {0.5, 0.0, 2.0}
    };
    std::vector<double> b = {1.0, 2.0, 3.0};

    LinearSystemSolver::print_augmented(A, b);
    std::vector<double> x = LinearSystemSolver::solve(A, b);

    std::cout << "\nSolution x:\n";
    for (std::size_t i = 0; i < x.size(); ++i) {
        std::cout << "  x[" << i << "] = " << x[i] << "\n";
    }

    return 0;
}
