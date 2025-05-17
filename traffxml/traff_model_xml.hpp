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
/**
 * @brief Retrieves a TraFF feed from an XML document.
 *
 * The document must conform loosely to the TraFF specification (currently version 0.8).
 *
 * The name of the root element is not verified, but the `message` elements must be its immediate
 * children.
 *
 * Custom elements and attributes which are not part of the TraFF specification are ignored.
 *
 * Values which cannot be parsed correctly are skipped.
 *
 * Events whose event type does not match their event class are skipped.
 *
 * Messages, events, locations or points which lack mandatory information are skipped.
 *
 * If children are skipped but the parent remains valid, parsing it will report success.
 *
 * Parsing the feed will report failure if all its messages fail to parse, but not if it has no
 * messages.
 *
 * @param document The XML document from which to retrieve the messages.
 * @param feed Receives the TraFF feed.
 * @return `true` on success, `false` on failure.
 */
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
