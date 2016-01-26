cmake_minimum_required(VERSION 3.2)

add_library(crag_compressed_word STATIC
    compressed_word.h
    compressed_word.cpp
    enumerate_words.h
    endomorphism.h
    xy_letter.h)

target_compile_features(crag_compressed_word PUBLIC cxx_relaxed_constexpr)

add_executable(crag.compressed_word.test_xy_letter test_xy_letter.cpp)
target_link_libraries(crag.compressed_word.test_xy_letter PRIVATE gtest_main)
add_test(
    NAME crag.compressed_word.test_xy_letter
    COMMAND crag.compressed_word.test_xy_letter
)


add_executable(crag.compressed_word.test_compressed_word test_compressed_word.cpp tuple_normal_form.cpp tuple_normal_form.h)
target_link_libraries(crag.compressed_word.test_compressed_word PRIVATE gtest_main crag_compressed_word)
add_test(
    NAME crag.compressed_word.test_compressed_word
    COMMAND crag.compressed_word.test_compressed_word
)

add_library(crag_compressed_word_longest_common_subword_cyclic STATIC
  longest_common_subword_cyclic.h
  longest_common_subword_cyclic.cpp
)

target_link_libraries(crag_compressed_word_longest_common_subword_cyclic PUBLIC crag_compressed_word)

add_executable(crag.compressed_word.test_longest_common_subword_cyclic test_longest_common_subword_cyclic.cpp)
target_link_libraries(crag.compressed_word.test_longest_common_subword_cyclic PRIVATE
    gtest_main
    crag_compressed_word_longest_common_subword_cyclic
    )

add_test(
    NAME crag.compressed_word.test_longest_common_subword_cyclic
    COMMAND crag.compressed_word.test_longest_common_subword_cyclic
)

add_library(crag_compressed_word_tuple_normal_form STATIC tuple_normal_form.cpp tuple_normal_form.h)

hunter_add_package(Boost)
find_package(Boost CONFIG REQUIRED)

target_link_libraries(crag_compressed_word_tuple_normal_form
    PUBLIC
    Boost::boost

    crag_compressed_word)

add_executable(crag.compressed_word.test_tuple_normal_form test_tuple_normal_form.cpp)

target_link_libraries(crag.compressed_word.test_tuple_normal_form
    PRIVATE crag_compressed_word_tuple_normal_form gtest_main)

add_executable(crag.compressed_word.profile_tuple_normal_form profile_tuple_normal_form.cpp)
target_link_libraries(crag.compressed_word.profile_tuple_normal_form
    PRIVATE crag_compressed_word_tuple_normal_form)

# add_executable(crag.compressed_word.test_xy_letter test_xy_letter.cpp)
