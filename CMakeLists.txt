cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(Sendrecv CXX)

# Enable C++ 11
set(CMAKE_CXX_STANDARD 11)

# Add src to include directory
include_directories("src")
include_directories(".")

# Link with thread library
set(THREADS_PREFER_PTHREAD_FLAG ON)
SET(CMAKE_BUILD_TYPE Debug)
find_package(Threads REQUIRED)
link_libraries(Threads::Threads ibverbs rdmacm)

# Include all header files under src/common/
file(GLOB COMMON_FILES "src/common/*.cpp" "src/common/*.h" "src/common/*.hpp")
file(GLOB TESTS_FILES "tests/test_*.cpp" "tests/*.h" "tests/*.hpp")
file(GLOB SRC_FILES "src/*.cpp" "src/*.h" "src/*.hpp" "src/rdma/*.cpp" "src/rdma/*.h" "src/rdma/*.hpp")
file(GLOB NONBLOCK_FILES "src/nonblocking/*.cpp" "src/nonblocking/*.h")
#file(GLOB PINGPONG "tests/pingpong_socket.cpp" "tests/*.h")

set(SOURCE_FILES
    ${COMMON_FILES}
    ${TESTS_FILES}
    ${SRC_FILES})

set(ISEND_SOURCE_FILES
        ${COMMON_FILES}
        ${SRC_FILES}
        ${NONBLOCK_FILES})

add_executable(samplesort ${ISEND_SOURCE_FILES} src/samplesort/samplesort_sendrecv.cpp)
#add_executable(test_socket_isendrecv ${ISEND_SOURCE_FILES} tests_nonblocking/test_socket_isendrecv.cpp)
#add_executable(test_rdma_isendrecv ${ISEND_SOURCE_FILES} tests_nonblocking/test_rdma_isendrecv.cpp)
#add_executable(socket_isendrecv_speed ${ISEND_SOURCE_FILES} tests_nonblocking/socket_isendrecv_speed.cpp)
#add_executable(rdma_isendrecv_speed ${ISEND_SOURCE_FILES} tests_nonblocking/rdma_isendrecv_speed.cpp)
#add_executable(socket_speed_single ${ISEND_SOURCE_FILES} tests_nonblocking/socket_speed_single.cpp)
#add_executable(rdma_speed_single ${ISEND_SOURCE_FILES} tests_nonblocking/rdma_speed_single.cpp)
#add_executable(Sendrecv ${SOURCE_FILES}  tests/driver.cpp)
#add_executable(pingpong_socket ${SOURCE_FILES} tests/pingpong_socket.cpp)
#add_executable(pingpong_rdma ${SOURCE_FILES} tests/pingpong_rdma.cpp)
#add_executable(pingpong_rdma_async_many ${SOURCE_FILES} tests/pingpong_rdma_async_many.cpp)
