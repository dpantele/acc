cmake_minimum_required(VERSION 3.2)

add_library(disjoint_subsets INTERFACE)

#target_sources(disjoint_subsets PRIVATE disjoint_subsets.h)

add_executable(crag.disjoint_subsets.test_disjoint_subsets test_disjoint_subsets.cpp)
target_link_libraries(crag.disjoint_subsets.test_disjoint_subsets gtest_main)


