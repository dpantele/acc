cmake_minimum_required(VERSION 3.2)

add_executable(crag.enumerate.enumerate enumerate.cpp)
target_link_libraries(crag.enumerate.enumerate PRIVATE crag_compressed_word)

add_executable(crag.enumerate.simplify simplify_words.cpp)
target_link_libraries(crag.enumerate.simplify PRIVATE crag_compressed_word)


add_executable(crag.enumerate.analyze analyze.cpp)
target_link_libraries(crag.enumerate.analyze PRIVATE crag_compressed_word)
