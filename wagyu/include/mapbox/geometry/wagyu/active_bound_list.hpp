#pragma once

#ifdef DEBUG
#include <iostream>
#endif

#include <mapbox/geometry/wagyu/bound.hpp>
#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/edge.hpp>
#include <mapbox/geometry/wagyu/exceptions.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>
#include <mapbox/geometry/wagyu/local_minimum_util.hpp>
#include <mapbox/geometry/wagyu/ring.hpp>
#include <mapbox/geometry/wagyu/scanbeam.hpp>
#include <mapbox/geometry/wagyu/util.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
using active_bound_list = std::list<bound_ptr<T>>;

template <typename T>
using active_bound_list_itr = typename active_bound_list<T>::iterator;

template <typename T>
using active_bound_list_rev_itr = typename active_bound_list<T>::reverse_iterator;

#ifdef DEBUG

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const active_bound_list<T>& bnds) {
    std::size_t c = 0;
    for (auto const& bnd : bnds) {
        out << "Index: " << c++ << std::endl;
        out << *bnd;
    }
    return out;
}

template <typename T>
std::string output_edges(active_bound_list<T> const& bnds) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (auto const& bnd : bnds) {
        if (first) {
            first = false;
        } else {
            out << ",";
        }
        out << "[[" << bnd->current_edge->bot.x << "," << bnd->current_edge->bot.y << "],[";
        out << bnd->current_edge->top.x << "," << bnd->current_edge->top.y << "]]";
    }
    out << "]";
    return out.str();
}

#endif

template <typename T>
bool is_even_odd_fill_type(bound<T> const& bound,
                           fill_type subject_fill_type,
                           fill_type clip_fill_type) {
    if (bound.poly_type == polygon_type_subject) {
        return subject_fill_type == fill_type_even_odd;
    } else {
        return clip_fill_type == fill_type_even_odd;
    }
}

template <typename T>
bool is_even_odd_alt_fill_type(bound<T> const& bound,
                               fill_type subject_fill_type,
                               fill_type clip_fill_type) {
    if (bound.poly_type == polygon_type_subject) {
        return clip_fill_type == fill_type_even_odd;
    } else {
        return subject_fill_type == fill_type_even_odd;
    }
}

template <typename T>
inline bool bound2_inserts_before_bound1(bound<T> const& bound1, bound<T> const& bound2) {
    if (values_are_equal(bound2.current_x, bound1.current_x)) {
        if (bound2.current_edge->top.y > bound1.current_edge->top.y) {
            return bound2.current_edge->top.x <
                   get_current_x(*(bound1.current_edge), bound2.current_edge->top.y);
        } else {
            return bound1.current_edge->top.x >
                   get_current_x(*(bound2.current_edge), bound1.current_edge->top.y);
        }
    } else {
        return bound2.current_x < bound1.current_x;
    }
}

template <typename T>
active_bound_list_itr<T> insert_bound_into_ABL(bound<T>& bnd, active_bound_list<T>& active_bounds) {
    auto itr = active_bounds.begin();
    while (itr != active_bounds.end() && !bound2_inserts_before_bound1(*(*itr), bnd)) {
        ++itr;
    }
    return active_bounds.insert(itr, &bnd);
}

template <typename T>
active_bound_list_itr<T> insert_bound_into_ABL(bound<T>& bnd,
                                               active_bound_list_itr<T> itr,
                                               active_bound_list<T>& active_bounds) {
    while (itr != active_bounds.end() && !bound2_inserts_before_bound1(*(*itr), bnd)) {
        ++itr;
    }
    return active_bounds.insert(itr, &bnd);
}

template <typename T>
inline bool is_maxima(bound<T>& bnd, T y) {
    return bnd.next_edge == bnd.edges.end() &&
           bnd.current_edge->top.y == y;
}

template <typename T>
inline bool is_maxima(active_bound_list_itr<T>& bnd, T y) {
    return is_maxima(*(*bnd), y);
}

template <typename T>
inline bool is_intermediate(bound<T>& bnd, T y) {
    return bnd.next_edge != bnd.edges.end() &&
           bnd.current_edge->top.y == y;
}

template <typename T>
inline bool is_intermediate(active_bound_list_itr<T>& bnd, T y) {
    return is_intermediate(*(*bnd), y);
}

template <typename T>
inline bool current_edge_is_horizontal(active_bound_list_itr<T>& bnd) {
    return is_horizontal(*((*bnd)->current_edge));
}

template <typename T>
inline bool next_edge_is_horizontal(active_bound_list_itr<T>& bnd) {
    return is_horizontal(*((*bnd)->next_edge));
}

template <typename T>
inline void swap_positions_in_ABL(active_bound_list_itr<T>& bnd1,
                                  active_bound_list_itr<T>& bnd2,
                                  active_bound_list<T>& active_bounds) {
    if (std::next(bnd2) == bnd1) {
        active_bounds.splice(bnd2, active_bounds, bnd1);
    } else { 
        active_bounds.splice(bnd1, active_bounds, bnd2);
    } 
}

template <typename T>
void next_edge_in_bound(active_bound_list_itr<T>& bnd, scanbeam_list<T>& scanbeam) {
    ++((*bnd)->current_edge);
    if ((*bnd)->current_edge != (*bnd)->edges.end()) {
        ++((*bnd)->next_edge);
        (*bnd)->current_x = static_cast<double>((*bnd)->current_edge->bot.x);
        if (!current_edge_is_horizontal<T>(bnd)) {
            scanbeam.push((*bnd)->current_edge->top.y);
        }
    }
}

template <typename T>
active_bound_list_itr<T> get_maxima_pair(active_bound_list_itr<T> bnd,
                                         active_bound_list<T>& active_bounds) {
    auto bnd_itr = active_bounds.begin();
    while (bnd_itr != active_bounds.end()) {
        if (*bnd_itr == (*bnd)->maximum_bound) {
            break;
        }
        ++bnd_itr;
    }
    return bnd_itr;
}

template <typename T>
void set_winding_count(active_bound_list_itr<T>& bnd_itr,
                       active_bound_list<T>& active_bounds,
                       clip_type cliptype,
                       fill_type subject_fill_type,
                       fill_type clip_fill_type) {

    auto rev_bnd_itr = active_bound_list_rev_itr<T>(bnd_itr);
    if (rev_bnd_itr == active_bounds.rend()) {
        if ((*bnd_itr)->winding_delta == 0) {
            fill_type pft = ((*bnd_itr)->poly_type == polygon_type_subject) ? subject_fill_type
                                                                            : clip_fill_type;
            (*bnd_itr)->winding_count = (pft == fill_type_negative ? -1 : 1);
        } else {
            (*bnd_itr)->winding_count = (*bnd_itr)->winding_delta;
        }
        (*bnd_itr)->winding_count2 = 0;
        return;
    }

    // find the edge of the same polytype that immediately preceeds 'edge' in
    // AEL
    while (rev_bnd_itr != active_bounds.rend() &&
           ((*rev_bnd_itr)->poly_type != (*bnd_itr)->poly_type ||
            (*rev_bnd_itr)->winding_delta == 0)) {
        ++rev_bnd_itr;
    }
    if (rev_bnd_itr == active_bounds.rend()) {
        if ((*bnd_itr)->winding_delta == 0) {
            fill_type pft = ((*bnd_itr)->poly_type == polygon_type_subject) ? subject_fill_type
                                                                            : clip_fill_type;
            (*bnd_itr)->winding_count = (pft == fill_type_negative ? -1 : 1);
        } else {
            (*bnd_itr)->winding_count = (*bnd_itr)->winding_delta;
        }
        (*bnd_itr)->winding_count2 = 0;
    } else if ((*bnd_itr)->winding_delta == 0 && cliptype != clip_type_union) {
        (*bnd_itr)->winding_count = 1;
        (*bnd_itr)->winding_count2 = (*rev_bnd_itr)->winding_count2;
    } else if (is_even_odd_fill_type(*(*bnd_itr), subject_fill_type, clip_fill_type)) {
        // EvenOdd filling ...
        if ((*bnd_itr)->winding_delta == 0) {
            // are we inside a subj polygon ...
            bool inside = true;
            auto rev2 = std::next(rev_bnd_itr);
            while (rev2 != active_bounds.rend()) {
                if ((*rev2)->poly_type == (*rev_bnd_itr)->poly_type &&
                    (*rev2)->winding_delta != 0) {
                    inside = !inside;
                }
                ++rev2;
            }
            (*bnd_itr)->winding_count = (inside ? 0 : 1);
        } else {
            (*bnd_itr)->winding_count = (*bnd_itr)->winding_delta;
        }
        (*bnd_itr)->winding_count2 = (*rev_bnd_itr)->winding_count2;
    } else {
        // nonZero, Positive or Negative filling ...
        if ((*rev_bnd_itr)->winding_count * (*rev_bnd_itr)->winding_delta < 0) {
            // prev edge is 'decreasing' WindCount (WC) toward zero
            // so we're outside the previous polygon ...
            if (std::abs((*rev_bnd_itr)->winding_count) > 1) {
                // outside prev poly but still inside another.
                // when reversing direction of prev poly use the same WC
                if ((*rev_bnd_itr)->winding_delta * (*bnd_itr)->winding_delta < 0) {
                    (*bnd_itr)->winding_count = (*rev_bnd_itr)->winding_count;
                } else {
                    // otherwise continue to 'decrease' WC ...
                    (*bnd_itr)->winding_count =
                        (*rev_bnd_itr)->winding_count + (*bnd_itr)->winding_delta;
                }
            } else {
                // now outside all polys of same polytype so set own WC ...
                (*bnd_itr)->winding_count =
                    ((*bnd_itr)->winding_delta == 0 ? 1 : (*bnd_itr)->winding_delta);
            }
        } else {
            // prev edge is 'increasing' WindCount (WC) away from zero
            // so we're inside the previous polygon ...
            if ((*bnd_itr)->winding_delta == 0) {
                (*bnd_itr)->winding_count =
                    ((*rev_bnd_itr)->winding_count < 0 ? (*rev_bnd_itr)->winding_count - 1
                                                       : (*rev_bnd_itr)->winding_count + 1);
            } else if ((*rev_bnd_itr)->winding_delta * (*bnd_itr)->winding_delta < 0) {
                // if wind direction is reversing prev then use same WC
                (*bnd_itr)->winding_count = (*rev_bnd_itr)->winding_count;
            } else {
                // otherwise add to WC ...
                (*bnd_itr)->winding_count =
                    (*rev_bnd_itr)->winding_count + (*bnd_itr)->winding_delta;
            }
        }
        (*bnd_itr)->winding_count2 = (*rev_bnd_itr)->winding_count2;
    }

    // update winding_count2 ...
    auto bnd_itr_forward = rev_bnd_itr.base();
    if (is_even_odd_alt_fill_type(*(*bnd_itr), subject_fill_type, clip_fill_type)) {
        // EvenOdd filling ...
        while (bnd_itr_forward != bnd_itr) {
            if ((*bnd_itr_forward)->winding_delta != 0) {
                (*bnd_itr)->winding_count2 = ((*bnd_itr)->winding_count2 == 0 ? 1 : 0);
            }
            ++bnd_itr_forward;
        }
    } else {
        // nonZero, Positive or Negative filling ...
        while (bnd_itr_forward != bnd_itr) {
            (*bnd_itr)->winding_count2 += (*bnd_itr_forward)->winding_delta;
            ++bnd_itr_forward;
        }
    }
}

template <typename T>
bool is_contributing(bound<T> const& bnd,
                     clip_type cliptype,
                     fill_type subject_fill_type,
                     fill_type clip_fill_type) {
    fill_type pft = subject_fill_type;
    fill_type pft2 = clip_fill_type;
    if (bnd.poly_type != polygon_type_subject) {
        pft = clip_fill_type;
        pft2 = subject_fill_type;
    }

    switch (pft) {
    case fill_type_even_odd:
        // return false if a subj line has been flagged as inside a subj
        // polygon
        if (bnd.winding_delta == 0 && bnd.winding_count != 1) {
            return false;
        }
        break;
    case fill_type_non_zero:
        if (std::abs(bnd.winding_count) != 1) {
            return false;
        }
        break;
    case fill_type_positive:
        if (bnd.winding_count != 1) {
            return false;
        }
        break;
    case fill_type_negative:
    default:
        if (bnd.winding_count != -1) {
            return false;
        }
    }

    switch (cliptype) {
    case clip_type_intersection:
        switch (pft2) {
        case fill_type_even_odd:
        case fill_type_non_zero:
            return (bnd.winding_count2 != 0);
        case fill_type_positive:
            return (bnd.winding_count2 > 0);
        case fill_type_negative:
        default:
            return (bnd.winding_count2 < 0);
        }
        break;
    case clip_type_union:
        switch (pft2) {
        case fill_type_even_odd:
        case fill_type_non_zero:
            return (bnd.winding_count2 == 0);
        case fill_type_positive:
            return (bnd.winding_count2 <= 0);
        case fill_type_negative:
        default:
            return (bnd.winding_count2 >= 0);
        }
        break;
    case clip_type_difference:
        if (bnd.poly_type == polygon_type_subject) {
            switch (pft2) {
            case fill_type_even_odd:
            case fill_type_non_zero:
                return (bnd.winding_count2 == 0);
            case fill_type_positive:
                return (bnd.winding_count2 <= 0);
            case fill_type_negative:
            default:
                return (bnd.winding_count2 >= 0);
            }
        } else {
            switch (pft2) {
            case fill_type_even_odd:
            case fill_type_non_zero:
                return (bnd.winding_count2 != 0);
            case fill_type_positive:
                return (bnd.winding_count2 > 0);
            case fill_type_negative:
            default:
                return (bnd.winding_count2 < 0);
            }
        }
        break;
    case clip_type_x_or:
        if (bnd.winding_delta == 0) {
            // XOr always contributing unless open
            switch (pft2) {
            case fill_type_even_odd:
            case fill_type_non_zero:
                return (bnd.winding_count2 == 0);
            case fill_type_positive:
                return (bnd.winding_count2 <= 0);
            case fill_type_negative:
            default:
                return (bnd.winding_count2 >= 0);
            }
        } else {
            return true;
        }
        break;
    default:
        return true;
    }
}

template <typename T>
void insert_lm_only_one_bound(bound<T>& bnd,
                              active_bound_list<T>& active_bounds,
                              ring_manager<T>& rings,
                              scanbeam_list<T>& scanbeam,
                              clip_type cliptype,
                              fill_type subject_fill_type,
                              fill_type clip_fill_type) {
    auto abl_itr = insert_bound_into_ABL(bnd, active_bounds);
    set_winding_count(abl_itr, active_bounds, cliptype, subject_fill_type, clip_fill_type);
    if (is_contributing(bnd, cliptype, subject_fill_type, clip_fill_type)) {
        add_first_point(abl_itr, active_bounds, (*abl_itr)->current_edge->bot, rings);
    }
    if (!current_edge_is_horizontal<T>(abl_itr)) {
        scanbeam.push((*abl_itr)->current_edge->top.y);
    }
}

template <typename T>
void insert_lm_left_and_right_bound(bound<T>& left_bound,
                                    bound<T>& right_bound,
                                    active_bound_list<T>& active_bounds,
                                    ring_manager<T>& rings,
                                    scanbeam_list<T>& scanbeam,
                                    clip_type cliptype,
                                    fill_type subject_fill_type,
                                    fill_type clip_fill_type) {

    // Both left and right bound
    auto lb_abl_itr = insert_bound_into_ABL(left_bound, active_bounds);
    auto rb_abl_itr = insert_bound_into_ABL(right_bound, lb_abl_itr, active_bounds);
    set_winding_count(lb_abl_itr, active_bounds, cliptype, subject_fill_type, clip_fill_type);
    (*rb_abl_itr)->winding_count = (*lb_abl_itr)->winding_count;
    (*rb_abl_itr)->winding_count2 = (*lb_abl_itr)->winding_count2;
    if (is_contributing(left_bound, cliptype, subject_fill_type, clip_fill_type)) {
        add_local_minimum_point(lb_abl_itr, rb_abl_itr, active_bounds,
                                (*lb_abl_itr)->current_edge->bot, rings);
    }

    // Add top of edges to scanbeam
    scanbeam.push((*lb_abl_itr)->current_edge->top.y);

    if (!current_edge_is_horizontal<T>(rb_abl_itr)) {
        scanbeam.push((*rb_abl_itr)->current_edge->top.y);
    }
    auto abl_itr = std::next(lb_abl_itr);
    while (abl_itr != rb_abl_itr && abl_itr != active_bounds.end()) {
        // We call intersect_bounds here, but we do not swap positions in the ABL
        // this is the logic that was copied from angus, but it might be correct
        // to swap the positions in the ABL following this or at least move
        // lb and rb to be next to each other in the ABL.
        intersect_bounds(rb_abl_itr, abl_itr, (*lb_abl_itr)->current_edge->bot, cliptype,
                         subject_fill_type, clip_fill_type, rings, active_bounds);
        ++abl_itr;
    }
}

template <typename T>
void insert_local_minima_into_ABL(T const bot_y,
                                  local_minimum_ptr_list<T> const& minima_sorted,
                                  local_minimum_ptr_list_itr<T>& current_lm,
                                  active_bound_list<T>& active_bounds,
                                  ring_manager<T>& rings,
                                  scanbeam_list<T>& scanbeam,
                                  clip_type cliptype,
                                  fill_type subject_fill_type,
                                  fill_type clip_fill_type) {
    while (current_lm != minima_sorted.end() && bot_y == (*current_lm)->y) {
        initialize_lm<T>(current_lm);
        auto& left_bound = (*current_lm)->left_bound;
        auto& right_bound = (*current_lm)->right_bound;
        if (left_bound.edges.empty() && !right_bound.edges.empty()) {
            insert_lm_only_one_bound(right_bound, active_bounds, rings, scanbeam, cliptype,
                                     subject_fill_type, clip_fill_type);
        } else if (right_bound.edges.empty() && !left_bound.edges.empty()) {
            insert_lm_only_one_bound(left_bound, active_bounds, rings, scanbeam, cliptype,
                                     subject_fill_type, clip_fill_type);
        } else {
            insert_lm_left_and_right_bound(left_bound, right_bound, active_bounds, rings, scanbeam,
                                           cliptype, subject_fill_type, clip_fill_type);
        }
        ++current_lm;
    }
}

template <typename T>
void insert_horizontal_local_minima_into_ABL(T const top_y,
                                             local_minimum_ptr_list<T> const& minima_sorted,
                                             local_minimum_ptr_list_itr<T>& current_lm,
                                             active_bound_list<T>& active_bounds,
                                             ring_manager<T>& rings,
                                             scanbeam_list<T>& scanbeam,
                                             clip_type cliptype,
                                             fill_type subject_fill_type,
                                             fill_type clip_fill_type) {
    while (current_lm != minima_sorted.end() && top_y == (*current_lm)->y &&
           (*current_lm)->minimum_has_horizontal) {
        initialize_lm<T>(current_lm);
        auto& left_bound = (*current_lm)->left_bound;
        auto& right_bound = (*current_lm)->right_bound;
        if (left_bound.edges.empty() && !right_bound.edges.empty()) {
            insert_lm_only_one_bound(right_bound, active_bounds, rings, scanbeam, cliptype,
                                     subject_fill_type, clip_fill_type);
        } else if (right_bound.edges.empty() && !left_bound.edges.empty()) {
            throw clipper_exception(
                "There should only be horizontal local minimum on right bounds!");
        } else {
            insert_lm_left_and_right_bound(left_bound, right_bound, active_bounds, rings, scanbeam,
                                           cliptype, subject_fill_type, clip_fill_type);
        }
        ++current_lm;
    }
}
}
}
}
