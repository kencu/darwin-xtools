configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/ld_vers.c.in"
  "${CMAKE_BINARY_DIR}/ld64/ld_vers.c"
  )

set(LD64_SOURCES
  Options.cpp
  ld.cpp
  debugline.c
  InputFiles.cpp
  SymbolTable.cpp
  Resolver.cpp
  OutputFile.cpp
  Snapshot.cpp
  parsers/macho_relocatable_file.cpp
  parsers/archive_file.cpp
  parsers/lto_file.cpp
  parsers/macho_dylib_file.cpp
  parsers/textstub_dylib_file.cpp
  parsers/opaque_section_file.cpp
  passes/stubs/stubs.cpp
  passes/dtrace_dof.cpp
  passes/compact_unwind.cpp
  passes/got.cpp
  passes/huge.cpp
  passes/branch_island.cpp
  passes/branch_shim.cpp
  passes/bitcode_bundle.cpp
  passes/objc.cpp
  passes/dylibs.cpp
  passes/tlvp.cpp
  passes/order.cpp
  passes/order.cpp
  code-sign-blobs/blob.cpp
  "${CMAKE_BINARY_DIR}/ld64/ld_vers.c"
  )

execute_process(
    COMMAND cat ${CMAKE_SOURCE_DIR}/ld64/compile_stubs
    OUTPUT_VARIABLE COMPILE_STUBS_COMMAND)
if (HAD_ERROR)
    message(FATAL_ERROR "ld64 compile_stubs generation failed with status ${HAD_ERROR}")
endif()
string(REPLACE "\n" "\\n" COMPILE_STUBS_COMMAND ${COMPILE_STUBS_COMMAND})
string(REPLACE "\"" "\\\"" COMPILE_STUBS_COMMAND ${COMPILE_STUBS_COMMAND})

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/compile_stubs.h.in"
  "${CMAKE_BINARY_DIR}/ld64/compile_stubs.h"
  )

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/parsers")

if(XTOOLS_LTO_SUPPORT)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lLTO")
endif()
add_definitions(-DLD_VERS="xtools-ld64-${LD64_VERSION_NUM}")

add_executable(ld ${LD64_SOURCES})
if(NOT XTOOLS_HAS_MODERNXAR)
  target_link_libraries(ld xarextralib)
endif()

install(TARGETS ld DESTINATION bin)
