cmake_minimum_required(VERSION 3.18)
include_guard(DIRECTORY)

set(CMAKE_CXX_STANDARD 20)
if(NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed" )
endif()

project(
  ntcode-parser
  VERSION 0.1.0
  LANGUAGES CXX C
)

Find_Package(LLVM REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
llvm_map_components_to_libnames(llvm_libs core support)

add_executable(parser Driver.cpp)
target_include_directories(parser PUBLIC src)
target_link_libraries(parser ${llvm_libs})
target_sources(parser PUBLIC 
  src/ParserHead.cpp
  src/ParserDump.cpp
  src/ParserTail.cpp
  src/Emitter.cpp
)
