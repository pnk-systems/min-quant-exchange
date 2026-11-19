cmake_minimum_required(VERSION 3.20)
project(MiniQuantExchange CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

option(MINIQ_ENABLE_SANITIZERS "Build with ASan/UBSan" OFF)
if(MINIQ_ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(miniq_core
  src/order.cpp
  src/risk.cpp
  src/order_book.cpp
  src/matching_engine.cpp
  src/event_bus.cpp
  src/position_service.cpp
  src/strategy.cpp
  src/exchange_service.cpp
)
target_include_directories(miniq_core PUBLIC include)

add_executable(miniq_demo src/main.cpp)
target_link_libraries(miniq_demo PRIVATE miniq_core)

# ---------------------------------------------------------------------------
# Playground executables — small standalone files demonstrating C++ concepts
# that would otherwise be forced into the trading domain code.
# ---------------------------------------------------------------------------
foreach(playground_file
    cpp_fundamentals
    copy_move_raii
    polymorphism
    templates_stl
    concurrency)
  add_executable(playground_${playground_file} playground/${playground_file}.cpp)
  target_include_directories(playground_${playground_file} PRIVATE include)
endforeach()
find_package(Threads REQUIRED)
target_link_libraries(playground_concurrency PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(MINIQ_BUILD_TESTS "Build tests" ON)
if(MINIQ_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
  )
  FetchContent_MakeAvailable(catch2)

  add_executable(miniq_tests
    tests/test_main.cpp
    tests/order_book_tests.cpp
    tests/risk_tests.cpp
    tests/exchange_tests.cpp
  )
  target_link_libraries(miniq_tests PRIVATE miniq_core Catch2::Catch2WithMain)

  include(CTest)
  include(Catch)
  catch_discover_tests(miniq_tests)
endif()
