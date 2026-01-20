cmake_minimum_required(VERSION 3.10)
project(simple_player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)

file(GLOB_RECURSE SRCS "src/*.cpp")
add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME}
    OpenSSL::Crypto
    pthread
)
