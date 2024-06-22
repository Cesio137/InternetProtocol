cmake_minimum_required(VERSION 3.25)
project(InternetProtocol)

set(CMAKE_CXX_STANDARD 23)
file(GLOB_RECURSE sources "${CMAKE_SOURCE_DIR}/include/*.h")
add_executable(InternetProtocol main.cpp ${sources})
target_include_directories(InternetProtocol PUBLIC "${CMAKE_SOURCE_DIR}/include")
add_subdirectory(thirdparty)
target_link_libraries(${CMAKE_PROJECT_NAME}
        PUBLIC
        asio
        Simdjson
)