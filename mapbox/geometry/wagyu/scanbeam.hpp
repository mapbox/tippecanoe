#pragma once

#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>

#include <algorithm>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
using scanbeam_list = std::vector<T>;

template <typename T>
void insert_sorted_scanbeam(scanbeam_list<T>& scanbeam, T& t) {
    typename scanbeam_list<T>::iterator i = std::lower_bound(scanbeam.begin(), scanbeam.end(), t);
    if (i == scanbeam.end() || t < *i) {
        scanbeam.insert(i, t);
    }
}

template <typename T>
bool pop_from_scanbeam(T& Y, scanbeam_list<T>& scanbeam) {
    if (scanbeam.empty()) {
        return false;
    }

    Y = scanbeam.back();
    scanbeam.pop_back();
    return true;
}

template <typename T>
void setup_scanbeam(local_minimum_list<T>& minima_list, scanbeam_list<T>& scanbeam) {

    scanbeam.reserve(minima_list.size());
    for (auto lm = minima_list.begin(); lm != minima_list.end(); ++lm) {
        scanbeam.push_back(lm->y);
    }
    std::sort(scanbeam.begin(), scanbeam.end());
}
} // namespace wagyu
} // namespace geometry
} // namespace mapbox
