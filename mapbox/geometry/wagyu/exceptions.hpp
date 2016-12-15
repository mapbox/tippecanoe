#pragma once

#include <stdexcept>

namespace mapbox {
namespace geometry {
namespace wagyu {
class clipper_exception : public std::exception {
private:
    std::string m_descr;

public:
    clipper_exception(const char* description) : m_descr(description) {
    }
    virtual ~clipper_exception() noexcept {
    }
    virtual const char* what() const noexcept {
        return m_descr.c_str();
    }
};
}
}
}
