
message(STATUS "starting build: base")
file(GLOB_RECURSE BASE_SRC "*.cpp" "*.h" "*.hpp")

add_library(base STATIC ${BASE_SRC})

target_link_libraries(base PUBLIC external)

set_target_properties(base PROPERTIES LINKER_LANGUAGE CXX)