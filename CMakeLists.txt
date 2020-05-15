cmake_minimum_required(VERSION 3.10)
project(udpmqttgw)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -Werror")
# set(CMAKE_VERBOSE_MAKEFILE ON)

# use clang-tidy
set(CLANG_TIDY_HEADER_FILTER "src/")
set(CLANG_TIDY_CHECKS "-*,bugprone-*,cert-*,modernize-*,-modernize-use-trailing-return-type,readability-*,performance-*,llvm-*,-llvm-header-guard,google-*,-google-readability-todo,cppcoreguidelines-*,-cppcoreguidelines-owning-memory,-cppcoreguidelines-pro-type-vararg,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-cppcoreguidelines-macro-usage,-cppcoreguidelines-non-private-member-variables-in-classes")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_CXX_CLANG_TIDY
#   clang-tidy;
#   -header-filter=${CLANG_TIDY_HEADER_FILTER};
#   -checks=${CLANG_TIDY_CHECKS};
# )

# add version number from git tag/ commit
execute_process(
  COMMAND git describe --always --tags --dirty
  OUTPUT_VARIABLE GIT_VERSION_TAG
  OUTPUT_STRIP_TRAILING_WHITESPACE
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
message(STATUS "Git version: ${GIT_VERSION_TAG}")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(
  autogen/version.h.in
  generated/version.h
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h)


# add paho MQTT shared library
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# add our own files
file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(udpmqttgw ${SOURCES})
target_link_libraries(udpmqttgw paho-mqtt3cs) # use asynchronous, SSL enabled version

install(TARGETS udpmqttgw
        RUNTIME DESTINATION bin)



# add a manual clang-tidy make target
find_program(CLANG_TIDY "clang-tidy")
if(CLANG_TIDY)
  # get include directories and convert them to compiler flags (prepend "-I")
  get_property(TIDY_INCLUDE_DIRS TARGET udpmqttgw PROPERTY INCLUDE_DIRECTORIES)
  foreach(X IN ITEMS ${TIDY_INCLUDE_DIRS})
    list(APPEND TIDY_INCLUDE_DIRS_ARG "-I${X}")
  endforeach()
  
  # add a Make target named "tidy" (to run clang-tidy manually)
  add_custom_target(tidy
    COMMAND ${CLANG_TIDY}
    -header-filter=${CLANG_TIDY_HEADER_FILTER}
    -checks=${CLANG_TIDY_CHECKS}
    -p=${PROJECT_BINARY_DIR}
    ${SOURCES}
    --
    ${TIDY_INCLUDE_DIRS_ARG}
  )
endif()
