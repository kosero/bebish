cmake_minimum_required(VERSION 3.16)
project(minishell LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/inc
)

find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
set_target_properties(${PROJECT_NAME} PROPERTIES
    C_CLANG_TIDY "${CLANG_TIDY_EXE};-p;${CMAKE_BINARY_DIR}"
)