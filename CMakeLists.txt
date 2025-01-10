cmake_minimum_required(VERSION 3.5.0)
project(ViditelPlayground VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)


# set(Boost_USE_STATIC_LIBS ON)
# set(Boost_USET_MULTITHREADED ON)
# set(Boost_USE_STATIC_RUNTIME ON)
set(Boost_NO_SYSTEM_PATHS TRUE)

if(Boost_NO_SYSTEM_PATHS)
    set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../boostSRC/boost_1_85_0")
    set(BOOST_INCLUDE_DIRS "${BOOST_ROOT}")
    set(BOOST_LIBRARY_DIRS "${BOOST_ROOT}/stage/x64/lib")
endif(Boost_NO_SYSTEM_PATHS)

# find_package(Boost)

add_executable(ViditelPlayground main.cpp)
set_property(TARGET ViditelPlayground PROPERTY CXX_STANDARD 20)

# target_link_libraries(ViditelPlayground PRIVATE ${BOOST_LIBRARY_DIRS}/libboost_atomic.a)
# link_directories(${BOOST_LIBRARY_DIRS})
    
target_include_directories(ViditelPlayground PRIVATE ${BOOST_INCLUDE_DIRS})