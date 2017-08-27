#include "evaluator.hpp"
#include "mapbox/geometry/feature.hpp"
#include "mvt.hpp"

Evaluator::Evaluator(const mvt_layer& layer, const mvt_feature& feature)
  : m_layer(layer)
  , m_feature(feature) {

  }

std::experimental::optional<mapbox::geometry::value> Evaluator::operator()(const std::string& key) const
{
  // find the property in our layer's key map.
  // if the layer doesn't have that property, return an
  // empty optional.
  const auto it = m_layer.key_map.find(key);
  if (it == m_layer.key_map.end()) {
    return {};
  }

  // if the layer does contain the property,
  // we need to see if the feature itself has the property.
  // look through the feature's tags to see if it contains
  // the right index.
  const size_t size_max = (size_t) - 1; // max value of size_t
  size_t tidx = size_max;
  for (size_t i = 0; i + 1 < m_feature.tags.size(); i+= 2) {
    if (m_feature.tags[i] == it->second) {
      tidx = i;
      break;
    }
  }

  if (tidx == size_max) {
    return {};
  }

  // now we know that the feature has the property,
  // get the value.
  const mvt_value& value = m_layer.values[m_feature.tags[tidx + 1]];

  switch (value.type) {
    case mvt_value_type::mvt_string:
      return std::experimental::optional<mapbox::geometry::value>(value.string_value);
    case mvt_value_type::mvt_float:
      return std::experimental::optional<mapbox::geometry::value>((double)value.numeric_value.float_value);
    case mvt_value_type::mvt_double:
      return std::experimental::optional<mapbox::geometry::value>(value.numeric_value.double_value);
    case mvt_value_type::mvt_int:
      return std::experimental::optional<mapbox::geometry::value>(value.numeric_value.int_value);
    case mvt_value_type::mvt_uint:
      return std::experimental::optional<mapbox::geometry::value>(value.numeric_value.uint_value);
    case mvt_value_type::mvt_sint:
      return std::experimental::optional<mapbox::geometry::value>(value.numeric_value.sint_value);
    case mvt_value_type::mvt_bool:
      return std::experimental::optional<mapbox::geometry::value>(value.numeric_value.bool_value);
    default:
      return {};
  }
}