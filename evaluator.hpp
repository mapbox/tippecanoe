#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP

#include <experimental/optional>

struct mvt_layer;
struct mvt_feature;

namespace mapbox {
namespace geometry {

struct value;

}
}

//  TODO:
// - we need to match up a filter.key (string?)
//   into a property. in order to do this, can we use the key_map
//   in the mvt_tile?
//    

// since we're dealing with mvt_tiles and
// mvt_features, wrap them in an evaluator
// class that can be used for the filter_evaluator
class Evaluator {
public:
  Evaluator(const mvt_layer& layer, const mvt_feature& feature);

  std::experimental::optional<mapbox::geometry::value> operator()(const std::string& key) const;

private:
  const mvt_layer& m_layer;
  const mvt_feature& m_feature;
};

#endif