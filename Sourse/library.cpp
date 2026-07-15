#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <extcode.h>

// Structure definition matching LabVIEW 1D Array Layout in memory
typedef struct {
    int32_t dimSize;   // Number of elements in the array
    double elt[1];     // Pointer to the first element of data
} LVArray;
typedef LVArray** LVArrayHandle;

// Fast bitwise validation for NaN and Inf (1 instruction)
inline bool isInvalidBitwise(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(double));
    return (bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL;
}

extern "C" __declspec(dllexport) int32_t ThielSenProcessing(
    LVArrayHandle y_handle,
    const LVArrayHandle x_handle,
    const int16_t match_lengths,
    const uint16_t case_mode,
    double* slope,
    double* intercept
) {
    // 1. Basic memory pointers validation
    if (!x_handle || !(*x_handle) || !y_handle || !(*y_handle) || !slope || !intercept) {
        return -1;
    }

    // Extract actual sizes directly from LabVIEW memory handles
    int32_t x_size = (*x_handle)->dimSize;
    int32_t y_size = (*y_handle)->dimSize;

    // Helper lambda to assign NaNs and instantly clear the output array to empty (size = 0)
    auto handle_error = [&](int32_t error_code) {
        *slope = std::numeric_limits<double>::quiet_NaN();
        *intercept = std::numeric_limits<double>::quiet_NaN();
        (*y_handle)->dimSize = 0; // Forces LabVIEW to treat the array as completely EMPTY []
        return error_code;
    };

    // 2. Array length synchronization
    int32_t n = match_lengths ? std::min(x_size, y_size) : x_size;
    if (!match_lengths && x_size != y_size) {
        return handle_error(-20002); // Error: mismatched input array lengths
    }
    if (n <= 1) {
        return handle_error(-20006); // Error: 0 or 1 elements
    }

    // Direct pointers to raw data inside handles for fast loop performance
    const double* x_in = (*x_handle)->elt;
    double* y_in_out = (*y_handle)->elt;

    // 3. High-speed input validation for NaN or Inf
    for (int32_t i = 0; i < n; ++i) {
        if (isInvalidBitwise(x_in[i]) || isInvalidBitwise(y_in_out[i])) {
            return handle_error(-20068); // Error: input arrays contain NaN or Inf
        }
    }

    // 4. Scan for duplicate X (Only required for AUTO mode)
    bool has_duplicates = false;
    if (case_mode == 0) {
        for (int32_t i = 0; i < n - 1; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (std::abs(x_in[i] - x_in[j]) < 1e-9) {
                    has_duplicates = true;
                    break;
                }
            }
            if (has_duplicates) break;
        }
    }

    // 5. Pairwise calculation of Thiel-Sen slopes
    std::vector<double> slopes;
    size_t max_slopes = (static_cast<size_t>(n) * (n - 1)) / 2;
    slopes.reserve(max_slopes);

    // Branch splitting for maximum pipeline speed
    if (case_mode == 1 || case_mode == 2 || (case_mode == 0 && has_duplicates)) {
        for (int32_t i = 0; i < n; ++i) {
            for (int32_t j = i + 1; j < n; ++j) {
                double dx = x_in[j] - x_in[i];
                if (std::abs(dx) > 1e-9) {
                    slopes.push_back((y_in_out[j] - y_in_out[i]) / dx);
                }
            }
        }
    } else {
        // Direct, uninterrupted loop execution for pure AUTO mode (No branch penalties)
        for (int32_t i = 0; i < n; ++i) {
            for (int32_t j = i + 1; j < n; ++j) {
                slopes.push_back((y_in_out[j] - y_in_out[i]) / (x_in[j] - x_in[i]));
            }
        }
    }

    if (slopes.empty()) {
        return handle_error(-20006);
    }

    // 6. High-speed single-pass median for slopes
    size_t n_slopes = slopes.size();
    auto mid_slopes = static_cast<ptrdiff_t>(n_slopes / 2);

    std::nth_element(slopes.begin(), slopes.begin() + mid_slopes, slopes.end());
    double median_slope = slopes[static_cast<size_t>(mid_slopes)];

    if (n_slopes % 2 == 0) {
        auto max_it = std::max_element(slopes.begin(), slopes.begin() + mid_slopes);
        median_slope = (median_slope + *max_it) / 2.0;
    }

    // 7. Calculate intercepts
    std::vector<double> intercepts;
    intercepts.reserve(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        intercepts.push_back(y_in_out[i] - median_slope * x_in[i]);
    }

    // 8. High-speed single-pass median for intercepts
    auto mid_intercepts = static_cast<ptrdiff_t>(n / 2);
    std::nth_element(intercepts.begin(), intercepts.begin() + mid_intercepts, intercepts.end());
    double median_intercept = intercepts[static_cast<size_t>(mid_intercepts)];

    if (n % 2 == 0) {
        auto max_it = std::max_element(intercepts.begin(), intercepts.begin() + mid_intercepts);
        median_intercept = (median_intercept + *max_it) / 2.0;
    }

    // 9. Assign final computed coefficients to output parameters
    *slope = median_slope;
    *intercept = median_intercept;

    // 10. Update size in case arrays were synchronized by min length
    (*y_handle)->dimSize = n;

    // 11. In-place output modification (smooth the line)
    for (int32_t i = 0; i < n; ++i) {
        y_in_out[i] = median_slope * x_in[i] + median_intercept;
    }

    return 0; // Success
}
