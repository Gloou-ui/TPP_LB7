#include "PolynomialSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace polynomial {
using Polynomial = std::vector<double>;

static double evaluateImpl(const Polynomial& coefficients, double x);
static Polynomial derivativeImpl(const Polynomial& coefficients);
static double newtonRootImpl(
    const Polynomial& coefficients,
    double initialGuess,
    double tolerance,
    int maxIterations
);
static std::vector<double> findRootsImpl(
    const Polynomial& coefficients,
    double tolerance
);

namespace {

void validatePolynomial(const Polynomial& coefficients) {
    if (coefficients.empty()) {
        throw std::invalid_argument("A polynomial must have at least one coefficient.");
    }
    for (double coefficient : coefficients) {
        if (!std::isfinite(coefficient)) {
            throw std::invalid_argument("Polynomial coefficients must be finite.");
        }
    }
}

Polynomial trimLeadingZeros(Polynomial coefficients) {
    while (coefficients.size() > 1 && coefficients.back() == 0.0) {
        coefficients.pop_back();
    }
    return coefficients;
}

long double evaluateLong(const Polynomial& coefficients, long double x) {
    long double result = 0.0L;
    for (auto it = coefficients.rbegin(); it != coefficients.rend(); ++it) {
        result = result * x + static_cast<long double>(*it);
    }
    return result;
}

long double evaluationScale(const Polynomial& coefficients, long double x) {
    long double scale = 0.0L;
    const long double absX = std::abs(x);
    for (auto it = coefficients.rbegin(); it != coefficients.rend(); ++it) {
        scale = scale * absX + std::abs(static_cast<long double>(*it));
    }
    return scale;
}

bool isApproximateRoot(
    const Polynomial& coefficients,
    long double x,
    double tolerance
) {
    const long double value = evaluateLong(coefficients, x);
    const long double scale = evaluationScale(coefficients, x);
    if (!std::isfinite(value) || !std::isfinite(scale)) {
        throw std::overflow_error("Polynomial evaluation overflowed.");
    }
    return std::abs(value) <=
           static_cast<long double>(tolerance) * std::max(1.0L, scale);
}

void addUniqueRoot(std::vector<double>& roots, double root, double tolerance) {
    const double threshold =
        tolerance * std::max(1.0, std::abs(root));
    for (double existing : roots) {
        if (std::abs(existing - root) <= threshold) {
            return;
        }
    }
    roots.push_back(root);
}

double bisectRoot(
    const Polynomial& coefficients,
    long double left,
    long double right,
    double tolerance
) {
    long double leftValue = evaluateLong(coefficients, left);
    if (leftValue == 0.0L) {
        return static_cast<double>(left);
    }

    for (int iteration = 0; iteration < 2048; ++iteration) {
        const long double middle = left + (right - left) / 2.0L;
        if (middle == left || middle == right ||
            right - left <= static_cast<long double>(tolerance) *
                                std::max(1.0L, std::abs(middle))) {
            return static_cast<double>(middle);
        }

        const long double middleValue = evaluateLong(coefficients, middle);
        if (!std::isfinite(middleValue)) {
            throw std::overflow_error("Polynomial evaluation overflowed.");
        }
        if (middleValue == 0.0L) {
            return static_cast<double>(middle);
        }

        if (std::signbit(leftValue) != std::signbit(middleValue)) {
            right = middle;
        } else {
            left = middle;
            leftValue = middleValue;
        }
    }

    throw std::runtime_error("Bisection did not converge.");
}

std::vector<double> findRootsRecursive(
    const Polynomial& input,
    double tolerance
) {
    const Polynomial coefficients = trimLeadingZeros(input);
    const std::size_t degree = coefficients.size() - 1;

    if (degree == 0) {
        return {};
    }

    if (degree == 1) {
        const long double root =
            -static_cast<long double>(coefficients[0]) /
            static_cast<long double>(coefficients[1]);
        if (!std::isfinite(root) ||
            std::abs(root) > std::numeric_limits<double>::max()) {
            throw std::overflow_error("A polynomial root is outside the double range.");
        }
        return {static_cast<double>(root)};
    }

    const Polynomial slope = derivativeImpl(coefficients);
    std::vector<double> criticalPoints =
        findRootsRecursive(slope, tolerance);

    long double maxRatio = 0.0L;
    const long double leading =
        std::abs(static_cast<long double>(coefficients.back()));
    for (std::size_t i = 0; i < degree; ++i) {
        maxRatio = std::max(
            maxRatio,
            std::abs(static_cast<long double>(coefficients[i])) / leading
        );
    }
    const long double bound = 1.0L + maxRatio;
    if (!std::isfinite(bound) ||
        bound > std::numeric_limits<double>::max()) {
        throw std::overflow_error("The polynomial root bound exceeds the double range.");
    }

    criticalPoints.erase(
        std::remove_if(
            criticalPoints.begin(),
            criticalPoints.end(),
            [bound](double point) {
                return static_cast<long double>(point) <= -bound ||
                       static_cast<long double>(point) >= bound;
            }
        ),
        criticalPoints.end()
    );
    std::sort(criticalPoints.begin(), criticalPoints.end());

    std::vector<double> roots;
    for (double point : criticalPoints) {
        if (isApproximateRoot(coefficients, point, tolerance)) {
            addUniqueRoot(roots, point, tolerance);
        }
    }

    std::vector<long double> boundaries;
    boundaries.reserve(criticalPoints.size() + 2);
    boundaries.push_back(-bound);
    for (double point : criticalPoints) {
        boundaries.push_back(static_cast<long double>(point));
    }
    boundaries.push_back(bound);

    for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
        const long double left = boundaries[i];
        const long double right = boundaries[i + 1];
        const long double leftValue = evaluateLong(coefficients, left);
        const long double rightValue = evaluateLong(coefficients, right);
        if (!std::isfinite(leftValue) || !std::isfinite(rightValue)) {
            throw std::overflow_error("Polynomial evaluation overflowed.");
        }
        if (leftValue != 0.0L && rightValue != 0.0L &&
            std::signbit(leftValue) != std::signbit(rightValue)) {
            addUniqueRoot(
                roots,
                bisectRoot(coefficients, left, right, tolerance),
                tolerance
            );
        }
    }

    std::sort(roots.begin(), roots.end());
    return roots;
}

}  // namespace

double evaluateImpl(const Polynomial& coefficients, double x) {
    validatePolynomial(coefficients);
    if (!std::isfinite(x)) {
        throw std::invalid_argument("The evaluation point must be finite.");
    }
    const long double result =
        evaluateLong(coefficients, static_cast<long double>(x));
    if (!std::isfinite(result) ||
        std::abs(result) > std::numeric_limits<double>::max()) {
        throw std::overflow_error("Polynomial evaluation exceeds the double range.");
    }
    return static_cast<double>(result);
}

Polynomial derivativeImpl(const Polynomial& coefficients) {
    validatePolynomial(coefficients);
    if (coefficients.size() == 1) {
        return {0.0};
    }

    Polynomial result(coefficients.size() - 1);
    for (std::size_t i = 1; i < coefficients.size(); ++i) {
        const long double value =
            static_cast<long double>(coefficients[i]) *
            static_cast<long double>(i);
        if (!std::isfinite(value) ||
            std::abs(value) > std::numeric_limits<double>::max()) {
            throw std::overflow_error("A derivative coefficient exceeds the double range.");
        }
        result[i - 1] = static_cast<double>(value);
    }
    return result;
}

double newtonRootImpl(
    const Polynomial& coefficients,
    double initialGuess,
    double tolerance,
    int maxIterations
) {
    validatePolynomial(coefficients);
    if (!std::isfinite(initialGuess)) {
        throw std::invalid_argument("The initial guess must be finite.");
    }
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        throw std::invalid_argument("Tolerance must be finite and positive.");
    }
    if (maxIterations <= 0) {
        throw std::invalid_argument("The iteration limit must be positive.");
    }

    const Polynomial slope = derivativeImpl(coefficients);
    long double x = initialGuess;
    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        if (isApproximateRoot(coefficients, x, tolerance)) {
            return static_cast<double>(x);
        }

        const long double value = evaluateLong(coefficients, x);
        const long double slopeValue = evaluateLong(slope, x);
        if (!std::isfinite(value) || !std::isfinite(slopeValue)) {
            throw std::overflow_error("Newton iteration overflowed.");
        }
        if (slopeValue == 0.0L) {
            throw std::runtime_error("Newton's method reached a zero derivative.");
        }

        const long double next = x - value / slopeValue;
        if (!std::isfinite(next) ||
            std::abs(next) > std::numeric_limits<double>::max()) {
            throw std::overflow_error("Newton's method produced an out-of-range value.");
        }
        if (std::abs(next - x) <=
            static_cast<long double>(tolerance) *
                std::max(1.0L, std::abs(next))) {
            if (isApproximateRoot(coefficients, next, tolerance)) {
                return static_cast<double>(next);
            }
            throw std::runtime_error("Newton's method stagnated before reaching a root.");
        }
        x = next;
    }

    throw std::runtime_error("Newton's method did not converge.");
}

std::vector<double> findRootsImpl(
    const Polynomial& coefficients,
    double tolerance
) {
    validatePolynomial(coefficients);
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        throw std::invalid_argument("Tolerance must be finite and positive.");
    }

    const Polynomial trimmed = trimLeadingZeros(coefficients);
    if (trimmed.size() == 1 && trimmed[0] == 0.0) {
        throw std::invalid_argument(
            "The zero polynomial has infinitely many roots."
        );
    }
    return findRootsRecursive(trimmed, tolerance);
}

}  // namespace polynomial

namespace {

polynomial::Polynomial makePolynomial(
    const double* coefficients,
    size_t coefficientCount
) {
    if (coefficients == nullptr || coefficientCount == 0) {
        throw std::invalid_argument(
            "Coefficients must not be null and must contain at least one value."
        );
    }
    return polynomial::Polynomial(
        coefficients,
        coefficients + coefficientCount
    );
}

PolynomialStatus mapException() {
    try {
        throw;
    } catch (const std::invalid_argument&) {
        return POLYNOMIAL_STATUS_INVALID_ARGUMENT;
    } catch (const std::overflow_error&) {
        return POLYNOMIAL_STATUS_NUMERIC_ERROR;
    } catch (const std::runtime_error&) {
        return POLYNOMIAL_STATUS_NO_CONVERGENCE;
    } catch (...) {
        return POLYNOMIAL_STATUS_INTERNAL_ERROR;
    }
}

}  // namespace

extern "C" {

PolynomialStatus polynomial_evaluate(
    const double* coefficients,
    size_t coefficient_count,
    double x,
    double* result
) {
    if (result == nullptr) {
        return POLYNOMIAL_STATUS_INVALID_ARGUMENT;
    }
    try {
        *result = polynomial::evaluateImpl(
            makePolynomial(coefficients, coefficient_count),
            x
        );
        return POLYNOMIAL_STATUS_OK;
    } catch (...) {
        return mapException();
    }
}

PolynomialStatus polynomial_derivative(
    const double* coefficients,
    size_t coefficient_count,
    double* result,
    size_t result_capacity,
    size_t* result_count
) {
    if (result_count == nullptr) {
        return POLYNOMIAL_STATUS_INVALID_ARGUMENT;
    }
    *result_count = 0;
    try {
        const polynomial::Polynomial derivative =
            polynomial::derivativeImpl(
                makePolynomial(coefficients, coefficient_count)
            );
        *result_count = derivative.size();
        if (result_capacity < derivative.size() || result == nullptr) {
            return POLYNOMIAL_STATUS_BUFFER_TOO_SMALL;
        }
        std::copy(derivative.begin(), derivative.end(), result);
        return POLYNOMIAL_STATUS_OK;
    } catch (...) {
        return mapException();
    }
}

PolynomialStatus polynomial_newton_root(
    const double* coefficients,
    size_t coefficient_count,
    double initial_guess,
    double tolerance,
    int max_iterations,
    double* result
) {
    if (result == nullptr) {
        return POLYNOMIAL_STATUS_INVALID_ARGUMENT;
    }
    try {
        *result = polynomial::newtonRootImpl(
            makePolynomial(coefficients, coefficient_count),
            initial_guess,
            tolerance,
            max_iterations
        );
        return POLYNOMIAL_STATUS_OK;
    } catch (...) {
        return mapException();
    }
}

PolynomialStatus polynomial_find_roots(
    const double* coefficients,
    size_t coefficient_count,
    double tolerance,
    double* roots,
    size_t root_capacity,
    size_t* root_count
) {
    if (root_count == nullptr) {
        return POLYNOMIAL_STATUS_INVALID_ARGUMENT;
    }
    *root_count = 0;
    try {
        const std::vector<double> foundRoots = polynomial::findRootsImpl(
            makePolynomial(coefficients, coefficient_count),
            tolerance
        );
        *root_count = foundRoots.size();
        if (root_capacity < foundRoots.size() ||
            (!foundRoots.empty() && roots == nullptr)) {
            return POLYNOMIAL_STATUS_BUFFER_TOO_SMALL;
        }
        if (!foundRoots.empty()) {
            std::copy(foundRoots.begin(), foundRoots.end(), roots);
        }
        return POLYNOMIAL_STATUS_OK;
    } catch (...) {
        return mapException();
    }
}

}  // extern "C"