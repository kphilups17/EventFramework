cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

# Detect if we're being included as a subproject.  If so, disable the tests.
if(DEFINED PROJECT_NAME)
  set(IS_SUBPROJECT ON)
else()
  set(IS_SUBPROJECT OFF)
endif()

project(EventFramework CXX)

include(FetchContent)


FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.ibm.com/ChipDesignRTX/fmt.git
  GIT_TAG        10.2.0
)
FetchContent_GetProperties(fmt)
if (NOT fmt_POPULATED)
  FetchContent_MakeAvailable(fmt)
endif ()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (NOT IS_SUBPROJECT)
  add_compile_options(
    -Wall
    -Werror
  )
endif()

if (NOT IS_SUBPROJECT)
  # If we're the primary project, create a compile_commands.json
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  # Look up and configure a linter.
  #   - clang-tidy
  find_program(LINTER clang-tidy)
  if (${LINTER} STREQUAL "LINTER-NOTFOUND")
    message("-- clang-tidy not found, 'lint' target will not be available.")
  else()
    message("-- clang-tidy found, 'lint' target will be available, using '${LINTER}'")
    add_custom_target(lint bash -c "\"${LINTER}\" -p \"${CMAKE_BINARY_DIR}\" $(find include/ src/ tests/ -type f)" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} VERBATIM)
  endif()
endif()


# The main evt interface library
add_library(
  evt::evt
  INTERFACE
  IMPORTED
  GLOBAL
)
target_include_directories(
  evt::evt
  INTERFACE
  ${PROJECT_SOURCE_DIR}/include
)


# The Fusion library to link when using the evt/interface module.
add_library(
  evt_interface
  ${PROJECT_SOURCE_DIR}/src/interface/EventFrameworkInterface.cpp
)

target_link_libraries(
  evt_interface
  PUBLIC
  evt::evt
)

add_library(
  evt::interface
  ALIAS
  evt_interface
)


target_link_libraries(
  evt_interface
  PUBLIC
  evt::evt
  fmt::fmt
)


if (NOT IS_SUBPROJECT)

  FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.ibm.com/ChipDesignRTX/Catch2.git
    GIT_TAG        v3.4.0
  )
  FetchContent_GetProperties(catch2)
  if (NOT catch2_POPULATED)
    FetchContent_MakeAvailable(catch2)
  endif ()

  add_executable(
    tests
    EXCLUDE_FROM_ALL
    tests/test_hashing.cpp
    tests/test_Event.cpp
    tests/test_EventHistory.cpp
    tests/test_MultiEventHandler.cpp
  )
  target_include_directories(
    tests
    PUBLIC
    ${PROJECT_SOURCE_DIR}/tests/support
  )

  target_link_libraries(
    tests
    PUBLIC
    Catch2::Catch2WithMain
    evt::evt
    fmt::fmt
  )
endif ()
