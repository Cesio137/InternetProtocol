cmake_minimum_required(VERSION 3.20)
project(InternetProtocol CXX)

set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE sources "${CMAKE_SOURCE_DIR}/include/*.hpp")
add_library(InternetProtocol INTERFACE ${sources})
target_include_directories(InternetProtocol INTERFACE "${CMAKE_SOURCE_DIR}/include")
add_subdirectory(thirdparty)
target_link_libraries(${CMAKE_PROJECT_NAME}
        INTERFACE
        asio
)
set_target_properties(InternetProtocol PROPERTIES LINKER_LANGUAGE CXX)

enable_testing()
add_executable(checksum test/test.cpp ${sources})
target_link_libraries(checksum PUBLIC InternetProtocol)
add_test(Checksum checksum)