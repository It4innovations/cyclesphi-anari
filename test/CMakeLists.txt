## Copyright 2022 Jefferson Amstutz
## SPDX-License-Identifier: Apache-2.0

project(anari_cycles_test LANGUAGES CXX)

find_package(anari 0.7.0 REQUIRED)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari)

