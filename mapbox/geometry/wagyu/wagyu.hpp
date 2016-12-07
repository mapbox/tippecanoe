#pragma once

#include <list>

#include <mapbox/geometry/box.hpp>
#include <mapbox/geometry/line_string.hpp>
#include <mapbox/geometry/multi_polygon.hpp>
#include <mapbox/geometry/polygon.hpp>

#include <mapbox/geometry/wagyu/build_result.hpp>
#include <mapbox/geometry/wagyu/build_local_minima_list.hpp>
#include <mapbox/geometry/wagyu/config.hpp>
#include <mapbox/geometry/wagyu/local_minimum.hpp>
#include <mapbox/geometry/wagyu/snap_rounding.hpp>
#include <mapbox/geometry/wagyu/topology_correction.hpp>
#include <mapbox/geometry/wagyu/vatti.hpp>

namespace mapbox {
namespace geometry {
namespace wagyu {

template <typename T>
class wagyu {
private:
    using value_type = T;

    local_minimum_list<value_type> minima_list;
    bool has_open_paths;
    bool reverse_output;

    wagyu( wagyu const& ) = delete;
    wagyu& operator=(wagyu const& ) = delete;

public:
    wagyu() : minima_list(), has_open_paths(false), reverse_output(false) {
    }

    ~wagyu() {
        clear();
    }

    bool add_line(mapbox::geometry::line_string<value_type> const& pg) {
        bool success = add_line_string(pg, minima_list);
        if (success) {
            has_open_paths = true;
        }
        return success;
    }

    bool add_ring(mapbox::geometry::linear_ring<value_type> const& pg,
                  polygon_type p_type = polygon_type_subject) {
        return add_linear_ring(pg, minima_list, p_type);
    }

    bool add_polygon(mapbox::geometry::polygon<value_type> const& ppg,
                     polygon_type p_type = polygon_type_subject) {
        bool result = false;
        for (auto const& r : ppg) {
            if (add_ring(r, p_type)) {
                result = true;
            }
        }
        return result;
    }

    void reverse_rings(bool value) {
        reverse_output = value;
    }

    void clear() {
        minima_list.clear();
        has_open_paths = false;
    }

    mapbox::geometry::box<value_type> get_bounds() {
        mapbox::geometry::point<value_type> min = { 0, 0 };
        mapbox::geometry::point<value_type> max = { 0, 0 };
        if (minima_list.empty()) {
            return mapbox::geometry::box<value_type>(min, max);
        }
        bool first_set = false;
        for (auto const& lm : minima_list) {
            if (!lm.left_bound.edges.empty()) {
                if (!first_set) {
                    min = lm.left_bound.edges.front().top;
                    max = lm.left_bound.edges.back().bot;
                    first_set = true;
                } else {
                    min.y = std::min(min.y, lm.left_bound.edges.front().top.y);
                    max.y = std::max(max.y, lm.left_bound.edges.back().bot.y);
                    max.x = std::max(max.x, lm.left_bound.edges.back().top.x);
                    min.x = std::min(min.x, lm.left_bound.edges.back().top.x);
                }
                for (auto const& e : lm.left_bound.edges) {
                    max.x = std::max(max.x, e.bot.x);
                    min.x = std::min(min.x, e.bot.x);
                }
            }
            if (!lm.right_bound.edges.empty()) {
                if (!first_set) {
                    min = lm.right_bound.edges.front().top;
                    max = lm.right_bound.edges.back().bot;
                    first_set = true;
                } else {
                    min.y = std::min(min.y, lm.right_bound.edges.front().top.y);
                    max.y = std::max(max.y, lm.right_bound.edges.back().bot.y);
                    max.x = std::max(max.x, lm.right_bound.edges.back().top.x);
                    min.x = std::min(min.x, lm.right_bound.edges.back().top.x);
                }
                for (auto const& e : lm.right_bound.edges) {
                    max.x = std::max(max.x, e.bot.x);
                    min.x = std::min(min.x, e.bot.x);
                }
            }
        }
        return mapbox::geometry::box<value_type>(min, max);
    }

    bool execute(clip_type cliptype,
                 mapbox::geometry::multi_polygon<value_type>& solution,
                 fill_type subject_fill_type,
                 fill_type clip_fill_type) {

        ring_manager<T> rings;
        
        build_hot_pixels(minima_list, rings);

        if (!execute_vatti(minima_list, rings, cliptype, subject_fill_type, clip_fill_type)) {
            return false;
        }
        
        do_simple_polygons(rings);

        build_result(solution, rings, reverse_output);

        return true;
    }
};
}
}
}
