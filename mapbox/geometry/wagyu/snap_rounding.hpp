#pragma once

#include <mapbox/geometry/wagyu/active_bound_list.hpp>
#include <mapbox/geometry/wagyu/edge.hpp>
#include <mapbox/geometry/wagyu/bound.hpp>
#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/intersect.hpp>
#include <mapbox/geometry/wagyu/intersect_util.hpp>
#include <mapbox/geometry/wagyu/ring.hpp>
#include <mapbox/geometry/wagyu/ring_util.hpp>
#include <mapbox/geometry/wagyu/util.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
void process_hot_pixel_intersections(T top_y,
                                     active_bound_list<T>& active_bounds,
                                     ring_manager<T>& rings) {
    if (active_bounds.empty()) {
        return;
    }

    update_current_x(active_bounds, top_y);
    // bubblesort ...
    bool isModified;
    do {
        isModified = false;
        auto bnd = active_bounds.begin();
        auto bnd_next = std::next(bnd);
        while (bnd_next != active_bounds.end()) {
            if ((*bnd)->current_x > (*bnd_next)->current_x && !slopes_equal(*(*bnd)->current_edge, *(*bnd_next)->current_edge)) {
                mapbox::geometry::point<double> pt;
                if (!get_edge_intersection<T, double>(*((*bnd)->current_edge),
                                                     *((*bnd_next)->current_edge), pt)) {
                    throw std::runtime_error("Edges do not intersect!");
                }
                add_to_hot_pixels(round_point<T>(pt), rings);
                swap_positions_in_ABL(bnd, bnd_next, active_bounds);
                bnd_next = std::next(bnd);
                isModified = true;
            } else {
                bnd = bnd_next;
                ++bnd_next;
            }
        }
    } while (isModified);
}

template <typename T>
void process_hot_pixel_edges_at_top_of_scanbeam(T top_y,
                                                scanbeam_list<T>& scanbeam,
                                                active_bound_list<T>& active_bounds,
                                                ring_manager<T>& rings) {
    for (auto bnd = active_bounds.begin(); bnd != active_bounds.end();) {
        auto bnd_2 = std::next(bnd);
        while ((*bnd)->current_edge != (*bnd)->edges.end() && (*bnd)->current_edge->top.y == top_y) {
            add_to_hot_pixels((*bnd)->current_edge->top, rings);
            if (current_edge_is_horizontal<T>(bnd)) {
                (*bnd)->current_x = static_cast<double>((*bnd)->current_edge->top.x);
                if ((*bnd)->current_edge->bot.x < (*bnd)->current_edge->top.x) {
                    // left to right
                    auto bnd_next = std::next(bnd);
                    while (bnd_next != active_bounds.end() && (*bnd_next)->current_x < (*bnd)->current_x) {
                        if (std::llround((*bnd_next)->current_edge->top.y) != top_y && std::llround((*bnd_next)->current_edge->bot.y) != top_y) {
                            mapbox::geometry::point<T> pt(std::llround((*bnd_next)->current_x), top_y);
                            add_to_hot_pixels(pt, rings);
                        }
                        swap_positions_in_ABL(bnd, bnd_next, active_bounds);
                        bnd_next = std::next(bnd);
                    }
                } else {
                    // right to left
                    if (bnd != active_bounds.begin()) {
                        auto bnd_prev = std::prev(bnd);
                        while (bnd != active_bounds.begin() && (*bnd_prev)->current_x > (*bnd)->current_x) {
                            if (std::llround((*bnd_prev)->current_edge->top.y) != top_y && std::llround((*bnd_prev)->current_edge->bot.y) != top_y) {
                                mapbox::geometry::point<T> pt(std::llround((*bnd_prev)->current_x), top_y);
                                add_to_hot_pixels(pt, rings);
                            }
                            swap_positions_in_ABL(bnd, bnd_prev, active_bounds);
                            bnd_prev = std::prev(bnd);
                        }
                    }
                }
            }
            next_edge_in_bound(bnd, scanbeam);
        }
        if ((*bnd)->current_edge == (*bnd)->edges.end()) {
            active_bounds.erase(bnd);
        }
        bnd = bnd_2;
    }
}

template <typename T>
void insert_local_minima_into_ABL_hot_pixel(T top_y,
                                            local_minimum_ptr_list<T> & minima_sorted,
                                            local_minimum_ptr_list_itr<T> & lm,
                                            active_bound_list<T>& active_bounds,
                                            ring_manager<T>& rings,
                                            scanbeam_list<T>& scanbeam) {
    while (lm != minima_sorted.end() && (*lm)->y == top_y) {
        if ((*lm)->left_bound.edges.empty() || (*lm)->right_bound.edges.empty()) {
            ++lm;
            continue;
        }

        add_to_hot_pixels((*lm)->left_bound.edges.front().bot, rings); 
        auto& left_bound = (*lm)->left_bound;
        left_bound.current_edge = left_bound.edges.begin();
        left_bound.current_x = static_cast<double>(left_bound.current_edge->bot.x);
        auto lb_abl_itr = insert_bound_into_ABL(left_bound, active_bounds);
        if (!current_edge_is_horizontal<T>(lb_abl_itr)) {
            scanbeam.push((*lb_abl_itr)->current_edge->top.y);
        }
        auto& right_bound = (*lm)->right_bound;
        right_bound.current_edge = right_bound.edges.begin();
        right_bound.current_x = static_cast<double>(right_bound.current_edge->bot.x);
        auto rb_abl_itr = insert_bound_into_ABL(right_bound, lb_abl_itr, active_bounds);
        if (!current_edge_is_horizontal<T>(rb_abl_itr)) {
            scanbeam.push((*rb_abl_itr)->current_edge->top.y);
        }
        ++lm;
    }
}

template <typename T>
void build_hot_pixels(local_minimum_list<T>& minima_list,
                      ring_manager<T>& rings) {
    if (minima_list.empty()) {
        return;
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

    setup_scanbeam(minima_list, scanbeam);

    // Estimate size for reserving hot pixels
    std::size_t reserve = 0;
    for (auto & lm : minima_list) {
        reserve += lm.left_bound.edges.size() + 2;
        reserve += lm.right_bound.edges.size() + 2;
    }
    rings.hot_pixels.reserve(reserve);

    while (pop_from_scanbeam(scanline_y, scanbeam) || current_lm != minima_sorted.end()) {

        process_hot_pixel_intersections(scanline_y, active_bounds, rings);

        insert_local_minima_into_ABL_hot_pixel(scanline_y, minima_sorted, current_lm, active_bounds,
                                     rings, scanbeam);
        
        process_hot_pixel_edges_at_top_of_scanbeam(scanline_y, scanbeam, active_bounds, rings);
        
    }
    preallocate_point_memory(rings, rings.hot_pixels.size());
    sort_hot_pixels(rings);
}

}
}
}
