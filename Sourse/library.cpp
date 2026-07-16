#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>
#include <extcode.h>
#include <omp.h>

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
    LVArrayHandle x_handle,
    const int16_t match_lengths,
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

    // 4. Sorting data for further processing
    std::vector<int32_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(), [&](int32_t a, int32_t b) {
        return x_in[a] < x_in[b];
    });

    std::vector<double> x_sorted(n);
    std::vector<double> y_sorted(n);
    for (int32_t i = 0; i < n; ++i) {
        x_sorted[i] = x_in[indices[i]];
        y_sorted[i] = y_in_out[indices[i]];
    }

    // 5 & 6. A variation of the Theil–Sen estimator, the repeated median regression of Siegel (Median of Medians)
    std::vector<double> group_medians;
    group_medians.reserve(n);

    int num_threads = omp_get_max_threads();
    std::vector<std::vector<double>> local_medians(num_threads);

    #pragma omp parallel for schedule(dynamic, 64) default(none) \
        shared(x_sorted, y_sorted, local_medians, n)
    for (int32_t i = 0; i < n - 1; ++i) {
        int tid = omp_get_thread_num();
        double xi = x_sorted[i];
        double yi = y_sorted[i];

        std::vector<double> local_slopes;
        local_slopes.reserve(n - i - 1);

        for (int32_t j = i + 1; j < n; ++j) {
            double dx = x_sorted[j] - xi;
            if (dx > 1e-9) {
                local_slopes.push_back((y_sorted[j] - yi) / dx);
            }
        }

        if (!local_slopes.empty()) {
            size_t mid = local_slopes.size() / 2;
            auto mid_it = local_slopes.begin();
            std::advance(mid_it, static_cast<std::ptrdiff_t>(mid));
            std::nth_element(local_slopes.begin(), mid_it, local_slopes.end());
            double group_median = *mid_it;

            if (local_slopes.size() % 2 == 0) {
                auto max_it = std::max_element(local_slopes.begin(), mid_it);
                group_median = (group_median + *max_it) / 2.0;
            }
            local_medians[tid].push_back(group_median);
        }
    }

    // Combine results from all threads safely
    for (int t = 0; t < num_threads; ++t) {
        group_medians.insert(group_medians.end(), local_medians[t].begin(), local_medians[t].end());
    }

    if (group_medians.empty()) {
        return handle_error(-20006);
    }

    // Final median of the group medians
    size_t n_groups = group_medians.size();
    size_t mid_slopes = n_groups / 2;
    auto mid_slope_it = group_medians.begin();
    std::advance(mid_slope_it, static_cast<std::ptrdiff_t>(mid_slopes));
    std::nth_element(group_medians.begin(), mid_slope_it, group_medians.end());
    double median_slope = *mid_slope_it;

    if (n_groups % 2 == 0) {
        auto max_it = std::max_element(group_medians.begin(), mid_slope_it);
        median_slope = (median_slope + *max_it) / 2.0;
    }

    // 7. Calculate intercepts
    std::vector<double> intercepts;
    intercepts.reserve(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        intercepts.push_back(y_in_out[i] - median_slope * x_in[i]);
    }

    // 8. High-speed single-pass median for intercepts
    size_t mid_intercepts = static_cast<size_t>(n) / 2;
    auto mid_intercept_it = intercepts.begin();
    std::advance(mid_intercept_it, static_cast<std::ptrdiff_t>(mid_intercepts));
    std::nth_element(intercepts.begin(), mid_intercept_it, intercepts.end());
    double median_intercept = *mid_intercept_it;

    if (n % 2 == 0) {
        auto max_it = std::max_element(intercepts.begin(), mid_intercept_it);
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
