cmake_minimum_required(VERSION 3.20)
project(GRMK C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "GRMK currently only supports Windows platform.")
endif()

# Compiler flags
add_compile_options(-Wall -g)
find_package(ZLIB REQUIRED)
set(COMMON_LIBS bcrypt ZLIB::ZLIB)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
enable_testing()

# ---------------------------------------------------------------
# Unit Tests
# ---------------------------------------------------------------
add_executable(test_pathlist pathList.c)
target_compile_definitions(test_pathlist PRIVATE TEST_PATHLIST)
add_test(NAME test_pathlist COMMAND test_pathlist)

add_executable(test_utils utils.c pathList.c ignore.c)
target_compile_definitions(test_utils PRIVATE TEST_UTILS)
target_link_libraries(test_utils PRIVATE bcrypt)
add_test(NAME test_utils COMMAND test_utils)

add_executable(test_stage stage.c diff.c utils.c pathList.c objects.c ignore.c branch.c)
target_compile_definitions(test_stage PRIVATE TEST_STAGE)
target_link_libraries(test_stage PRIVATE ${COMMON_LIBS})
add_test(NAME test_stage COMMAND test_stage)

add_executable(test_ignore ignore.c pathList.c utils.c)
target_compile_definitions(test_ignore PRIVATE TEST_IGNORE)
target_link_libraries(test_ignore PRIVATE bcrypt)
add_test(NAME test_ignore COMMAND test_ignore)

# ---------------------------------------------------------------
# Main Executable
# ---------------------------------------------------------------
add_executable(grmk grmk.c stage.c diff.c utils.c pathList.c objects.c ignore.c branch.c)
target_link_libraries(grmk PRIVATE ${COMMON_LIBS})
set_target_properties(grmk PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
