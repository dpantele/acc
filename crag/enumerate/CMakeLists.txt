cmake_minimum_required(VERSION 3.2)

add_library(enumerate_normal_form
    normal_form.h
    normal_form.cpp
    canonical_word_mapping.h
    canonical_word_mapping.cpp
)

target_link_libraries(enumerate_normal_form PUBLIC crag_compressed_word)

add_executable(crag.enumerate.enumerate enumerate.cpp)
target_link_libraries(crag.enumerate.enumerate PRIVATE crag_compressed_word)

add_executable(crag.enumerate.simplify simplify_words.cpp)
target_link_libraries(crag.enumerate.simplify PRIVATE crag_compressed_word enumerate_normal_form  PRIVATE crag_compressed_word_tuple_normal_form)

hunter_add_package(Boost)
find_package(Boost CONFIG REQUIRED)

add_executable(crag.enumerate.analyze analyze.cpp)
target_link_libraries(crag.enumerate.analyze PRIVATE
    Boost::boost
    crag_compressed_word
    crag_compressed_word_longest_common_subword_cyclic
    crag_compressed_word_tuple_normal_form
    enumerate_normal_form)

add_executable(crag.orbits orbits.cpp)
target_link_libraries(crag.orbits PRIVATE
    crag_compressed_word
    crag_compressed_word_tuple_normal_form
)

add_executable(crag.enumerate.trivial_presentaions trivial_presentations.cpp)
target_link_libraries(crag.enumerate.trivial_presentaions PRIVATE
    crag_compressed_word
    crag_compressed_word_longest_common_subword_cyclic
    crag_compressed_word_tuple_normal_form
    crag_folded_graph
    crag_folded_graph_complete
    crag_folded_graph_harvest
)

