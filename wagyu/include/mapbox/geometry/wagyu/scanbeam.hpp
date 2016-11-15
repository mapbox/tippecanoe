#pragma once

#include <queue>

#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
using scanbeam_list = std::priority_queue<T>;

template <typename T>
bool pop_from_scanbeam(T& Y, scanbeam_list<T>& scanbeam) {
    if (scanbeam.empty()) {
        return false;
    }
    Y = scanbeam.top();
    scanbeam.pop();
    while (!scanbeam.empty() && Y == scanbeam.top()) {
        scanbeam.pop();
    } // Pop duplicates.
    return true;
}

template <typename T>
void setup_scanbeam(local_minimum_list<T>& minima_list, scanbeam_list<T>& scanbeam) {

    for (auto lm = minima_list.begin(); lm != minima_list.end(); ++lm) {
        scanbeam.push(lm->y);
    }
}
}
}
}
