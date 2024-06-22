set(Simdjson_DIR ${CMAKE_CURRENT_LIST_DIR})

project(Simdjson)
file(GLOB_RECURSE Simdjson_Sources "${Simdjson_DIR}/include/*.h" "${Simdjson_DIR}/src/*.cpp")
add_library(Simdjson INTERFACE ${Simdjson_Sources})
target_include_directories(Simdjson
        INTERFACE
        "${Simdjson_DIR}/include"
)

set_target_properties(Simdjson PROPERTIES FOLDER "Thirdparty")