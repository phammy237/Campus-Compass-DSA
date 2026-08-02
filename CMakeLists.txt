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

# Ensure both executables are built after the data is copied
add_dependencies(Main copy_data)
add_dependencies(Tests copy_data)
