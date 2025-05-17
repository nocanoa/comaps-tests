#pragma once

#include "traffxml/traff_model.hpp"

#include "geometry/rect2d.hpp"

#include <string>
#include <vector>

namespace pugi
{
class xml_document;
class xml_node;
}  // namespace pugi

namespace traffxml
{
bool ParseTraff(pugi::xml_document const & document, TraffFeed & feed);

/**
 * @brief Generates a list of XML `filter` elements from a vector of rects representing bboxes.
 *
 * The resulting string can be placed inside a TraFF XML `filter_list` element or can be passed as
 * an extra to an Android intent.
 *
 * It will have one `filter` element for each element in `bboxRects`, although simplification may
 * be applied to reduce the number of rects (currently not implemented).
 *
 * The `min_road_class` attribute is not used.
 *
 * @param bboxRects The rectangles, each of which represents a bounding box.
 * @return A string of XML `filter` elements.
 */
std::string FiltersToXml(std::vector<m2::RectD> & bboxRects);
}  // namespace traffxml
