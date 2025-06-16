project(traffxml)

set(SRC
  traff_decoder.cpp
  traff_decoder.hpp
  traff_model.cpp
  traff_model.hpp
  traff_model_xml.cpp
  traff_model_xml.hpp
  traff_storage.cpp
  traff_storage.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  pugixml
  openlr
  coding
)

if (NOT SKIP_QT_GUI)
  omim_add_tool_subdirectory(traff_assessment_tool)
endif()
