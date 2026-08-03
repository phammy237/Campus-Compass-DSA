cmake_minimum_required(VERSION 3.22)
project(Project3)

set(CMAKE_CXX_STANDARD 17)

#compile flags to match Gradescope test environment
set(GCC_COVERAGE_COMPILE_FLAGS "-Wall -Werror") # remove -Wall if you don't want as many warnings treated as errors
# gradescope does use -Werror, so if you remove it here you may run into issues when trying to submit
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )

##pull in desired version of catch through cmake automatically, make it available
Include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2 # if you run into issues, try updating your editor or downgrading to a lower version of catch v3
)
FetchContent_MakeAvailable(Catch2)

## pull in cpp-httplib (single-header HTTP server library) for the ApiServer target
FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

include_directories(src)

add_executable(Main
        src/main.cpp # your main file
        src/CampusCompass.cpp
        src/CampusCompass.h
        src/Models.h
        src/Graph.cpp
        src/Graph.h
        src/StudentManager.cpp
        src/StudentManager.h
        src/Validation.cpp
        src/Validation.h
        )

# These tests can use the Catch2-provided main
add_executable(Tests
        test/test.cpp # your test file
        src/CampusCompass.cpp
        src/CampusCompass.h
        src/Models.h
        src/Graph.cpp
        src/Graph.h
        src/StudentManager.cpp
        src/StudentManager.h
        src/Validation.cpp
        src/Validation.h
        )
        
target_link_libraries(Tests PRIVATE Catch2::Catch2WithMain) #link catch to test.cpp file
# the name here must match that of your testing executable (the one that has test.cpp)

# Local HTTP API + static-file server backing the web frontend (web/)
add_executable(ApiServer
        src/ApiServer.cpp
        src/ApiServer.h
        src/CampusCompass.cpp
        src/CampusCompass.h
        src/Models.h
        src/Graph.cpp
        src/Graph.h
        src/StudentManager.cpp
        src/StudentManager.h
        src/Validation.cpp
        src/Validation.h
        src/Locations.cpp
        src/Locations.h
        src/ScheduleBuilder.cpp
        src/ScheduleBuilder.h
        )
target_include_directories(ApiServer PRIVATE ${httplib_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(ApiServer PRIVATE Threads::Threads)
if (WIN32)
    target_link_libraries(ApiServer PRIVATE ws2_32 crypt32)
endif()

# comment everything below out if you are using CLion
include(CTest)
include(Catch)
catch_discover_tests(Tests) # must be named the same as your test executable

# Copy `data/` into the build directory so runtime can open files with `data/...`
add_custom_target(copy_data ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                        ${CMAKE_SOURCE_DIR}/data
                        ${CMAKE_BINARY_DIR}/data
        COMMENT "Copying data directory to build directory"
)

# Copy `web/` into the build directory so ApiServer can serve the frontend as static files
add_custom_target(copy_web ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                        ${CMAKE_SOURCE_DIR}/web
                        ${CMAKE_BINARY_DIR}/web
        COMMENT "Copying web directory to build directory"
)

# Ensure all executables are built after the data/web directories are copied
add_dependencies(Main copy_data)
add_dependencies(Tests copy_data)
add_dependencies(ApiServer copy_data copy_web)
