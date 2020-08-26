#pragma once

/**
 * To enable this by the program, define USE_WAGYU_INTERRUPT before including wagyu.hpp
 * To request an interruption, call `interrupt_request()`. As soon as Wagyu detects the request
 * it will raise an exception (`std::runtime_error`).
 */

#ifdef USE_WAGYU_INTERRUPT

namespace {
thread_local bool WAGYU_INTERRUPT_REQUESTED = false;
}

namespace mapbox {
namespace geometry {
namespace wagyu {

static void interrupt_reset(void) {
    WAGYU_INTERRUPT_REQUESTED = false;
}

static void interrupt_request(void) {
    WAGYU_INTERRUPT_REQUESTED = true;
}

static void interrupt_check(void) {
    if (WAGYU_INTERRUPT_REQUESTED) {
        interrupt_reset();
        throw std::runtime_error("Wagyu interrupted");
    }
}
} // namespace wagyu
} // namespace geometry
} // namespace mapbox

#else /* ! USE_WAGYU_INTERRUPT */

namespace mapbox {
namespace geometry {
namespace wagyu {

static void interrupt_check(void) {
}

} // namespace wagyu
} // namespace geometry
} // namespace mapbox

#endif /* USE_WAGYU_INTERRUPT */
