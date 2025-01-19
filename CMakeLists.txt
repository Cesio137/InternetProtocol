#
# Copyright (c) 2023-2025 Nathan Miguel
#
# InternetProtocol is free library: you can redistribute it and/or modify it under the terms
# of the GNU Affero General Public License as published by the Free Software Foundation,
# version 3.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Affero General Public License for more details.
#
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#

cmake_minimum_required(VERSION 3.20)
project(InternetProtocol CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE sources "${CMAKE_SOURCE_DIR}/include/*.hpp")
add_library(InternetProtocol INTERFACE ${sources})
target_include_directories(InternetProtocol INTERFACE "${CMAKE_SOURCE_DIR}/include")
add_subdirectory(thirdparty)
target_link_libraries(${CMAKE_PROJECT_NAME}
        INTERFACE
        asio
        $<TARGET_NAME_IF_EXISTS:OpenSSL::SSL>
        $<TARGET_NAME_IF_EXISTS:OpenSSL::Crypto>
)
set_target_properties(InternetProtocol PROPERTIES LINKER_LANGUAGE CXX)

enable_testing()
add_executable(checksum test/test.cpp ${sources})
target_include_directories(checksum PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(checksum 
        PUBLIC 
        InternetProtocol
)
add_test(Checksum checksum)