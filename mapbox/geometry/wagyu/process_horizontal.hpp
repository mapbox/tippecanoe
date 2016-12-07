#pragma once

#include <mapbox/geometry/line_string.hpp>
#include <mapbox/geometry/point.hpp>
#include <mapbox/geometry/polygon.hpp>

#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/edge.hpp>
#include <mapbox/geometry/wagyu/exceptions.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>
#include <mapbox/geometry/wagyu/util.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
active_bound_list_itr<T> process_horizontal_left_to_right(T scanline_y,
                                                          active_bound_list_itr<T>& horz_bound,
                                                          active_bound_list<T>& active_bounds,
                                                          ring_manager<T>& rings,
                                                          scanbeam_list<T>& scanbeam,
                                                          clip_type cliptype,
                                                          fill_type subject_fill_type,
                                                          fill_type clip_fill_type) {
    auto horizontal_itr_behind = horz_bound;
    bool is_open = (*horz_bound)->winding_delta == 0;
    bool is_maxima_edge = is_maxima(horz_bound, scanline_y);
    auto bound_max_pair = active_bounds.end();
    if (is_maxima_edge) {
        bound_max_pair = get_maxima_pair<T>(horz_bound, active_bounds);
    }

    auto hp_itr = rings.current_hp_itr;
    while (hp_itr != rings.hot_pixels.end() && (hp_itr->y > scanline_y || (hp_itr->y == scanline_y && hp_itr->x < (*horz_bound)->current_edge->bot.x))) {
        ++hp_itr;
    }
    
    auto bnd = std::next(horz_bound);

    while (bnd != active_bounds.end()) {
        // this code block inserts extra coords into horizontal edges (in output
        // polygons) wherever hot pixels touch these horizontal edges. This helps
        //'simplifying' polygons (ie if the Simplify property is set).
        while (hp_itr != rings.hot_pixels.end() && hp_itr->y == scanline_y && hp_itr->x < std::llround((*bnd)->current_x) &&
               hp_itr->x < (*horz_bound)->current_edge->top.x) {
            if ((*horz_bound)->ring && !is_open) {
                add_point_to_ring(*(*horz_bound), *hp_itr, rings);
            }
            ++hp_itr;
        }

        if ((*bnd)->current_x > static_cast<double>((*horz_bound)->current_edge->top.x)) {
            break;
        }

        // Also break if we've got to the end of an intermediate horizontal edge ...
        // nb: Smaller Dx's are to the right of larger Dx's ABOVE the horizontal.
        if (std::llround((*bnd)->current_x) == (*horz_bound)->current_edge->top.x &&
            (*horz_bound)->next_edge != (*horz_bound)->edges.end() &&
            (*horz_bound)->current_edge->dx < (*horz_bound)->next_edge->dx) {
            break;
        }

        // note: may be done multiple times
        if ((*horz_bound)->ring && !is_open) {
            add_point_to_ring(*(*horz_bound),
                              mapbox::geometry::point<T>(std::llround((*bnd)->current_x),
                                                         scanline_y),
                              rings);
        }

        // OK, so far we're still in range of the horizontal Edge  but make sure
        // we're at the last of consec. horizontals when matching with eMaxPair
        if (is_maxima_edge && bnd == bound_max_pair) {
            if ((*horz_bound)->ring) {
                add_local_maximum_point(horz_bound, bound_max_pair,
                                        (*horz_bound)->current_edge->top, rings, active_bounds);
            }
            active_bounds.erase(bound_max_pair);
            auto after_horz = active_bounds.erase(horz_bound);
            if (horizontal_itr_behind != horz_bound) {
                return horizontal_itr_behind;
            } else {
                return after_horz;
            }
        }

        intersect_bounds(horz_bound, bnd,
                         mapbox::geometry::point<T>(std::llround((*bnd)->current_x),
                                                    scanline_y),
                         cliptype, subject_fill_type, clip_fill_type, rings, active_bounds);
        auto next_bnd = std::next(bnd);
        swap_positions_in_ABL(horz_bound, bnd, active_bounds);
        if (current_edge_is_horizontal<T>(bnd) && horizontal_itr_behind == horz_bound) {
            horizontal_itr_behind = bnd;
        }
        bnd = next_bnd;
    } // end while (bnd != active_bounds.end())

    if ((*horz_bound)->ring && !is_open) {
        while (hp_itr != rings.hot_pixels.end() && hp_itr->y == scanline_y &&
               hp_itr->x < std::llround((*horz_bound)->current_edge->top.x)) {
            add_point_to_ring(*(*horz_bound), *hp_itr, rings);
            ++hp_itr;
        }
    }

    if ((*horz_bound)->next_edge != (*horz_bound)->edges.end()) {
        if ((*horz_bound)->ring) {
            add_point_to_ring(*(*horz_bound), (*horz_bound)->current_edge->top, rings);
            next_edge_in_bound(horz_bound, scanbeam);

            if ((*horz_bound)->winding_delta == 0) {
                if (horizontal_itr_behind != horz_bound) {
                    return horizontal_itr_behind;
                } else {
                    return std::next(horz_bound);
                }
            }
        } else {
            next_edge_in_bound(horz_bound, scanbeam);
        }
        if (horizontal_itr_behind != horz_bound) {
            return horizontal_itr_behind;
        } else {
            return std::next(horz_bound);
        }
    } else {
        if ((*horz_bound)->ring) {
            add_point_to_ring(*(*horz_bound), (*horz_bound)->current_edge->top, rings);
        }
        auto after_horz = active_bounds.erase(horz_bound);
        if (horizontal_itr_behind != horz_bound) {
            return horizontal_itr_behind;
        } else {
            return after_horz;
        }
    }
}

template <typename T>
active_bound_list_itr<T> process_horizontal_right_to_left(T scanline_y,
                                                          active_bound_list_itr<T>& horz_bound,
                                                          active_bound_list<T>& active_bounds,
                                                          ring_manager<T>& rings,
                                                          scanbeam_list<T>& scanbeam,
                                                          clip_type cliptype,
                                                          fill_type subject_fill_type,
                                                          fill_type clip_fill_type) {
    bool is_open = (*horz_bound)->winding_delta == 0;
    bool is_maxima_edge = is_maxima(horz_bound, scanline_y);
    auto bound_max_pair = active_bounds.end();
    if (is_maxima_edge) {
        bound_max_pair = get_maxima_pair<T>(horz_bound, active_bounds);
    }
    auto hp_itr_fwd = rings.current_hp_itr;
    while (hp_itr_fwd != rings.hot_pixels.end() && (hp_itr_fwd->y < scanline_y || (hp_itr_fwd->y == scanline_y && hp_itr_fwd->x < (*horz_bound)->current_edge->top.x))) {
        ++hp_itr_fwd;
    }
    auto hp_itr = hot_pixel_rev_itr<T>(hp_itr_fwd);

    auto bnd = active_bound_list_rev_itr<T>(horz_bound);
    while (bnd != active_bounds.rend()) {
        // this code block inserts extra coords into horizontal edges (in output
        // polygons) wherever hot pixels touch these horizontal edges.
        while (hp_itr != rings.hot_pixels.rend() && hp_itr->y == scanline_y && hp_itr->x > std::llround((*bnd)->current_x) &&
               hp_itr->x > (*horz_bound)->current_edge->top.x) {
            if ((*horz_bound)->ring && !is_open) {
                add_point_to_ring(*(*horz_bound), *hp_itr, rings);
            }
            ++hp_itr;
        }

        if ((*bnd)->current_x < static_cast<double>((*horz_bound)->current_edge->top.x)) {
            break;
        }

        // Also break if we've got to the end of an intermediate horizontal edge ...
        // nb: Smaller Dx's are to the right of larger Dx's ABOVE the horizontal.
        if (std::llround((*bnd)->current_x) == (*horz_bound)->current_edge->top.x &&
            (*horz_bound)->next_edge != (*horz_bound)->edges.end() &&
            (*horz_bound)->current_edge->dx < (*horz_bound)->next_edge->dx) {
            break;
        }

        // note: may be done multiple times
        if ((*horz_bound)->ring && !is_open) {
            add_point_to_ring(*(*horz_bound),
                              mapbox::geometry::point<T>(std::llround((*bnd)->current_x),
                                                         scanline_y),
                              rings);
        }
        auto bnd_forward = --(bnd.base());

        // OK, so far we're still in range of the horizontal Edge  but make sure
        // we're at the last of consec. horizontals when matching with eMaxPair
        if (is_maxima_edge && bnd_forward == bound_max_pair) {
            if ((*horz_bound)->ring) {
                add_local_maximum_point(horz_bound, bound_max_pair,
                                        (*horz_bound)->current_edge->top, rings, active_bounds);
            }
            active_bounds.erase(bound_max_pair);
            return active_bounds.erase(horz_bound);
        }

        intersect_bounds(bnd_forward, horz_bound,
                         mapbox::geometry::point<T>(std::llround((*bnd)->current_x),
                                                    scanline_y),
                         cliptype, subject_fill_type, clip_fill_type, rings, active_bounds);
        swap_positions_in_ABL(horz_bound, bnd_forward, active_bounds);
        // Why are we not incrementing the bnd iterator here:
        // It is because reverse iterators point to a `base()` iterator that is a forward
        // iterator that is one ahead of the reverse bound. This will always be the horizontal
        // bound,
        // so what the reverse bound points to will have changed.
    } // end while (bnd != active_bounds.rend())

    if ((*horz_bound)->ring && !is_open) {
        while (hp_itr != rings.hot_pixels.rend() && hp_itr->y == scanline_y && hp_itr->x > (*horz_bound)->current_edge->top.x) {
            add_point_to_ring(*(*horz_bound), *hp_itr, rings);
            ++hp_itr;
        }
    }

    if ((*horz_bound)->next_edge != (*horz_bound)->edges.end()) {
        if ((*horz_bound)->ring) {
            add_point_to_ring(*(*horz_bound), (*horz_bound)->current_edge->top, rings);
            next_edge_in_bound(horz_bound, scanbeam);

            if ((*horz_bound)->winding_delta == 0) {
                return std::next(horz_bound);
            }
        } else {
            next_edge_in_bound(horz_bound, scanbeam);
        }
        return std::next(horz_bound);
    } else {
        if ((*horz_bound)->ring) {
            add_point_to_ring(*(*horz_bound), (*horz_bound)->current_edge->top, rings);
        }
        return active_bounds.erase(horz_bound);
    }
}

template <typename T>
active_bound_list_itr<T> process_horizontal(T scanline_y,
                                            active_bound_list_itr<T>& horz_bound,
                                            active_bound_list<T>& active_bounds,
                                            ring_manager<T>& rings,
                                            scanbeam_list<T>& scanbeam,
                                            clip_type cliptype,
                                            fill_type subject_fill_type,
                                            fill_type clip_fill_type) {
    if ((*horz_bound)->current_edge->bot.x < (*horz_bound)->current_edge->top.x) {
        return process_horizontal_left_to_right(scanline_y, horz_bound, active_bounds, rings,
                                                scanbeam, cliptype, subject_fill_type,
                                                clip_fill_type);
    } else {
        return process_horizontal_right_to_left(scanline_y, horz_bound, active_bounds, rings,
                                                scanbeam, cliptype, subject_fill_type,
                                                clip_fill_type);
    }
}

template <typename T>
void process_horizontals(T scanline_y,
                         active_bound_list<T>& active_bounds,
                         ring_manager<T>& rings,
                         scanbeam_list<T>& scanbeam,
                         clip_type cliptype,
                         fill_type subject_fill_type,
                         fill_type clip_fill_type) {
    for (auto bnd_itr = active_bounds.begin(); bnd_itr != active_bounds.end();) {
        if (current_edge_is_horizontal<T>(bnd_itr)) {
            bnd_itr = process_horizontal(scanline_y, bnd_itr, active_bounds, rings, scanbeam,
                                         cliptype, subject_fill_type, clip_fill_type);
        } else {
            ++bnd_itr;
        }
    }
}
}
}
}
