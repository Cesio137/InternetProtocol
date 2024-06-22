set(Asio_DIR ${CMAKE_CURRENT_LIST_DIR})

project(asio)
file(GLOB_RECURSE Asio_Sources "${Asio_DIR}/include/*.h")
add_library(asio INTERFACE ${Asio_Sources})
target_include_directories(asio
        INTERFACE
        "${Asio_DIR}/include"
)

set_target_properties(asio PROPERTIES FOLDER "Thirdparty")