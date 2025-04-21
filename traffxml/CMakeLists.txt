project(traffxml)

set(SRC
  traff_decoder.cpp
  traff_decoder.hpp
  traff_model.cpp
  traff_model.hpp
  traff_model_xml.cpp
  traff_model_xml.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  pugixml
)
