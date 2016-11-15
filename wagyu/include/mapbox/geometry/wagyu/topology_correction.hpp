#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/ring.hpp>
#include <mapbox/geometry/wagyu/ring_util.hpp>

#ifdef DEBUG
#include <iostream>
#endif

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
struct point_ptr_pair {
    point_ptr<T> op1;
    point_ptr<T> op2;

    constexpr point_ptr_pair(point_ptr<T> o1, point_ptr<T> o2)
     : op1(o1),
       op2(o2) {}

    point_ptr_pair(point_ptr_pair<T> const& p) = default;

    point_ptr_pair(point_ptr_pair<T> && p)
        : op1(std::move(p.op1)),
          op2(std::move(p.op2)) {}

    point_ptr_pair& operator=(point_ptr_pair<T> && p) {
        op1 = std::move(p.op1);
        op2 = std::move(p.op2);
        return *this;
    }

};

#ifdef DEBUG

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>&
operator<<(std::basic_ostream<charT, traits>& out,
           const std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring) {

    out << " BEGIN CONNECTIONS: " << std::endl;
    for (auto& r : dupe_ring) {
        out << "  Ring: ";
        if (r.second.op1->ring) {
            out << r.second.op1->ring->ring_index;
        } else {
            out << "---";
        }
        out << " to ";
        if (r.second.op2->ring) {
            out << r.second.op2->ring->ring_index;
        } else {
            out << "---";
        }
        out << "  ( at " << r.second.op1->x << ", " << r.second.op1->y << " )";
        out << "  Ring1 ( ";
        if (r.second.op1->ring) {
            out << "area: " << r.second.op1->ring->area << " parent: ";
            if (r.second.op1->ring->parent) {
                out << r.second.op1->ring->parent->ring_index;
            } else {
                out << "---";
            }
        } else {
            out << "---";
        }
        out << " )";
        out << "  Ring2 ( ";
        if (r.second.op2->ring) {
            out << "area: " << r.second.op2->ring->area << " parent: ";
            if (r.second.op2->ring->parent) {
                out << r.second.op2->ring->parent->ring_index;
            } else {
                out << "---";
            }
        } else {
            out << "---";
        }
        out << " )";
        out << std::endl;
    }
    out << " END CONNECTIONS: " << std::endl;
    return out;
}

#endif

template <typename T>
bool find_intersect_loop(std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                         std::list<std::pair<ring_ptr<T>, point_ptr_pair<T>>>& iList,
                         ring_ptr<T> ring_parent,
                         ring_ptr<T> ring_origin,
                         ring_ptr<T> ring_search,
                         std::set<ring_ptr<T>>& visited,
                         point_ptr<T> orig_pt,
                         point_ptr<T> prev_pt,
                         ring_manager<T>& rings) {
    {
        auto range = dupe_ring.equal_range(ring_search);
        // Check for direct connection
        for (auto& it = range.first; it != range.second;) {
            ring_ptr<T> it_ring1 = it->second.op1->ring;
            ring_ptr<T> it_ring2 = it->second.op2->ring;
            if (!it_ring1 || !it_ring2 || it_ring1 != ring_search ||
                (!ring_is_hole(it_ring1) && !ring_is_hole(it_ring2))) {
                it = dupe_ring.erase(it);
                continue;
            }
            if (it_ring2 == ring_origin &&
                (ring_parent == it_ring2 || ring_parent == it_ring2->parent) &&
                *prev_pt != *it->second.op2 && *orig_pt != *it->second.op2) {
                iList.emplace_front(ring_search, it->second);
                return true;
            }
            ++it;
        }
    }
    auto range = dupe_ring.equal_range(ring_search);
    visited.insert(ring_search);
    // Check for connection through chain of other intersections
    for (auto& it = range.first;
         it != range.second && it != dupe_ring.end() && it->first == ring_search; ++it) {
        ring_ptr<T> it_ring = it->second.op2->ring;
        if (visited.count(it_ring) > 0 || it_ring == nullptr ||
            (ring_parent != it_ring && ring_parent != it_ring->parent) ||
            value_is_zero(area(it_ring)) || *prev_pt == *it->second.op2) {
            continue;
        }
        if (find_intersect_loop(dupe_ring, iList, ring_parent, ring_origin, it_ring, visited,
                                orig_pt, it->second.op2, rings)) {
            iList.emplace_front(ring_search, it->second);
            return true;
        }
    }
    return false;
}

template <typename T>
void remove_spikes(point_ptr<T>& pt) {
    ring_ptr<T> r = pt->ring;
    while (true) {
        if (pt->next == pt) {
            r->points = nullptr;
            r->area = std::numeric_limits<double>::quiet_NaN();
            pt->ring = nullptr;
            pt = nullptr;
            break;
        } else if (*(pt) == *(pt->next)) {
            point_ptr<T> old_next = pt->next;
            old_next->next->prev = pt;
            pt->next = old_next->next;
            old_next->next = old_next;
            old_next->prev = old_next;
            if (r->points == old_next) {
                r->points = pt;
            }
            r->area = std::numeric_limits<double>::quiet_NaN();
            old_next->ring = nullptr;
        } else if (*(pt) == *(pt->prev)) {
            point_ptr<T> old_prev = pt->prev;
            old_prev->prev->next = pt;
            pt->prev = old_prev->prev;
            old_prev->next = old_prev;
            old_prev->prev = old_prev;
            if (r->points == old_prev) {
                r->points = pt;
            }
            r->area = std::numeric_limits<double>::quiet_NaN();
            old_prev->ring = nullptr;
        } else if (*(pt->next) == *(pt->prev)) {
            point_ptr<T> next = pt->next;
            point_ptr<T> prev = pt->prev;
            next->prev = prev;
            prev->next = next;
            if (r->points == pt) {
                r->points = prev;
            }
            r->area = std::numeric_limits<double>::quiet_NaN();
            pt->ring = nullptr;
            pt->next = pt;
            pt->prev = pt;
            pt = next;
        } else {
            break;
        }
    }
}

template <typename T>
void fixup_children(ring_ptr<T> old_ring, ring_ptr<T> new_ring) {
    // Tests if any of the children from the old ring are now children of the new ring
    assert(old_ring != new_ring);
    for (auto r = old_ring->children.begin(); r != old_ring->children.end();) {
        assert((*r)->points);
        assert((*r) != old_ring);
        if ((*r) != new_ring && !ring1_right_of_ring2(new_ring, (*r)) &&
            poly2_contains_poly1((*r)->points, new_ring->points)) {
            (*r)->parent = new_ring;
            new_ring->children.push_back((*r));
            r = old_ring->children.erase(r);
        } else {
            ++r;
        }
    }
}

template <typename T>
bool fix_intersects(std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                    point_ptr<T> op_j,
                    point_ptr<T> op_k,
                    ring_manager<T>& rings,
                    mapbox::geometry::point<T>& rewind_point) {
    ring_ptr<T> ring_j = op_j->ring;
    ring_ptr<T> ring_k = op_k->ring;
    if (ring_j == ring_k) {
        return false;
    }

    if (!ring_is_hole(ring_j) && !ring_is_hole(ring_k)) {
        // Both are not holes, return nothing to do.
        return false;
    }

    ring_ptr<T> ring_origin;
    ring_ptr<T> ring_search;
    ring_ptr<T> ring_parent;
    point_ptr<T> op_origin_1;
    point_ptr<T> op_origin_2;
    if (!ring_is_hole(ring_j)) {
        ring_origin = ring_j;
        ring_parent = ring_origin;
        ring_search = ring_k;
        op_origin_1 = op_j;
        op_origin_2 = op_k;
    } else if (!ring_is_hole(ring_k)) {
        ring_origin = ring_k;
        ring_parent = ring_origin;
        ring_search = ring_j;
        op_origin_1 = op_k;
        op_origin_2 = op_j;

    } else {
        // both are holes
        // Order doesn't matter
        ring_origin = ring_j;
        ring_parent = ring_origin->parent;
        ring_search = ring_k;
        op_origin_1 = op_j;
        op_origin_2 = op_k;
    }
    if (ring_parent != ring_search->parent) {
        // The two holes do not have the same parent, do not add them
        // simply return!
        if (ring_parent->parent != ring_search &&
            poly2_contains_poly1(ring_search->points, ring_parent->points) && 
            !ring1_right_of_ring2(ring_search, ring_parent)) {
            ring_ptr<T> old_parent = ring_search->parent;
            ring_search->parent = ring_parent;
            old_parent->children.remove(ring_search);
            ring_parent->children.push_back(ring_search);
        } else {
            return false;
        }
    }
    bool found = false;
    std::list<std::pair<ring_ptr<T>, point_ptr_pair<T>>> iList;
    {
        auto range = dupe_ring.equal_range(ring_search);
        // Check for direct connection
        for (auto& it = range.first; it != range.second;) {
            if (!it->second.op1->ring) {
                it = dupe_ring.erase(it);
                continue;
            }
            if (!it->second.op2->ring) {
                it = dupe_ring.erase(it);
                continue;
            }
            ring_ptr<T> it_ring2 = it->second.op2->ring;
            if (it_ring2 == ring_origin) {
                found = true;
                if (*op_origin_1 != *(it->second.op2)) {
                    iList.emplace_back(ring_search, it->second);
                    break;
                }
            }
            ++it;
        }
    }
    if (iList.empty()) {
        auto range = dupe_ring.equal_range(ring_search);
        std::set<ring_ptr<T>> visited;
        visited.insert(ring_search);
        // Check for connection through chain of other intersections
        for (auto& it = range.first;
             it != range.second && it != dupe_ring.end() && it->first == ring_search; ++it) {
            ring_ptr<T> it_ring = it->second.op2->ring;
            if (it_ring != ring_search && *op_origin_2 != *it->second.op2 && it_ring != nullptr &&
                (ring_parent == it_ring || ring_parent == it_ring->parent) &&
                !value_is_zero(area(it_ring)) &&
                find_intersect_loop(dupe_ring, iList, ring_parent, ring_origin, it_ring, visited,
                                    op_origin_2, it->second.op2, rings)) {
                found = true;
                iList.emplace_front(ring_search, it->second);
                break;
            }
        }
    }
    if (!found) {
        point_ptr_pair<T> intPt_origin = { op_origin_1, op_origin_2 };
        point_ptr_pair<T> intPt_search = { op_origin_2, op_origin_1 };
        dupe_ring.emplace(ring_origin, std::move(intPt_origin));
        dupe_ring.emplace(ring_search, std::move(intPt_search));
        return false;
    }

    if (iList.empty()) {
        // The situation where both origin and search are holes might have a missing
        // search condition, we must check if a new pair must be added.
        bool missing = true;
        auto rng = dupe_ring.equal_range(ring_origin);
        // Check for direct connection
        for (auto& it = rng.first; it != rng.second; ++it) {
            ring_ptr<T> it_ring2 = it->second.op2->ring;
            if (it_ring2 == ring_search) {
                missing = false;
            }
        }
        if (missing) {
            point_ptr_pair<T> intPt_origin = { op_origin_1, op_origin_2 };
            dupe_ring.emplace(ring_origin, std::move(intPt_origin));
        }
        return false;
    }

    if (ring_is_hole(ring_origin)) {
        for (auto& iRing : iList) {
            ring_ptr<T> ring_itr = iRing.first;
            if (!ring_is_hole(ring_itr)) {
                // Make the hole the origin!
                point_ptr<T> op1 = op_origin_1;
                op_origin_1 = iRing.second.op1;
                iRing.second.op1 = op1;
                point_ptr<T> op2 = op_origin_2;
                op_origin_2 = iRing.second.op2;
                iRing.second.op2 = op2;
                iRing.first = ring_origin;
                ring_origin = ring_itr;
                ring_parent = ring_origin;
                break;
            }
        }
    }

    // Switch
    point_ptr<T> op_origin_1_next = op_origin_1->next;
    point_ptr<T> op_origin_2_next = op_origin_2->next;
    op_origin_1->next = op_origin_2_next;
    op_origin_2->next = op_origin_1_next;
    op_origin_1_next->prev = op_origin_2;
    op_origin_2_next->prev = op_origin_1;

    for (auto& iRing : iList) {
        mapbox::geometry::point<T> possible_rewind_point = find_rewind_point(iRing.second.op2);
        if (possible_rewind_point.y > rewind_point.y ||
            (possible_rewind_point.y == rewind_point.y &&
             possible_rewind_point.x < rewind_point.x)) {
            rewind_point.x = possible_rewind_point.x;
            rewind_point.y = possible_rewind_point.y;
        }
    }

    for (auto& iRing : iList) {
        point_ptr<T> op_search_1 = iRing.second.op1;
        point_ptr<T> op_search_2 = iRing.second.op2;
        point_ptr<T> op_search_1_next = op_search_1->next;
        point_ptr<T> op_search_2_next = op_search_2->next;
        op_search_1->next = op_search_2_next;
        op_search_2->next = op_search_1_next;
        op_search_1_next->prev = op_search_2;
        op_search_2_next->prev = op_search_1;
    }

    remove_spikes(op_origin_1);
    remove_spikes(op_origin_2);

    if (op_origin_1 == nullptr || op_origin_2 == nullptr) {
        if (op_origin_1 == nullptr && op_origin_2 == nullptr) {
            // Self destruction!
            ring_origin->points = nullptr;
            ring_origin->area = std::numeric_limits<double>::quiet_NaN();
            remove_ring(ring_origin, rings);
            for (auto& iRing : iList) {
                ring_ptr<T> ring_itr = iRing.first;
                ring_itr->points = nullptr;
                ring_itr->area = std::numeric_limits<double>::quiet_NaN();
                remove_ring(ring_itr, rings);
            }
        } else {
            if (op_origin_1 == nullptr) {
                ring_origin->points = op_origin_2;
            } else {
                //(op_origin_2 == nullptr)
                ring_origin->points = op_origin_1;
            }
            ring_origin->area = std::numeric_limits<double>::quiet_NaN();
            update_points_ring(ring_origin);
            for (auto& iRing : iList) {
                ring_ptr<T> ring_itr = iRing.first;
                ring_itr->points = nullptr;
                ring_itr->area = std::numeric_limits<double>::quiet_NaN();
                ring_itr->bottom_point = nullptr;
                if (ring_is_hole(ring_origin)) {
                    ring1_replaces_ring2(ring_origin, ring_itr, rings);
                } else {
                    ring1_replaces_ring2(ring_origin->parent, ring_itr, rings);
                }
            }
        }
    } else {
        ring_ptr<T> ring_new = create_new_ring(rings);
        std::size_t size_1 = 0;
        std::size_t size_2 = 0;
        double area_1 = area_from_point(op_origin_1, size_1);
        double area_2 = area_from_point(op_origin_2, size_2);
        if (ring_is_hole(ring_origin) && ((area_1 < 0.0))) {
            ring_origin->points = op_origin_1;
            ring_origin->area = area_1;
            ring_origin->size = size_1;
            ring_new->points = op_origin_2;
            ring_new->area = area_2;
            ring_new->size = size_2;
        } else {
            ring_origin->points = op_origin_2;
            ring_origin->area = area_2;
            ring_origin->size = size_2;
            ring_new->points = op_origin_1;
            ring_new->area = area_1;
            ring_new->size = size_1;
        }

        update_points_ring(ring_origin);
        update_points_ring(ring_new);

        ring_origin->bottom_point = nullptr;

        for (auto& iRing : iList) {
            ring_ptr<T> ring_itr = iRing.first;
            ring_itr->points = nullptr;
            ring_itr->area = std::numeric_limits<double>::quiet_NaN();
            ring_itr->bottom_point = nullptr;
            if (ring_is_hole(ring_origin)) {
                ring1_replaces_ring2(ring_origin, ring_itr, rings);
            } else {
                ring1_replaces_ring2(ring_origin->parent, ring_itr, rings);
            }
        }
        if (ring_is_hole(ring_origin)) {
            ring_new->parent = ring_origin;
            ring_new->parent->children.push_back(ring_new);
            fixup_children(ring_origin, ring_new);
            fixup_children(ring_parent, ring_new);
        } else {
            ring_new->parent = ring_origin->parent;
            if (ring_new->parent == nullptr) {
                rings.children.push_back(ring_new);
            } else {
                ring_new->parent->children.push_back(ring_new);
            }
            fixup_children(ring_origin, ring_new);
        }
    }

    std::list<std::pair<ring_ptr<T>, point_ptr_pair<T>>> move_list;

    for (auto& iRing : iList) {
        auto range_itr = dupe_ring.equal_range(iRing.first);
        if (range_itr.first != range_itr.second) {
            for (auto& it = range_itr.first; it != range_itr.second; ++it) {
                ring_ptr<T> it_ring = it->second.op1->ring;
                ring_ptr<T> it_ring2 = it->second.op2->ring;
                if (it_ring == nullptr || it_ring2 == nullptr || it_ring == it_ring2) {
                    continue;
                }
                if ((ring_is_hole(it_ring) || ring_is_hole(it_ring2))) {
                    move_list.emplace_back(it_ring, it->second);
                }
            }
            dupe_ring.erase(iRing.first);
        }
    }

    auto range_itr = dupe_ring.equal_range(ring_origin);
    for (auto& it = range_itr.first; it != range_itr.second;) {
        ring_ptr<T> it_ring = it->second.op1->ring;
        ring_ptr<T> it_ring2 = it->second.op2->ring;
        if (it_ring == nullptr || it_ring2 == nullptr || it_ring == it_ring2) {
            it = dupe_ring.erase(it);
            continue;
        }
        if (it_ring != ring_origin) {
            if ((ring_is_hole(it_ring) || ring_is_hole(it_ring2))) {
                move_list.emplace_back(it_ring, it->second);
            }
            it = dupe_ring.erase(it);
        } else {
            if ((ring_is_hole(it_ring) || ring_is_hole(it_ring2))) {
                ++it;
            } else {
                it = dupe_ring.erase(it);
            }
        }
    }

    if (!move_list.empty()) {
        dupe_ring.insert(move_list.begin(), move_list.end());
    }

    return true;
}

template <typename T>
struct point_ptr_cmp {
    inline bool operator()(point_ptr<T> op1, point_ptr<T> op2) {
        if (op1->y != op2->y) {
            return (op1->y > op2->y);
        } else if (op1->x != op2->x) {
            return (op1->x < op2->x);
        } else {
            std::size_t depth_1 = ring_depth(op1->ring);
            std::size_t depth_2 = ring_depth(op2->ring);
            return depth_1 > depth_2;
        }
    }
};

template <typename T>
struct point_ptr_depth_cmp {
    inline bool operator()(point_ptr<T> op1, point_ptr<T> op2) {
        std::size_t depth_1 = ring_depth(op1->ring);
        std::size_t depth_2 = ring_depth(op2->ring);
        return depth_1 > depth_2;
    }
};

template <typename T>
void update_duplicate_point_entries(
    ring_ptr<T> ring, std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring) {
    auto range = dupe_ring.equal_range(ring);
    std::list<std::pair<ring_ptr<T>, point_ptr_pair<T>>> move_list;
    for (auto& it = range.first; it != range.second;) {
        ring_ptr<T> it_ring = it->second.op1->ring;
        ring_ptr<T> it_ring_2 = it->second.op2->ring;
        if (it_ring == nullptr || it_ring_2 == nullptr) {
            it = dupe_ring.erase(it);
            continue;
        }
        if (it_ring != ring) {
            if ((ring_is_hole(it_ring) || ring_is_hole(it_ring_2))) {
                move_list.emplace_back(it_ring, it->second);
            }
            it = dupe_ring.erase(it);
        } else {
            if ((ring_is_hole(it_ring) || ring_is_hole(it_ring_2))) {
                ++it;
            } else {
                it = dupe_ring.erase(it);
            }
        }
    }
    if (!move_list.empty()) {
        dupe_ring.insert(move_list.begin(), move_list.end());
    }
}

template <typename T>
bool parent_in_tree(ring_ptr<T> r, ring_ptr<T> possible_parent) {

    ring_ptr<T> current_ring = r->parent;
    while (current_ring != nullptr) {
        if (current_ring == possible_parent) {
            return true;
        }
        current_ring = current_ring->parent;
    }
    return false;
}

template <typename T>
void fixup_children_new_interior_ring(ring_ptr<T> old_ring,
                                      ring_ptr<T> new_ring,
                                      ring_manager<T>& rings) {
    bool old_ring_area_is_positive = area(old_ring) > 0.0;
    // Now we must search the siblings of the old ring
    // This is slow, but there is no other way I know of currently
    // to solve these problems.
    if (old_ring->parent == nullptr) {
        for (auto r = rings.children.begin(); r != rings.children.end();) {
            assert((*r)->points);
            bool ring_area_is_positive = area((*r)) > 0.0;
            if ((*r) != new_ring && ring_area_is_positive == old_ring_area_is_positive &&
                poly2_contains_poly1((*r)->points, new_ring->points)) {
                (*r)->parent = new_ring;
                new_ring->children.push_back((*r));
                r = rings.children.erase(r);
            } else {
                ++r;
            }
        }
    } else {
        ring_ptr<T> parent = old_ring->parent;
        for (auto r = parent->children.begin(); r != parent->children.end();) {
            assert((*r)->points);
            assert((*r) != parent);
            bool ring_area_is_positive = area((*r)) > 0.0;
            if ((*r) != new_ring && ring_area_is_positive == old_ring_area_is_positive &&
                poly2_contains_poly1((*r)->points, new_ring->points)) {
                (*r)->parent = new_ring;
                new_ring->children.push_back((*r));
                r = parent->children.erase(r);
            } else {
                ++r;
            }
        }
    }
}

#ifdef DEBUG

template <typename T>
void check_if_intersections_cross(point_ptr<T> p1, point_ptr<T> p2) {
    // LCOV_EXCL_START
    point_ptr<T> p1_next = p1->next;
    point_ptr<T> p2_next = p2->next;
    point_ptr<T> p1_prev = p1->prev;
    point_ptr<T> p2_prev = p2->prev;
    while (*p1_next == *p1) {
        if (p1_next == p1) {
            return;
        }
        p1_next = p1_next->next;
    }
    while (*p2_next == *p2) {
        if (p2_next == p2) {
            return;
        }
        p2_next = p2_next->next;
    }
    while (*p1_prev == *p1) {
        if (p1_prev == p1) {
            return;
        }
        p1_prev = p1_prev->prev;
    }
    while (*p2_prev == *p2) {
        if (p2_prev == p2) {
            return;
        }
        p2_prev = p2_prev->prev;
    }
    double a1_p1 = std::atan2(static_cast<double>(p1_prev->y - p1->y),
                              static_cast<double>(p1_prev->x - p1->x));
    double a2_p1 = std::atan2(static_cast<double>(p1_next->y - p1->y),
                              static_cast<double>(p1_next->x - p1->x));
    double a1_p2 = std::atan2(static_cast<double>(p2_prev->y - p2->y),
                              static_cast<double>(p2_prev->x - p2->x));
    double a2_p2 = std::atan2(static_cast<double>(p2_next->y - p2->y),
                              static_cast<double>(p2_next->x - p2->x));
    double min_p1 = std::min(a1_p1, a2_p1);
    double max_p1 = std::max(a1_p1, a2_p1);
    double min_p2 = std::min(a1_p2, a2_p2);
    double max_p2 = std::max(a1_p2, a2_p2);
    if ((min_p1 < max_p2 && min_p1 > min_p2 && max_p1 > max_p2) ||
        (min_p2 < max_p1 && min_p2 > min_p1 && max_p2 > max_p1)) {
        throw std::runtime_error("Paths are found to be crossing");
    }
    // LCOV_EXCL_END
}

#endif

template <typename T>
void handle_self_intersections(point_ptr<T> op,
                               point_ptr<T> op2,
                               std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                               ring_manager<T>& rings) {
    // Check that are same ring
    assert(op->ring == op2->ring);
    ring_ptr<T> ring = op->ring;
    double original_area = area(ring);
    bool original_is_positive = (original_area > 0.0);
#ifdef DEBUG
    check_if_intersections_cross(op, op2);
#endif

    // split the polygon into two ...
    point_ptr<T> op3 = op->prev;
    point_ptr<T> op4 = op2->prev;
    op->prev = op4;
    op4->next = op;
    op2->prev = op3;
    op3->next = op2;

    remove_spikes(op);
    remove_spikes(op2);

    if (op == nullptr && op2 == nullptr) {
        // Self destruction!
        // I am not positive that it could ever reach this point -- but leaving
        // the logic in here for now.
        ring->points = nullptr;
        ring->area = std::numeric_limits<double>::quiet_NaN();
        remove_ring(ring, rings);
        update_duplicate_point_entries(ring, dupe_ring);
        return;
    } else if (op == nullptr) {
        ring->points = op2;
        ring->area = std::numeric_limits<double>::quiet_NaN();
        update_duplicate_point_entries(ring, dupe_ring);
        return;
    } else if (op2 == nullptr) {
        ring->points = op;
        ring->area = std::numeric_limits<double>::quiet_NaN();
        update_duplicate_point_entries(ring, dupe_ring);
        return;
    }

    ring_ptr<T> new_ring = create_new_ring(rings);
    std::size_t size_1 = 0;
    std::size_t size_2 = 0;
    double area_1 = area_from_point(op, size_1);
    double area_2 = area_from_point(op2, size_2);
    bool area_1_is_positive = (area_1 > 0.0);
    bool area_2_is_positive = (area_2 > 0.0);
    bool area_1_is_zero = value_is_zero(area_1);
    bool area_2_is_zero = value_is_zero(area_2);

    // Situation # 1 - Orientations are NOT the same:
    // - One ring contains the other and MUST be a child of that ring
    // - The one that changed orientation is the child of the other ring
    //
    // Situation # 2 - Orientations are the same
    // - The rings are now split, such a new ring of the same orientation
    //   must be created.
    // - If the new ring is WITHIN the old ring:
    //      * It WILL be the child of a hole of that ring (this ring may not yet be created)
    //        or possible the child of a child of a child of the ring (an so on)...
    // - If the new ring is OUTSIDE the old ring:
    //      * It may contain any of the children of the old ring.
    if (area_2_is_zero || area_1_is_zero || area_1_is_positive != area_2_is_positive) {
        // Situation #1 - new_ring is contained by ring ...
        if (area_2_is_zero || (!area_1_is_zero && area_1_is_positive == original_is_positive)) {
            ring->points = op;
            ring->area = area_1;
            ring->size = size_1;
            new_ring->points = op2;
            new_ring->area = area_2;
            new_ring->size = size_2;
        } else {
            ring->points = op2;
            ring->area = area_2;
            ring->size = size_2;
            new_ring->points = op;
            new_ring->area = area_1;
            new_ring->size = size_1;
        }
        update_points_ring(ring);
        update_points_ring(new_ring);
        new_ring->parent = ring;
        new_ring->parent->children.push_back(new_ring);
        fixup_children_new_interior_ring(ring, new_ring, rings);
    } else {
        // Situation #2 - create new ring
        // The largest absolute area is the parent
        if (std::fabs(area_1) > std::fabs(area_2)) {
            ring->points = op;
            ring->area = area_1;
            ring->size = size_1;
            new_ring->points = op2;
            new_ring->area = area_2;
            new_ring->size = size_2;
        } else {
            ring->points = op2;
            ring->area = area_2;
            ring->size = size_2;
            new_ring->points = op;
            new_ring->area = area_1;
            new_ring->size = size_1;
        }
        update_points_ring(ring);
        update_points_ring(new_ring);
        if (poly2_contains_poly1(new_ring->points, ring->points)) {
            // This is the situation where there is the new ring is
            // created inside the ring. Later on this should be inherited
            // as child of a newly created hole. However, we should check existing
            // holes of this polygon to see if they might belong inside this polygon.
            new_ring->parent = ring;
            new_ring->parent->children.push_back(new_ring);
            fixup_children(ring, new_ring);
        } else {
            // Polygons are completely seperate
            new_ring->parent = ring->parent;
            if (new_ring->parent == nullptr) {
                rings.children.push_back(new_ring);
            } else {
                new_ring->parent->children.push_back(new_ring);
            }
            fixup_children(ring, new_ring);
        }
    }
    update_duplicate_point_entries(ring, dupe_ring);
}

template <typename T>
mapbox::geometry::point<T> find_rewind_point(point_ptr<T> pt) {
    mapbox::geometry::point<T> rewind;
    rewind.x = pt->x;
    rewind.y = pt->y;
    point_ptr<T> itr = pt->next;
    while (pt != itr) {
        if (itr->y > rewind.y || (itr->y == rewind.y && itr->x < rewind.x)) {
            rewind.x = itr->x;
            rewind.y = itr->y;
        }
        itr = itr->next;
    }
    return rewind;
}

template <typename T>
bool handle_collinear_edges(point_ptr<T> pt1,
                            point_ptr<T> pt2,
                            std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                            ring_manager<T>& rings,
                            mapbox::geometry::point<T>& rewind_point) {
    ring_ptr<T> ring1 = pt1->ring;
    ring_ptr<T> ring2 = pt2->ring;
    if (ring1 == ring2) {
        return false;
    }

    bool valid = (ring1 != ring2 && (ring1->parent == ring2->parent || ring2->parent == ring1 ||
                                     ring1->parent == ring2));
    if (!valid) {
        return false;
    }

    if (*(pt1->next) != *(pt2->prev) && *(pt2->next) != *(pt1->prev)) {
        return false;
    }

    if (ring1->parent == ring2) {
        // switch ring1 and ring2
        std::swap(pt1, pt2);
        std::swap(ring1, ring2);
    }

    mapbox::geometry::point<T> rewind_1 = find_rewind_point(pt1);
    mapbox::geometry::point<T> rewind_2 = find_rewind_point(pt2);

    // The lower right of the two points is the rewind point.
    mapbox::geometry::point<T> possible_rewind;
    if (rewind_1.y > rewind_2.y) {
        possible_rewind = rewind_2;
    } else if (rewind_1.y < rewind_2.y) {
        possible_rewind = rewind_1;
    } else if (rewind_1.x > rewind_2.x) {
        possible_rewind = rewind_1;
    } else {
        possible_rewind = rewind_2;
    }
    if (possible_rewind.y > rewind_point.y ||
        (possible_rewind.y == rewind_point.y && possible_rewind.x < rewind_point.x)) {
        rewind_point.x = possible_rewind.x;
        rewind_point.y = possible_rewind.y;
    }

    // swap points
    point_ptr<T> pt3 = pt1->prev;
    point_ptr<T> pt4 = pt2->prev;
    pt1->prev = pt4;
    pt4->next = pt1;
    pt2->prev = pt3;
    pt3->next = pt2;

    // remove spikes
    remove_spikes(pt1);
    if (!pt1) {
        // rings self destructed
        ring1->points = nullptr;
        ring1->area = std::numeric_limits<double>::quiet_NaN();
        remove_ring(ring1, rings);
        ring2->points = nullptr;
        ring2->area = std::numeric_limits<double>::quiet_NaN();
        remove_ring(ring2, rings);
        return false;
    }
    if (pt2->ring) {
        remove_spikes(pt2);
        if (!pt2) {
            // rings self destructed
            // Not sure this logic is reachable, but leaving it in for now.
            ring1->points = nullptr;
            ring1->area = std::numeric_limits<double>::quiet_NaN();
            remove_ring(ring1, rings);
            ring2->points = nullptr;
            ring2->area = std::numeric_limits<double>::quiet_NaN();
            remove_ring(ring2, rings);
            return false;
        }
        // We could have removed pt1 during this process..
        // if we did we need to reassign pt2 to pt1
        if (!pt1->ring) {
            pt1 = pt2;
        }
    }
    ring1->points = pt1;
    ring2->points = nullptr;
    ring1->area = std::numeric_limits<double>::quiet_NaN();
    ring2->area = std::numeric_limits<double>::quiet_NaN();
    if (ring2->parent == ring1) {
        ring1_replaces_ring2(ring1->parent, ring2, rings);
    } else {
        ring1_replaces_ring2(ring1, ring2, rings);
    }
    update_points_ring(ring1);
    update_duplicate_point_entries(ring2, dupe_ring);

    return true;
}

template <typename T>
bool point_2_is_between_point_1_and_point_3(point_ptr<T> pt1, point_ptr<T> pt2, point_ptr<T> pt3) {
    if ((*pt1 == *pt3) || (*pt1 == *pt2) || (*pt3 == *pt2)) {
        return false;
    } else if (pt1->x != pt3->x) {
        return (pt2->x > pt1->x) == (pt2->x < pt3->x);
    } else {
        return (pt2->y > pt1->y) == (pt2->y < pt3->y);
    }
}

enum orientation_type : std::uint8_t {
    orientation_collinear_spike = 0,
    orientation_clockwise,
    orientation_collinear_line,
    orientation_counter_clockwise
};

// To find orientation of ordered triplet (p, q, r)
// Orientation between q and r with respect to p.
template <typename T>
inline orientation_type orientation_of_points(point_ptr<T> p, point_ptr<T> q, point_ptr<T> r) {
    T val = (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y);
    if (val == 0) {
        if (point_2_is_between_point_1_and_point_3(q, p, r)) {
            return orientation_collinear_line;
        } else {
            return orientation_collinear_spike;
        }
    }
    return (val > 0) ? orientation_clockwise : orientation_counter_clockwise;
}

// Self intersection point vector
template <typename T>
using si_point_vector = std::vector<std::pair<point_ptr<T>, bool>>;

#ifdef DEBUG

template <typename T>
std::string output_si_angles(point_ptr<T> pt) {
    // LCOV_EXCL_START
    std::ostringstream out;
    double prev_angle = std::atan2(static_cast<double>(pt->prev->y - pt->y),
                                   static_cast<double>(pt->prev->x - pt->x));
    prev_angle = (180.0 / M_PI) * prev_angle;
    if (prev_angle < 0.0) {
        prev_angle += 360.0;
    }
    double next_angle = std::atan2(static_cast<double>(pt->next->y - pt->y),
                                   static_cast<double>(pt->next->x - pt->x));
    next_angle = (180.0 / M_PI) * next_angle;
    if (next_angle < 0.0) {
        next_angle += 360.0;
    }
    out << " angles: " << prev_angle << " , " << next_angle;
    out << " - [[" << pt->prev->x << "," << pt->prev->y << "],[";
    out << pt->x << "," << pt->y << "],[";
    out << pt->next->x << "," << pt->next->y << "]]";
    return out.str();
    // LCOV_EXCL_END
}

template <typename T>
std::string output_compare_si_angles(point_ptr<T> pt, point_ptr<T> compare) {
    std::ostringstream out;
    double cmp_prev_angle = std::atan2(static_cast<double>(compare->prev->y - compare->y),
                                       static_cast<double>(compare->prev->x - compare->x));
    double prev_angle = std::atan2(static_cast<double>(pt->prev->y - pt->y),
                                   static_cast<double>(pt->prev->x - pt->x));
    prev_angle = (180.0 / M_PI) * prev_angle;
    if (prev_angle < 0.0) {
        prev_angle += 360.0;
    }
    cmp_prev_angle = (180.0 / M_PI) * cmp_prev_angle;
    if (cmp_prev_angle < 0.0) {
        cmp_prev_angle += 360.0;
    }
    double cmp_next_angle = std::atan2(static_cast<double>(compare->next->y - compare->y),
                                       static_cast<double>(compare->next->x - compare->x));
    double next_angle = std::atan2(static_cast<double>(pt->next->y - pt->y),
                                   static_cast<double>(pt->next->x - pt->x));
    next_angle = (180.0 / M_PI) * next_angle;
    if (next_angle < 0.0) {
        next_angle += 360.0;
    }
    cmp_next_angle = (180.0 / M_PI) * cmp_next_angle;
    if (cmp_next_angle < 0.0) {
        cmp_next_angle += 360.0;
    }
    out << " compared to prev: " << prev_angle - cmp_prev_angle << ", "
        << next_angle - cmp_prev_angle << std::endl;
    out << " compared to next: " << prev_angle - cmp_next_angle << ", "
        << next_angle - cmp_next_angle << std::endl;
    return out.str();
}

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const si_point_vector<T>& pts) {
    out << "Self Intersection Point Vector:" << std::endl;
    for (auto& pt : pts) {
        out << output_si_angles(pt.first);
        if (pt.second) {
            out << " - clockwise" << std::endl;
        } else {
            out << " - counter clockwise" << std::endl;
        }
    }
    return out;
}

template <class charT, class traits>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const orientation_type& ot) {
    switch (ot) {
    case orientation_collinear_spike:
        out << "collinear - spike";
        break;
    case orientation_clockwise:
        out << "clockwise";
        break;
    case orientation_collinear_line:
        out << "collinear - line";
        break;
    case orientation_counter_clockwise:
        out << "counter clockwise";
        break;
    };
    return out;
}

#endif

template <typename T>
bool clockwise_of_next(point_ptr<T> const& origin, point_ptr<T> pt) {

    // Determine if the prev and next of pt is either clockwise of next from
    // origin (and therefore ccw of previous) or if it is ccw of next and clockwise
    // of previous

    // First inspect orientation of points, keep in mind this is the orientations of
    // the three points
    orientation_type ot_origin = orientation_of_points(origin, origin->next, origin->prev);
    if (ot_origin == orientation_collinear_spike) {
        return true;
    } else if (ot_origin == orientation_clockwise) {
        orientation_type ot_prev_next = orientation_of_points(origin, origin->next, pt->prev);
        if (ot_prev_next == orientation_collinear_spike) {
            orientation_type ot_next_next = orientation_of_points(origin, origin->next, pt->next);
            if (ot_next_next == orientation_collinear_spike) {
                // Pt forms a spike on origin next
                // so lets assume it is CW.
                // I am not sure that we would ever encounter a spike so this will
                // be missing from coverage, but left in code.
                return true;
            } else if (ot_next_next == orientation_clockwise) {
                // We need to check which is after "origin->next" traveling
                // clockwise -- origin->prev or pt->next
                orientation_type ot_next_prev =
                    orientation_of_points(origin, origin->prev, pt->next);
                if (ot_next_prev == orientation_collinear_spike) {
                    // Both origin and pt follow the same paths.
                    // so we will call this clockwise
                    return true;
                } else if (ot_next_prev == orientation_clockwise) {
                    // pt->next is clockwise of prev
                    return false;
                } else if (ot_next_prev == orientation_collinear_line) {
                    // This shouldn't happen
                    // LCOV_EXCL_START
                    throw std::runtime_error("Impossible situation reached in clockwise_of_next");
                    // LCOV_EXCL_END
                } else {
                    // Pt next is counter clockwise of origin prev
                    return true;
                }
            } else {
                // ot_next_next == orientation_collinear_line
                // ot_next_next == orientation_counter_clockwise
                return false;
            }
        } else if (ot_prev_next == orientation_clockwise) {
            // We need to check which is after "origin->next" traveling
            // clockwise -- origin->prev or pt->prev
            orientation_type ot_prev_prev = orientation_of_points(origin, origin->prev, pt->prev);
            if (ot_prev_prev == orientation_clockwise ||
                ot_prev_prev == orientation_collinear_spike) {
                // pt->prev is before this, so.. between the two.
                return false;
            } else {
                // ot_prev_prev == orientation_clockwise
                // ot_prev_prev == orientation_collinear_line
                return true;
            }
        } else {
            // ot_prev_next == orientation_collinear_line
            // ot_prev_next == orientation_counter_clockwise
            return false;
        }
    } else if (ot_origin == orientation_collinear_line) {
        orientation_type ot_prev_next = orientation_of_points(origin, origin->next, pt->prev);
        if (ot_prev_next == orientation_collinear_spike ||
            ot_prev_next == orientation_collinear_line) {
            // prev and next on top of each other
            orientation_type ot_next_next = orientation_of_points(origin, origin->next, pt->next);
            if (ot_next_next == orientation_counter_clockwise) {
                return false;
            } else {
                // ot_next_next == orientation_collinear_spike
                // ot_next_next == orientation_clockwise
                // ot_next_next == orientation_collinear_line
                return true;
            }
        } else if (ot_prev_next == orientation_clockwise) {
            return true;
        } else {
            // ot_prev_next == orientation_counter_clockwise
            return false;
        }
    } else {
        // orientation_counter_clockwise
        orientation_type ot_prev_next = orientation_of_points(origin, origin->next, pt->prev);
        if (ot_prev_next == orientation_collinear_spike) {
            orientation_type ot_next_next = orientation_of_points(origin, origin->next, pt->next);
            if (ot_next_next == orientation_collinear_spike) {
                // Pt forms a spike on origin next
                // so lets assume it is CW.
                // I am not sure that we would ever encounter a spike so this will
                // be missing from coverage, but left in code.
                return true;
            } else if (ot_next_next == orientation_counter_clockwise) {
                // We need to check which is after "origin->next" traveling
                // counter clockwise -- origin->prev or pt->next
                orientation_type ot_next_prev =
                    orientation_of_points(origin, origin->prev, pt->next);
                if (ot_next_prev == orientation_collinear_spike) {
                    // Both origin and pt follow the same paths.
                    // so we will call this clockwise
                    return true;
                } else if (ot_next_prev == orientation_clockwise) {
                    // pt->next is clockwise of prev
                    return false;
                } else if (ot_next_prev == orientation_collinear_line) {
                    // This shouldn't happen?
                    // LCOV_EXCL_START
                    throw std::runtime_error("Impossible situation reached in clockwise_of_next - 2");
                    // LCOV_EXCL_END
                } else {
                    // Pt next is counter clockwise of origin prev
                    return true;
                }
            } else {
                // ot_next_next == orientation_clockwise
                // ot_next_next == orientation_collinear_line
                return true;
            }
        } else if (ot_prev_next == orientation_counter_clockwise) {
            // We need to check which is after "origin->next" traveling
            // counter clockwise -- origin->prev or pt->prev
            orientation_type ot_prev_prev = orientation_of_points(origin, origin->prev, pt->prev);
            if (ot_prev_prev == orientation_clockwise ||
                ot_prev_prev == orientation_collinear_spike) {
                // pt->prev is before this, so.. between the two.
                return false;
            } else {
                // ot_prev_prev == orientation_counter_clockwise
                // ot_prev_prev == orientation_collinear_line
                return true;
            }
        } else {
            // ot_prev_next == orientation_collinear_line
            // ot_prev_next == orientation_clockwise
            return true;
        }
    }
}

template <typename T>
inline bool cw_p1p2_prev_collinear_spike(point_ptr<T> const& origin,
                                         point_ptr<T> const& next,
                                         point_ptr<T> const& p1,
                                         point_ptr<T> const& p2) {

    // we must compare the nexts to determine the order between the two.
    orientation_type ot_p1_next = orientation_of_points(origin, next, p1->next);
    orientation_type ot_p2_next = orientation_of_points(origin, next, p2->next);
    if (ot_p1_next == orientation_collinear_spike) {
        // p1 is a spike on origin next
        if (ot_p2_next == orientation_collinear_spike) {
            // I am not sure that a spike will ever occur at this point in the processing!
            return true;
        } else {
            return false;
        }
    } else if (ot_p1_next == orientation_clockwise) {
        if (ot_p2_next == orientation_collinear_spike) {
            return true;
        } else if (ot_p2_next == orientation_clockwise) {
            // Both are clockwise we have to compare.
            orientation_type ot = orientation_of_points(origin, p1->next, p2->next);
            if (ot == orientation_collinear_spike) {
                return true;
            } else if (ot == orientation_clockwise) {
                return false;
            } else if (ot == orientation_collinear_line) {
                return false;
            } else {
                return true;
            }
        } else {
            // ot_p2_next == orienation_collinear_line
            // ot_p2_next == orienation_counter_clockwise
            return false;
        }
    } else if (ot_p1_next == orientation_collinear_line) {
        if (ot_p2_next == orientation_counter_clockwise) {
            return false;
        } else {
            // ot_p2_next == orienation_collinear_spike
            // ot_p2_next == orienation_clockwise
            // ot_p2_next == orienation_collinear_line
            return true;
        }
    } else {
        // ot_p1_next == orientation_counter_clockwise
        if (ot_p2_next == orientation_counter_clockwise) {
            // Both are counter clockwise we have to compare.
            orientation_type ot = orientation_of_points(origin, p1->next, p2->next);
            if (ot == orientation_collinear_spike) {
                return true;
            } else if (ot == orientation_clockwise) {
                return false;
            } else if (ot == orientation_collinear_line) {
                return false;
            } else {
                return true;
            }
        } else {
            // ot_p2_next == orienation_collinear_spike
            // ot_p2_next == orienation_clockwise
            // ot_p2_next == orienation_collinear_line
            return true;
        }
    }
}

template <typename T>
inline bool ccw_p1p2_prev_collinear_spike(point_ptr<T> const& origin,
                                          point_ptr<T> const& next,
                                          point_ptr<T> const& p1,
                                          point_ptr<T> const& p2) {

    // we must compare the nexts to determine the order between the two.
    orientation_type ot_p1_next = orientation_of_points(origin, next, p1->next);
    orientation_type ot_p2_next = orientation_of_points(origin, next, p2->next);
    if (ot_p1_next == orientation_collinear_spike) {
        // p1 is a spike on origin next
        if (ot_p2_next == orientation_collinear_spike) {
            return true;
        } else {
            return false;
        }
    } else if (ot_p1_next == orientation_clockwise) {
        if (ot_p2_next == orientation_collinear_spike) {
            return false;
        } else if (ot_p2_next == orientation_clockwise) {
            // Both are clockwise we have to compare.
            orientation_type ot = orientation_of_points(origin, p1->next, p2->next);
            if (ot == orientation_collinear_spike) {
                return true;
            } else if (ot == orientation_clockwise) {
                return true;
            } else if (ot == orientation_collinear_line) {
                return true;
            } else {
                return false;
            }
        } else {
            // ot_p2_next == orienation_collinear_line
            // ot_p2_next == orienation_counter_clockwise
            return true;
        }
    } else if (ot_p1_next == orientation_collinear_line) {
        if (ot_p2_next == orientation_clockwise) {
            return false;
        } else {
            // ot_p2_next == orienation_collinear_spike
            // ot_p2_next == orienation_counter_clockwise
            // ot_p2_next == orienation_collinear_line
            return true;
        }
    } else {
        // ot_p1_next == orientation_counter_clockwise
        if (ot_p2_next == orientation_counter_clockwise) {
            // Both are counter clockwise we have to compare.
            orientation_type ot = orientation_of_points(origin, p1->next, p2->next);
            if (ot == orientation_collinear_spike) {
                return true;
            } else if (ot == orientation_clockwise) {
                return true;
            } else if (ot == orientation_collinear_line) {
                return true;
            } else {
                return false;
            }
        } else {
            // ot_p2_next == orienation_collinear_spike
            // ot_p2_next == orienation_clockwise
            // ot_p2_next == orienation_collinear_line
            return false;
        }
    }
}

template <typename T>
struct si_point_sorter {

    point_ptr<T> origin;
    point_ptr<T> next;

    si_point_sorter(point_ptr<T> origin_) : origin(origin_), next(origin_->next) {
    }

    // Sorting order
    //
    // Primary Sort:
    // * Left of next, right of previous
    // * Right of next, left of previous
    //
    // Secondary Sort:
    // * Magnitude of angle (direction based on primary sort)
    //   between item's previous and origin's next

    inline bool operator()(std::pair<point_ptr<T>, bool> const& pp1,
                           std::pair<point_ptr<T>, bool> const& pp2) {
        // Because a next must be paired with a previous, we are only
        // caring first about previous segments for ordering
        point_ptr<T> p1 = pp1.first;
        point_ptr<T> p2 = pp2.first;
        if (pp1.second != pp2.second) {
            return pp1.second;
        }

        orientation_type ot_p1 = orientation_of_points(origin, next, p1->prev);
        orientation_type ot_p2 = orientation_of_points(origin, next, p2->prev);
        if (pp1.second) {
            if (ot_p1 == orientation_collinear_spike) {
                // p1 prev lines up with origin next
                if (ot_p2 != orientation_collinear_spike) {
                    // ot_p2 == orienation_clockwise
                    // ot_p2 == orienation_collinear_line
                    // ot_p2 == orienation_counter_clockwise
                    return true;
                } else {
                    // ot_p2 == orientation_collinear_spike
                    return cw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                }
            } else if (ot_p1 == orientation_clockwise) {
                if (ot_p2 == orientation_collinear_spike) {
                    return false;
                } else if (ot_p2 == orientation_clockwise) {
                    orientation_type ot_p1p2 = orientation_of_points(origin, p1->prev, p2->prev);
                    if (ot_p1p2 == orientation_collinear_spike) {
                        // Both p1 prev and p2 prev are on top of each other
                        return cw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                    } else if (ot_p1p2 == orientation_clockwise) {
                        return true;
                    } else {
                        // ot_p1p2 == orientation_counter_clockwise
                        // ot_p1p2 == orientation_collinear_line
                        return false;
                    }
                } else {
                    // ot_p2 == orientation_collinear_line
                    // ot_p2 == orientation_counter_clockwise
                    return true;
                }
            } else if (ot_p1 == orientation_collinear_line) {
                if (ot_p2 == orientation_collinear_spike || ot_p2 == orientation_clockwise) {
                    return false;
                } else if (ot_p2 == orientation_collinear_line) {
                    return cw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                } else {
                    // ot_p2 == orientation_counter_clockwise
                    return true;
                }
            } else {
                // ot_p1 == orientation_counter_clockwise
                if (ot_p2 == orientation_counter_clockwise) {
                    orientation_type ot_p1p2 = orientation_of_points(origin, p1->prev, p2->prev);
                    if (ot_p1p2 == orientation_collinear_spike) {
                        // Both p1 prev and p2 prev are on top of each other
                        return cw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                    } else if (ot_p1p2 == orientation_clockwise) {
                        return true;
                    } else {
                        // ot_p1p2 == orientation_counter_clockwise
                        // ot_p1p2 == orientation_collinear_line
                        return false;
                    }
                } else {
                    // ot_p2 == orientation_collinear_spike
                    // ot_p2 == orientation_counter_clockwise
                    // ot_p2 == orientation_collinear_line
                    return false;
                }
            }
        } else {
            if (ot_p1 == orientation_collinear_spike) {
                // p1 prev lines up with origin next
                if (ot_p2 != orientation_collinear_spike) {
                    // ot_p2 == orienation_clockwise
                    // ot_p2 == orienation_collinear_line
                    // ot_p2 == orienation_counter_clockwise
                    return true;
                } else {
                    // ot_p2 == orientation_collinear_spike
                    return ccw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                }
            } else if (ot_p1 == orientation_clockwise) {
                if (ot_p2 == orientation_collinear_spike) {
                    return false;
                } else if (ot_p2 == orientation_clockwise) {
                    orientation_type ot_p1p2 = orientation_of_points(origin, p1->prev, p2->prev);
                    if (ot_p1p2 == orientation_collinear_spike) {
                        // Both p1 prev and p2 prev are on top of each other
                        return ccw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                    } else if (ot_p1p2 == orientation_counter_clockwise) {
                        return true;
                    } else {
                        // ot_p1p2 == orientation_clockwise
                        // ot_p1p2 == orientation_collinear_line
                        return false;
                    }
                } else {
                    // ot_p2 == orientation_collinear_line
                    // ot_p2 == orientation_counter_clockwise
                    return false;
                }
            } else if (ot_p1 == orientation_collinear_line) {
                if (ot_p2 == orientation_collinear_spike ||
                    ot_p2 == orientation_counter_clockwise) {
                    return false;
                } else if (ot_p2 == orientation_collinear_line) {
                    return ccw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                } else {
                    // ot_p2 == orientation_clockwise
                    return true;
                }
            } else {
                // ot_p1 == orientation_counter_clockwise
                if (ot_p2 == orientation_collinear_spike) {
                    return false;
                } else if (ot_p2 == orientation_counter_clockwise) {
                    orientation_type ot_p1p2 = orientation_of_points(origin, p1->prev, p2->prev);
                    if (ot_p1p2 == orientation_collinear_spike) {
                        // Both p1 prev and p2 prev are on top of each other
                        return ccw_p1p2_prev_collinear_spike(origin, next, p1, p2);
                    } else if (ot_p1p2 == orientation_counter_clockwise) {
                        return true;
                    } else {
                        // ot_p1p2 == orientation_clockwise
                        // ot_p1p2 == orientation_collinear_line
                        return false;
                    }
                } else {
                    // ot_p2 == orientation_clockwise
                    // ot_p2 == orientation_collinear_line
                    return true;
                }
            }
        }
    }
};

template <typename T>
si_point_vector<T> build_si_point_vector(std::size_t first_index,
                                         std::size_t last_index,
                                         std::size_t current_index,
                                         ring_ptr<T> match_ring,
                                         ring_manager<T>& rings) {
    si_point_vector<T> point_vec;
    point_ptr<T> origin = rings.all_points[current_index];
    for (std::size_t j = first_index; j <= last_index; ++j) {
        if (j == current_index) {
            continue;
        }
        point_ptr<T> op_j = rings.all_points[j];
        if (op_j->ring == match_ring) {
            bool clockwise = clockwise_of_next(origin, op_j);
            point_vec.emplace_back(op_j, clockwise);
        }
    }
    return point_vec;
}

template <typename T>
bool process_repeated_point_set(std::size_t first_index,
                                std::size_t last_index,
                                std::size_t current_index,
                                std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                                ring_manager<T>& rings) {
    point_ptr<T> point_1 = rings.all_points[current_index];

    if (point_1->ring == nullptr) {
        return false;
    }

    // Build point vector
    si_point_vector<T> vec =
        build_si_point_vector(first_index, last_index, current_index, point_1->ring, rings);

    if (vec.empty()) {
        return false;
    }

    // Sort points in vector
    std::stable_sort(vec.begin(), vec.end(), si_point_sorter<T>(point_1));

    auto vec_itr = vec.begin();
    point_ptr<T> point_2 = vec_itr->first;

    // If there are collinear sets of lines, we might not be able to just pick
    // the first point in the sorted list (and we will have to do special processing).
    if (vec.size() > 2) {
        ++vec_itr;
        point_ptr<T> point_3 = vec_itr->first;
        orientation_type ot_next = orientation_of_points(point_2, point_2->next, point_3->next);
        if (ot_next == orientation_collinear_spike) {
            orientation_type ot_prev = orientation_of_points(point_2, point_2->prev, point_3->prev);
            if (ot_prev == orientation_collinear_spike) {
                // Start at point_1 and we will travel around the circle until we find another point at
                // the same location. We will measure its area, and then continue till the next point at
                // the same location. The smallest absolute value of area is the one we will use.
                point_ptr<T> point_a = point_1;
                point_ptr<T> min_a = nullptr;
                point_ptr<T> min_b = nullptr;
                point_ptr<T> pt = point_1->next;
                double area = 0.0;
                double min_area = std::numeric_limits<double>::max();
                while (pt != point_1) {
                    area += static_cast<double>(pt->prev->x + pt->x) * static_cast<double>(pt->prev->y - pt->y);
                    if (*pt == *point_1) {
                        if (std::fabs(area) < min_area) {
                            min_area = std::fabs(area);
                            min_a = point_a;
                            min_b = pt;
                        }
                        point_a = pt;
                        area = 0.0;
                    }
                    pt = pt->next;
                }
                if (point_a == point_1) {
                    // LCOV_EXCL_START
                    throw std::runtime_error("No other point was between point_1 on the path");
                    // LCOV_EXCL_END
                }
                area += static_cast<double>(pt->prev->x + pt->x) * static_cast<double>(pt->prev->y - pt->y);
                if (std::fabs(area) < min_area) {
                    min_area = std::fabs(area);
                    min_a = point_a;
                    min_b = pt;
                }
                handle_self_intersections(min_a, min_b, dupe_ring, rings);
                return true;
            }
        }
    }
    handle_self_intersections(point_1, point_2, dupe_ring, rings);
    return true;
}

template <typename T>
void process_repeated_points(std::size_t first_index,
                             std::size_t last_index,
                             std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                             ring_manager<T>& rings) {
    for (std::size_t j = first_index; j <= last_index; ++j) {
        while (process_repeated_point_set(first_index, last_index, j, dupe_ring, rings))
            ;
    }

#ifdef DEBUG
    // LCOV_EXCL_START
    for (std::size_t j = first_index; j <= last_index; ++j) {
        point_ptr<T> op_j = rings.all_points[j];
        if (!op_j->ring) {
            continue;
        }
        double ring_area = area(op_j->ring);
        bool ring_is_positive = ring_area > 0.0;
        bool ring_is_zero = value_is_zero(ring_area);
        if (!ring_is_zero) {
            if (op_j->ring->parent) {
                double parent_area = area(op_j->ring->parent);
                bool parent_is_positive = parent_area > 0.0;
                bool parent_is_zero = value_is_zero(parent_area);
                if (!parent_is_zero && ring_is_positive == parent_is_positive) {
                    throw std::runtime_error(
                        "Created a ring with a parent having the same orientation (sign of area)");
                }
            }
            for (auto& c : op_j->ring->children) {
                double c_area = area(c);
                bool c_is_positive = c_area > 0.0;
                bool c_is_zero = value_is_zero(c_area);
                if (!c_is_zero && ring_is_positive == c_is_positive) {
                    throw std::runtime_error(
                        "Created a ring with a child having the same orientation (sign of area)");
                }
            }
        }
    }
    // LCOV_EXCL_STOP
#endif
}

template <typename T>
bool process_chains(std::size_t first_index,
                    std::size_t last_index,
                    std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                    ring_manager<T>& rings,
                    mapbox::geometry::point<T>& rewind_point) {
    bool rewind = false;
    for (std::size_t j = first_index; j <= last_index; ++j) {
        point_ptr<T> op_j = rings.all_points[j];
        if (!op_j->ring) {
            continue;
        }
        for (std::size_t k = j + 1; k <= last_index; ++k) {
            point_ptr<T> op_k = rings.all_points[k];
            if (!op_k->ring || !op_j->ring) {
                continue;
            }
            if (fix_intersects(dupe_ring, op_j, op_k, rings, rewind_point)) {
                rewind = true;
            }
        }
    }
    return rewind;
}

template <typename T>
bool process_collinear_edges(std::size_t first_index,
                             std::size_t last_index,
                             std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>>& dupe_ring,
                             ring_manager<T>& rings,
                             mapbox::geometry::point<T>& rewind_point) {
    bool rewind = false;
    for (std::size_t j = first_index; j <= last_index; ++j) {
        point_ptr<T> op_j = rings.all_points[j];
        if (!op_j->ring) {
            continue;
        }
        for (std::size_t k = j + 1; k <= last_index; ++k) {
            point_ptr<T> op_k = rings.all_points[k];
            if (!op_k->ring || !op_j->ring) {
                continue;
            }
            if (handle_collinear_edges(op_j, op_k, dupe_ring, rings, rewind_point)) {
                rewind = true;
            }
        }
    }
    return rewind;
}

template <typename T>
bool index_is_after_point(std::size_t const& i,
                          mapbox::geometry::point<T> const& pt,
                          ring_manager<T> const& rings) {
    if (i == 0) {
        return false;
    }

    if (rings.all_points[i]->y < pt.y) {
        return true;
    } else if (rings.all_points[i]->y > pt.y) {
        return false;
    } else if (rings.all_points[i]->x >= pt.x) {
        return true;
    } else {
        return false;
    }
}

template <typename T>
void rewind_to_point(std::size_t& i,
                     mapbox::geometry::point<T> const& pt,
                     ring_manager<T> const& rings) {
    if (i >= rings.rings.size()) {
        i = rings.rings.size() - 1;
    }
    while (index_is_after_point(i, pt, rings)) {
        --i;
    }
}

template <typename T>
void remove_spikes_in_polygons(ring_ptr<T> r, ring_manager<T>& rings) {

    point_ptr<T> first_point = r->points;
    remove_spikes(first_point);
    if (!first_point) {
        r->points = nullptr;
        r->area = std::numeric_limits<double>::quiet_NaN();
        remove_ring(r, rings);
        return;
    }
    point_ptr<T> p = first_point->next;
    while (p != first_point) {
        remove_spikes(p);
        if (!p) {
            r->points = nullptr;
            r->area = std::numeric_limits<double>::quiet_NaN();
            remove_ring(r, rings);
            return;
        }
        if (p->ring && !first_point->ring) {
            first_point = p;
        }
        p = p->next;
    }
}

template <typename T>
void fixup_out_polyline(ring<T>& ring, ring_manager<T>& rings) {
    point_ptr<T> pp = ring.points;
    point_ptr<T> lastPP = pp->prev;
    while (pp != lastPP) {
        pp = pp->next;
        if (*pp == *pp->prev) {
            if (pp == lastPP)
                lastPP = pp->prev;
            point_ptr<T> tmpPP = pp->prev;
            tmpPP->next = pp->next;
            pp->next->prev = tmpPP;
            // delete pp;
            pp->next = pp;
            pp->prev = pp;
            pp->ring = nullptr;
            pp = tmpPP;
        }
    }

    if (pp == pp->prev) {
        remove_ring(&ring, rings);
        dispose_out_points(pp);
        ring.points = nullptr;
        return;
    }
}


template <typename T>
void fixup_out_polygon(ring<T>& ring, ring_manager<T>& rings) {
    // FixupOutPolygon() - removes duplicate points and simplifies consecutive
    // parallel edges by removing the middle vertex.
    point_ptr<T> lastOK = nullptr;
    ring.bottom_point = nullptr;
    point_ptr<T> pp = ring.points;

    for (;;) {
        if (pp->prev == pp || pp->prev == pp->next) {
            // We now need to make sure any children rings to this are promoted and their hole
            // status is changed
            // promote_children_of_removed_ring(&ring, rings);
            remove_ring(&ring, rings);
            dispose_out_points(pp);
            ring.points = nullptr;
            return;
        }

        // test for duplicate points and collinear edges ...
        if ((*pp == *pp->next) || (*pp == *pp->prev) ||
            (slopes_equal(*pp->prev, *pp, *pp->next))) {
            lastOK = nullptr;
            point_ptr<T> tmp = pp;
            pp->prev->next = pp->next;
            pp->next->prev = pp->prev;
            pp = pp->prev;
            tmp->ring = nullptr;
            tmp->next = tmp;
            tmp->prev = tmp;
        } else if (pp == lastOK) {
            break;
        } else {
            if (!lastOK) {
                lastOK = pp;
            }
            pp = pp->next;
        }
    }
    ring.points = pp;
}


template <typename T>
void do_simple_polygons(ring_manager<T>& rings) {

    // fix orientations ...
    for (auto& r : rings.rings) {
        if (!r.points || r.is_open) {
            continue;
        }
        std::size_t s = 0;
        if (ring_is_hole(&r) == (area_from_point(r.points, s) > 0)) {
            reverse_ring(r.points);
        }
        remove_spikes_in_polygons(&r, rings);
        r.area = std::numeric_limits<double>::quiet_NaN();
    }

    std::stable_sort(rings.all_points.begin(), rings.all_points.end(), point_ptr_cmp<T>());
    std::unordered_multimap<ring_ptr<T>, point_ptr_pair<T>> dupe_ring;
    dupe_ring.reserve(rings.rings.size());

    // Find sets of repeated points and process them
    std::size_t count = 0;
    for (std::size_t i = 1; i < rings.all_points.size(); ++i) {
        if (*rings.all_points[i] == *rings.all_points[i - 1]) {
            ++count;
            if (i < (rings.all_points.size() - 1)) {
                continue;
            } else {
                ++i;
            }
        }

        if (count == 0) {
            continue;
        }
        std::size_t first_index = i - count - 1;
        std::size_t last_index = i - 1;
        auto sort_begin = rings.all_points.begin();
        std::advance(sort_begin, first_index);
        auto sort_end = rings.all_points.begin();
        std::advance(sort_end, i);
        std::stable_sort(sort_begin, sort_end, point_ptr_depth_cmp<T>());
        process_repeated_points(first_index, last_index, dupe_ring, rings);
        mapbox::geometry::point<T> rewind_point(std::numeric_limits<T>::min(),
                                                std::numeric_limits<T>::min());
        bool do_rewind = false;
        if (process_chains(first_index, last_index, dupe_ring, rings, rewind_point)) {
            do_rewind = true;
        }
        if (process_collinear_edges(first_index, last_index, dupe_ring, rings, rewind_point)) {
            do_rewind = true;
        }
        if (do_rewind) {
            rewind_to_point(i, rewind_point, rings);
        }
        count = 0;
    }

#if DEBUG
    // LCOV_EXCL_START
    for (auto& r : rings.rings) {
        if (!r.points || r.is_open) {
            continue;
        }
        double stored_area = area(&r);
        std::size_t s = 0;
        double calculated_area = area_from_point(r.points, s);
        if (!values_near_equal(stored_area, calculated_area)) {
            throw std::runtime_error("Difference in stored area vs calculated area!");
        }
    }
    // LCOV_EXCL_END
#endif

    for (auto& r : rings.rings) {
        if (!r.points || r.is_open) {
            continue;
        }
        fixup_out_polygon(r, rings);
        if (ring_is_hole(&r) == (area(&r) > 0.0)) {
            reverse_ring(r.points);
            r.area = std::numeric_limits<double>::quiet_NaN();
        }
    }
}
}
}
}
