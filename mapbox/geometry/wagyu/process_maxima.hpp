#pragma once

#include <mapbox/geometry/wagyu/active_bound_list.hpp>
#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/edge.hpp>
#include <mapbox/geometry/wagyu/intersect_util.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>
#include <mapbox/geometry/wagyu/local_minimum_util.hpp>
#include <mapbox/geometry/wagyu/process_horizontal.hpp>
#include <mapbox/geometry/wagyu/ring.hpp>
#include <mapbox/geometry/wagyu/ring_util.hpp>
#include <mapbox/geometry/wagyu/topology_correction.hpp>
#include <mapbox/geometry/wagyu/util.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
active_bound_list_itr<T> do_maxima(active_bound_list_itr<T>& bnd,
                                   active_bound_list_itr<T>& bndMaxPair,
                                   clip_type cliptype,
                                   fill_type subject_fill_type,
                                   fill_type clip_fill_type,
                                   ring_manager<T>& rings,
                                   active_bound_list<T>& active_bounds) {
    if (bndMaxPair == active_bounds.end()) {
        if ((*bnd)->ring) {
            add_point_to_ring(*(*bnd), (*bnd)->current_edge->top, rings);
        }
        return active_bounds.erase(bnd);
    }
    auto bnd_next = std::next(bnd);
    auto return_bnd = bnd_next;
    bool skipped = false;
    while (bnd_next != active_bounds.end() && bnd_next != bndMaxPair) {
        skipped = true;
        intersect_bounds(bnd, bnd_next, (*bnd)->current_edge->top, cliptype, subject_fill_type,
                         clip_fill_type, rings, active_bounds);
        swap_positions_in_ABL(bnd, bnd_next, active_bounds);
        bnd_next = std::next(bnd);
    }

    if (!(*bnd)->ring && !(*bndMaxPair)->ring) {
        active_bounds.erase(bndMaxPair);
    } else if ((*bnd)->ring && (*bndMaxPair)->ring) {
        add_local_maximum_point(bnd, bndMaxPair, (*bnd)->current_edge->top, rings, active_bounds);
        active_bounds.erase(bndMaxPair);
    } else {
        throw std::runtime_error("DoMaxima error");
    }
    auto prev_itr = active_bounds.erase(bnd);
    if (skipped) {
        return return_bnd;
    } else {
        return prev_itr;
    }
}

template <typename T>
void process_edges_at_top_of_scanbeam(T top_y,
                                      active_bound_list<T>& active_bounds,
                                      scanbeam_list<T>& scanbeam,
                                      local_minimum_ptr_list<T> const& minima_sorted,
                                      local_minimum_ptr_list_itr<T>& current_lm,
                                      ring_manager<T>& rings,
                                      clip_type cliptype,
                                      fill_type subject_fill_type,
                                      fill_type clip_fill_type) {

    for (auto bnd = active_bounds.begin(); bnd != active_bounds.end();) {
        // 1. Process maxima, treating them as if they are "bent" horizontal edges,
        // but exclude maxima with horizontal edges.

        bool is_maxima_edge = is_maxima(bnd, top_y);

        if (is_maxima_edge) {
            auto bnd_max_pair = get_maxima_pair(bnd, active_bounds);
            is_maxima_edge = ((bnd_max_pair == active_bounds.end() ||
                               !current_edge_is_horizontal<T>(bnd_max_pair)) &&
                              is_maxima(bnd_max_pair, top_y));
            if (is_maxima_edge) {
                bnd = do_maxima(bnd, bnd_max_pair, cliptype, subject_fill_type, clip_fill_type,
                                rings, active_bounds);
                continue;
            }
        }

        // 2. Promote horizontal edges.
        if (is_intermediate(bnd, top_y) && next_edge_is_horizontal<T>(bnd)) {
            if ((*bnd)->ring) {
                insert_hot_pixels_in_path(*(*bnd), (*bnd)->current_edge->top, rings, false);
            }
            next_edge_in_bound(bnd, scanbeam);
            if ((*bnd)->ring) {
                add_point_to_ring(*(*bnd), (*bnd)->current_edge->bot, rings);
            }
        } else {
            (*bnd)->current_x = get_current_x(*((*bnd)->current_edge), top_y);
        }

        ++bnd;
    }

    insert_horizontal_local_minima_into_ABL(top_y, minima_sorted, current_lm, active_bounds, rings,
                                            scanbeam, cliptype, subject_fill_type, clip_fill_type);

    process_horizontals(top_y, active_bounds, rings, scanbeam, cliptype, subject_fill_type,
                        clip_fill_type);

    // 4. Promote intermediate vertices

    for (auto bnd = active_bounds.begin(); bnd != active_bounds.end(); ++bnd) {
        if (is_intermediate(bnd, top_y)) {
            if ((*bnd)->ring) {
                add_point_to_ring(*(*bnd), (*bnd)->current_edge->top, rings);
                insert_hot_pixels_in_path(*(*bnd), (*bnd)->current_edge->top, rings, false);
            }
            next_edge_in_bound(bnd, scanbeam);
        }
    }
}
}
}
}
