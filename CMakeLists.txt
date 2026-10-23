cmake_minimum_required(VERSION 3.16)
project(JBank VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core banking library
add_library(jbank_core STATIC
    src/Money.cpp
    src/Accounts.cpp
    src/Investment.cpp
    src/JBank.cpp
)
target_include_directories(jbank_core PUBLIC include)

# Test binary
add_executable(jbank_tests tests/test_jbank.cpp)
target_link_libraries(jbank_tests PRIVATE jbank_core)

# Main demo binary
add_executable(jbank_app src/main.cpp)
target_link_libraries(jbank_app PRIVATE jbank_core)
