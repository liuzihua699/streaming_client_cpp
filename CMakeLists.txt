cmake_minimum_required(VERSION 3.10)
project(simple_player)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)

add_executable(simple_player
        src/main.cpp
)

target_include_directories(simple_player PRIVATE src)
target_link_libraries(simple_player OpenSSL::Crypto)