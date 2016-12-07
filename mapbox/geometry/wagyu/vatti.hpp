#pragma once

#include <algorithm>
#include <set>

#include <mapbox/geometry/wagyu/active_bound_list.hpp>
#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/exceptions.hpp>
#include <mapbox/geometry/wagyu/intersect_util.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>
#include <mapbox/geometry/wagyu/local_minimum_util.hpp>
#include <mapbox/geometry/wagyu/process_horizontal.hpp>
#include <mapbox/geometry/wagyu/process_maxima.hpp>
#include <mapbox/geometry/wagyu/ring.hpp>
#include <mapbox/geometry/wagyu/ring_util.hpp>
#include <mapbox/geometry/wagyu/util.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
bool execute_vatti(local_minimum_list<T>& minima_list,
                   ring_manager<T>& rings,
                   clip_type cliptype,
                   fill_type subject_fill_type,
                   fill_type clip_fill_type) {

    if (minima_list.empty()) {
        return false;
    }
    
    active_bound_list<T> active_bounds;
    scanbeam_list<T> scanbeam;
    T scanline_y = std::numeric_limits<T>::max();

    local_minimum_ptr_list<T> minima_sorted;
    minima_sorted.reserve(minima_list.size());
    for (auto& lm : minima_list) {
        minima_sorted.push_back(&lm);
    }
    std::stable_sort(minima_sorted.begin(), minima_sorted.end(), local_minimum_sorter<T>());
    local_minimum_ptr_list_itr<T> current_lm = minima_sorted.begin();
    // std::clog << output_all_edges(minima_sorted) << std::endl;

    setup_scanbeam(minima_list, scanbeam);
    rings.current_hp_itr = rings.hot_pixels.begin();

    while (pop_from_scanbeam(scanline_y, scanbeam) || current_lm != minima_sorted.end()) {

        process_intersections(scanline_y, active_bounds, cliptype,
                              subject_fill_type, clip_fill_type, rings);

        update_current_hp_itr(scanline_y, rings);

        // First we process bounds that has already been added to the active bound list --
        // if the active bound list is empty local minima that are at this scanline_y and
        // have a horizontal edge at the local minima will be processed
        process_edges_at_top_of_scanbeam(scanline_y, active_bounds, scanbeam, minima_sorted,
                                         current_lm, rings, cliptype, subject_fill_type,
                                         clip_fill_type);

        // Next we will add local minima bounds to the active bounds list that are on the local
        // minima queue at
        // this current scanline_y
        insert_local_minima_into_ABL(scanline_y, minima_sorted, current_lm, active_bounds,
                                     rings, scanbeam, cliptype, subject_fill_type,
                                     clip_fill_type);

    }
    // std::clog << rings.rings << std::endl;
    // std::clog << output_as_polygon(rings.all_rings[0]);
    return true;
}
}
}
}
