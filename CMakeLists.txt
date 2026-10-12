# ---------------------------------------------------------------
# GENERAL
# ---------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
project(GRMK C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -g)


if(NOT WIN32)
    message(FATAL_ERROR "GRMK currently only supports Windows platform.")
endif()

#   BUILD LIBRARY FOR SIMPLE LINKING
include(FetchContent)
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

add_library(grmk_core STATIC 
    pathList.c 
    stage.c 
    diff.c 
    utils.c 
    objects.c 
    ignore.c 
    branch.c
)
target_link_libraries(grmk_core PUBLIC bcrypt zlibstatic )
target_include_directories(grmk_core PUBLIC ${CMAKE_SOURCE_DIR})

# ---------------------------------------------------------------
# Unit Tests
# ---------------------------------------------------------------
include(CTest)

add_executable(test_pathList Test/test_pathList.c)
target_link_libraries(test_pathList PRIVATE grmk_core)
add_test(NAME test_pathList COMMAND test_pathList)

add_executable(test_stage Test/test_stage.c)
target_link_libraries(test_stage PRIVATE grmk_core)
add_test(NAME test_stage COMMAND test_stage)

add_executable(test_utils Test/test_utils.c)
target_link_libraries(test_utils PRIVATE grmk_core)
add_test(NAME test_utils COMMAND test_utils)

add_executable(test_objects Test/test_objects.c)
target_link_libraries(test_objects PRIVATE grmk_core)
add_test(NAME test_objects COMMAND test_objects)

# ---------------------------------------------------------------
# Main Executable
# ---------------------------------------------------------------
add_executable(grmk grmk.c)
target_link_libraries(grmk PRIVATE grmk_core)
set_target_properties(grmk PROPERTIES   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
                                        COMPILE_WARNING_AS_ERROR ON
                                        LINK_FLAGS "-static")
