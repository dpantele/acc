cmake_minimum_required(VERSION 3.2)
project(acc)

add_library(crag_compressed_word STATIC
    xy_letter.h
    compressed_word.h
    compressed_word.cpp
)

add_executable(crag.compressed_word.test_xy_letter test_xy_letter.cpp)
target_link_libraries(crag.compressed_word.test_xy_letter PRIVATE gtest_main)
add_test(
    NAME crag.compressed_word.test_xy_letter
    COMMAND crag.compressed_word.test_xy_letter
)


add_executable(crag.compressed_word.test_compressed_word test_compressed_word.cpp)
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



# add_executable(crag.compressed_word.test_xy_letter test_xy_letter.cpp)
