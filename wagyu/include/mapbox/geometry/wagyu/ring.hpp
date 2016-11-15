#pragma once

#include <assert.h>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include <mapbox/geometry/wagyu/point.hpp>

#ifdef DEBUG
#include <execinfo.h>
#include <iostream>
#include <stdio.h>
//
// void* callstack[128];
// int i, frames = backtrace(callstack, 128);
// char** strs = backtrace_symbols(callstack, frames);
// for (i = 0; i < frames; ++i) {
//     printf("%s\n", strs[i]);
// }
// free(strs);
#endif

namespace mapbox {
namespace geometry {
namespace wagyu {

// NOTE: ring and ring_ptr are forward declared in wagyu/point.hpp

template <typename T>
using ring_vector = std::vector<ring_ptr<T>>;

template <typename T>
using ring_list = std::list<ring_ptr<T>>;

template <typename T>
struct ring {
    std::size_t ring_index; // To support unset 0 is undefined and indexes offset by 1
    std::size_t size;
    double area;
    ring_ptr<T> parent;
    ring_list<T> children;
    point_ptr<T> points;
    point_ptr<T> bottom_point;
    bool is_open;

    ring( ring const& ) = delete;
    ring& operator=(ring const& ) = delete;

    ring()
        : ring_index(0),
          size(0),
          area(std::numeric_limits<double>::quiet_NaN()),
          parent(nullptr),
          children(),
          points(nullptr),
          bottom_point(nullptr),
          is_open(false) {
    }
};

template <typename T>
using hot_pixel_vector = std::vector<mapbox::geometry::point<T>>;

template <typename T>
using hot_pixel_itr = typename hot_pixel_vector<T>::iterator;

template <typename T>
using hot_pixel_rev_itr = typename hot_pixel_vector<T>::reverse_iterator;

template <typename T>
struct ring_manager {
    
    ring_list<T> children;
    std::vector<point_ptr<T>> all_points;
    hot_pixel_vector<T> hot_pixels;
    hot_pixel_itr<T> current_hp_itr;
    std::deque<point<T>> points;
    std::deque<ring<T>> rings;
    std::vector<point<T>> storage;
    std::size_t index;

    ring_manager( ring_manager const& ) = delete;
    ring_manager& operator=(ring_manager const& ) = delete;

    ring_manager()
        : children(),
          all_points(),
          hot_pixels(),
          current_hp_itr(hot_pixels.end()),
          points(),
          rings(),
          storage(),
          index(0) {
    }
};

template <typename T>
void preallocate_point_memory(ring_manager<T>& rings, std::size_t size) {
    rings.storage.reserve(size);
    rings.all_points.reserve(size);
}

template <typename T>
ring_ptr<T> create_new_ring(ring_manager<T>& rings) {
    rings.rings.emplace_back();
    ring_ptr<T> result = &rings.rings.back();
    result->ring_index = rings.index++;
    return result;
}

template <typename T>
point_ptr<T>
create_new_point(ring_ptr<T> r, mapbox::geometry::point<T> const& pt, ring_manager<T>& rings) {
    point_ptr<T> point;
    if (rings.storage.size() < rings.storage.capacity()) {
        rings.storage.emplace_back(r, pt);
        point = &rings.storage.back();
    } else {
        rings.points.emplace_back(r, pt);
        point = &rings.points.back();
    }
    rings.all_points.push_back(point);
    return point;
}

template <typename T>
point_ptr<T> create_new_point(ring_ptr<T> r,
                              mapbox::geometry::point<T> const& pt,
                              point_ptr<T> before_this_point,
                              ring_manager<T>& rings) {
    point_ptr<T> point;
    if (rings.storage.size() < rings.storage.capacity()) {
        rings.storage.emplace_back(r, pt, before_this_point);
        point = &rings.storage.back();
    } else {
        rings.points.emplace_back(r, pt, before_this_point);
        point = &rings.points.back();
    }
    rings.all_points.push_back(point);
    return point;
}

template <typename T>
void ring1_child_of_ring2(ring_ptr<T> ring1, ring_ptr<T> ring2, ring_manager<T>& manager) {
    assert(ring1 != ring2);
    if (ring1->parent == ring2) {
        return;
    }
    if (ring1->parent == nullptr) {
        manager.children.remove(ring1);
    } else {
        ring1->parent->children.remove(ring1);
    }
    if (ring2 == nullptr) {
        ring1->parent = nullptr;
        manager.children.push_back(ring1);
    } else {
        ring1->parent = ring2;
        ring2->children.push_back(ring1);
    }
}

template <typename T>
void ring1_sibling_of_ring2(ring_ptr<T> ring1, ring_ptr<T> ring2, ring_manager<T>& manager) {
    assert(ring1 != ring2);
    if (ring1->parent == ring2->parent) {
        return;
    }
    if (ring1->parent == nullptr) {
        manager.children.remove(ring1);
    } else {
        ring1->parent->children.remove(ring1);
    }
    if (ring2->parent == nullptr) {
        manager.children.push_back(ring1);
    } else {
        ring2->parent->children.push_back(ring1);
    }
    ring1->parent = ring2->parent;
}

template <typename T>
void ring1_replaces_ring2(ring_ptr<T> ring1, ring_ptr<T> ring2, ring_manager<T>& manager) {
    assert(ring1 != ring2);
    if (ring2->parent == nullptr) {
        manager.children.remove(ring2);
    } else {
        ring2->parent->children.remove(ring2);
    }
    for (auto& c : ring2->children) {
        c->parent = ring1;
    }
    if (ring1 == nullptr) {
        manager.children.splice(manager.children.end(), ring2->children);
    } else {
        ring1->children.splice(ring1->children.end(), ring2->children);
    }
    ring2->parent = nullptr;
}

template <typename T>
void remove_ring(ring_ptr<T> r, ring_manager<T>& manager) {
    if (r->parent == nullptr) {
        manager.children.remove(r);
        for (auto& c : r->children) {
            c->parent = nullptr;
        }
        manager.children.splice(manager.children.end(), r->children);
    } else {
        r->parent->children.remove(r);
        for (auto& c : r->children) {
            c->parent = r->parent;
        }
        r->parent->children.splice(r->parent->children.end(), r->children);
        r->parent = nullptr;
    }
}

template <typename T>
inline std::size_t ring_depth(ring_ptr<T> r) {
    std::size_t depth = 0;
    if (!r) {
        return depth;
    }
    while (r->parent) {
        depth++;
        r = r->parent;
    }
    return depth;
}

template <typename T>
inline bool ring_is_hole(ring_ptr<T> r) {
    return ring_depth(r) & 1;
}

template <typename T>
void set_next(const_point_ptr<T>& node, const const_point_ptr<T>& next_node) {
    node->next = next_node;
}

template <typename T>
point_ptr<T> get_next(const_point_ptr<T>& node) {
    return node->next;
}

template <typename T>
point_ptr<T> get_prev(const_point_ptr<T>& node) {
    return node->prev;
}

template <typename T>
void set_prev(const_point_ptr<T>& node, const const_point_ptr<T>& prev_node) {
    node->prev = prev_node;
}

template <typename T>
void init(const_point_ptr<T>& node) {
    set_next(node, node);
    set_prev(node, node);
}

template <typename T>
std::size_t point_count(const const_point_ptr<T>& orig_node) {
    std::size_t size = 0;
    point_ptr<T> n = orig_node;
    do {
        n = get_next(n);
        ++size;
    } while (n != orig_node);
    return size;
}

template <typename T>
void link_before(point_ptr<T>& node, point_ptr<T>& new_node) {
    point_ptr<T> prev_node = get_prev(node);
    set_prev(new_node, prev_node);
    set_next(new_node, node);
    set_prev(node, new_node);
    set_next(prev_node, new_node);
}

template <typename T>
void link_after(point_ptr<T>& node, point_ptr<T>& new_node) {
    point_ptr<T> next_node = get_next(node);
    set_prev(new_node, node);
    set_next(new_node, next_node);
    set_next(node, new_node);
    set_prev(next_node, new_node);
}

template <typename T>
void transfer_point(point_ptr<T>& p, point_ptr<T>& b, point_ptr<T>& e) {
    if (b != e) {
        point_ptr<T> prev_p = get_prev(p);
        point_ptr<T> prev_b = get_prev(b);
        point_ptr<T> prev_e = get_prev(e);
        set_next(prev_e, p);
        set_prev(p, prev_e);
        set_next(prev_b, e);
        set_prev(e, prev_b);
        set_next(prev_p, b);
        set_prev(b, prev_p);
    } else {
        link_before(p, b);
    }
}

template <typename T>
void reverse_ring(point_ptr<T> pp) {
    if (!pp) {
        return;
    }
    point_ptr<T> pp1;
    point_ptr<T> pp2;
    pp1 = pp;
    do {
        pp2 = pp1->next;
        pp1->next = pp1->prev;
        pp1->prev = pp2;
        pp1 = pp2;
    } while (pp1 != pp);
}

template <typename T>
double area_from_point(point_ptr<T> op, std::size_t & size) {
    point_ptr<T> startOp = op;
    size = 1;
    if (!op) {
        return 0.0;
    }
    double a = 0.0;
    do {
        ++size;
        a += static_cast<double>(op->prev->x + op->x) * static_cast<double>(op->prev->y - op->y);
        op = op->next;
    } while (op != startOp);
    return a * 0.5;
}

template <typename T>
double area(ring_ptr<T> r) {
    assert(r != nullptr);
    if (std::isnan(r->area)) {
        r->area = area_from_point(r->points, r->size);
    }
    return r->area;
}

#ifdef DEBUG

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const ring<T>& r) {
    out << "  ring_index: " << r.ring_index << std::endl;
    if (!r.parent) {
        // out << "  parent_ring ptr: nullptr" << std::endl;
        out << "  parent_index: -----" << std::endl;
    } else {
        // out << "  parent_ring ptr: " << r.parent << std::endl;
        out << "  parent_ring idx: " << r.parent->ring_index << std::endl;
    }
    ring_ptr<T> n = const_cast<ring_ptr<T>>(&r);
    if (ring_is_hole(n)) {
        out << "  is_hole: true " << std::endl;
    } else {
        out << "  is_hole: false " << std::endl;
    }
    auto pt_itr = r.points;
    if (pt_itr) {
        out << "  area: " << r.area << std::endl;
        out << "  points:" << std::endl;
        out << "      [[[" << pt_itr->x << "," << pt_itr->y << "],";
        pt_itr = pt_itr->next;
        while (pt_itr != r.points) {
            out << "[" << pt_itr->x << "," << pt_itr->y << "],";
            pt_itr = pt_itr->next;
        }
        out << "[" << pt_itr->x << "," << pt_itr->y << "]]]" << std::endl;
    } else {
        out << "  area: NONE" << std::endl;
        out << "  points: NONE" << std::endl;
    }
    return out;
}

template <typename T>
std::string output_as_polygon(ring_ptr<T> r) {
    std::ostringstream out;

    auto pt_itr = r->points;
    if (pt_itr) {
        out << "[";
        out << "[[" << pt_itr->x << "," << pt_itr->y << "],";
        pt_itr = pt_itr->next;
        while (pt_itr != r->points) {
            out << "[" << pt_itr->x << "," << pt_itr->y << "],";
            pt_itr = pt_itr->next;
        }
        out << "[" << pt_itr->x << "," << pt_itr->y << "]]";
        for (auto const& c : r->children) {
            pt_itr = c->points;
            if (pt_itr) {
                out << ",[[" << pt_itr->x << "," << pt_itr->y << "],";
                pt_itr = pt_itr->next;
                while (pt_itr != c->points) {
                    out << "[" << pt_itr->x << "," << pt_itr->y << "],";
                    pt_itr = pt_itr->next;
                }
                out << "[" << pt_itr->x << "," << pt_itr->y << "]]";
            }
        }
        out << "]" << std::endl;
    } else {
        out << "[]" << std::endl;
    }

    return out.str();
}

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const ring_list<T>& rings) {
    out << "START RING LIST" << std::endl;
    for (auto& r : rings) {
        out << " ring: " << r->ring_index << " - " << r << std::endl;
        out << *r;
    }
    out << "END RING LIST" << std::endl;
    return out;
}

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const ring_vector<T>& rings) {
    out << "START RING VECTOR" << std::endl;
    for (auto& r : rings) {
        if (!r->points) {
            continue;
        }
        out << " ring: " << r->ring_index << " - " << r << std::endl;
        out << *r;
    }
    out << "END RING VECTOR" << std::endl;
    return out;
}

template <class charT, class traits, typename T>
inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out,
                                                     const hot_pixel_vector<T>& hp_vec) {
    out << "Hot Pixels: " << std::endl;
    for (auto& hp : hp_vec) {
        out << hp << std::endl;
    }
    return out;
}
#endif
}
}
}
