cmake_minimum_required(VERSION 3.2)
project(acc)

add_library(crag_folded_graph_modulus STATIC
    modulus.cpp
    )

add_library(crag_folded_graph STATIC
    folded_graph.h
    folded_graph.cpp )

target_compile_features(crag_folded_graph PUBLIC cxx_decltype_auto)
target_link_libraries(crag_folded_graph PUBLIC crag_compressed_word crag_folded_graph_modulus)


add_executable(crag.folded_graph.test_folded_graph test_folded_graph.cpp)
target_link_libraries(crag.folded_graph.test_folded_graph PRIVATE gtest_main crag_folded_graph)
add_test(
    NAME crag.folded_graph.test_folded_graph
    COMMAND crag.folded_graph.test_folded_graph
)
