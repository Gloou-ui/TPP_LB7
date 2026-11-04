#ifndef POLYNOMIAL_SOLVER_H
#define POLYNOMIAL_SOLVER_H

#include <stddef.h>

#if defined(POLYNOMIAL_SOLVER_STATIC)
#define POLYNOMIAL_SOLVER_API
#elif defined(_WIN32)
#if defined(POLYNOMIAL_SOLVER_BUILD)
#define POLYNOMIAL_SOLVER_API __declspec(dllexport)
#else
#define POLYNOMIAL_SOLVER_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define POLYNOMIAL_SOLVER_API __attribute__((visibility("default")))
#else
#define POLYNOMIAL_SOLVER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PolynomialStatus {
    POLYNOMIAL_STATUS_OK = 0,
    POLYNOMIAL_STATUS_INVALID_ARGUMENT = 1,
    POLYNOMIAL_STATUS_BUFFER_TOO_SMALL = 2,
    POLYNOMIAL_STATUS_NO_CONVERGENCE = 3,
    POLYNOMIAL_STATUS_NUMERIC_ERROR = 4,
    POLYNOMIAL_STATUS_INTERNAL_ERROR = 5
} PolynomialStatus;


POLYNOMIAL_SOLVER_API PolynomialStatus polynomial_evaluate(
    const double* coefficients,
    size_t coefficient_count,
    double x,
    double* result
);


POLYNOMIAL_SOLVER_API PolynomialStatus polynomial_derivative(
    const double* coefficients,
    size_t coefficient_count,
    double* result,
    size_t result_capacity,
    size_t* result_count
);

POLYNOMIAL_SOLVER_API PolynomialStatus polynomial_newton_root(
    const double* coefficients,
    size_t coefficient_count,
    double initial_guess,
    double tolerance,
    int max_iterations,
    double* result
);


POLYNOMIAL_SOLVER_API PolynomialStatus polynomial_find_roots(
    const double* coefficients,
    size_t coefficient_count,
    double tolerance,
    double* roots,
    size_t root_capacity,
    size_t* root_count
);

#ifdef __cplusplus
}
#endif

#endif
