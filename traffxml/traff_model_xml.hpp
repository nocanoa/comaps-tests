#pragma once

#include "traffxml/traff_model.hpp"

namespace pugi
{
class xml_document;
class xml_node;
}  // namespace pugi

namespace traffxml
{
bool ParseTraff(pugi::xml_document const & document, TraffFeed & feed);
}  // namespace traffxml
