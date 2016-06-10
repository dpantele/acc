cmake_minimum_required(VERSION 3.2)

add_library(crag_multithreading
    STATIC
    sem.h
    SharedQueue.h SharedQueue.cpp)

find_package(Threads)

target_link_libraries(crag_multithreading
  PUBLIC
    Threads::Threads
)

add_executable(crag.multithreading.test_shared_queue test_shared_queue.cpp)
add_test(
    NAME crag.multithreading.test_shared_queue
    COMMAND crag.multithreading.test_shared_queue
)

target_link_libraries(crag.multithreading.test_shared_queue PUBLIC crag_multithreading)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_compile_options(crag.multithreading.test_shared_queue PRIVATE $<$<CONFIG:Debug>:-fno-sanitize=address -fsanitize=thread>)
  set_target_properties(crag.multithreading.test_shared_queue PROPERTIES LINK_FLAGS_DEBUG "-fno-sanitize=address -fsanitize=thread")
endif()

