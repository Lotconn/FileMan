#[[

                          MIT License

                      Copyright Lotconn 2026

This is the CMakeLists file for the example interactive cli app demonstrating
the integration of FileMan class in a Qt/C++ implementation.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

]]

#[KEY:Minimum 3.16 recommended]
cmake_minimum_required(VERSION 3.16...4.4)

include(CheckIPOSupported)

set(TARGET_NAME fileman-cli-qt)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(${TARGET_NAME}
    DESCRIPTION "Example C++ program implementing FileMan"
    LANGUAGES CXX
  )

  #[KEY:BEGIN:Recommended]
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  #[KEY:END]

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  endif()
endif()

set(CF_PROTECTION_FLAG -fcf-protection=none)
set(CF_PROTECTION_FLAG_MSVC "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  message(STATUS "Enabling Intel CET for x86_64/amd64")
  set(CF_PROTECTION_FLAG -fcf-protection)
  set(CF_PROTECTION_FLAG_MSVC /guard:cf)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Qml)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

  #[KEY:Recommended]
  qt_standard_project_setup(REQUIRES 6.8)
endif()

if(TARGET FileMan)
  message(STATUS "Building ${TARGET_NAME} example in-tree with root project")
else()

  #[KEY]
  find_package(FileMan REQUIRED)
endif()

set(default_build_type "Release")

if((CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  AND
  NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS
    "Setting build type to '${default_build_type}' as none was specified."
  )
  set(CMAKE_BUILD_TYPE "${default_build_type}"
    CACHE
    STRING "Choose the type of build."
    FORCE
  )
  set_property(CACHE
    CMAKE_BUILD_TYPE
    PROPERTY
    STRINGS
    "Debug" "MinSizeRel" "Release" "RelWithDebInfo"
  )
endif()

qt_add_executable(${TARGET_NAME} main.cpp)

check_ipo_supported(RESULT result OUTPUT output)
if(result)
  set_target_properties(${TARGET_NAME}
    PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
  )
else()
  message(STATUS "Not enabling IPO: ${output}")
endif()

target_compile_definitions(${TARGET_NAME} PRIVATE
  QT_ENABLE_STRICT_MODE_UP_TO=0x060800
)
target_compile_options(${TARGET_NAME} PRIVATE
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:
  -Wall
  -Wextra
  -Wp,-D_FORTIFY_SOURCE=3
  -Wp,-D_GLIBCXX_ASSERTIONS
  -fexceptions
  -fstack-protector-strong
  -fasynchronous-unwind-tables
  -fstack-clash-protection
  ${CF_PROTECTION_FLAG}
  -mno-omit-leaf-frame-pointer
  -pipe
  $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
  -grecord-gcc-switches
  -O1
  >>
  $<$<OR:$<CXX_COMPILER_ID:MSVC>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:
  /W3
  /guard:cf
  /EHsc
  >
)

#[KEY:Include Lotconn::FileMan]
target_link_libraries(${TARGET_NAME} PRIVATE Qt::Core Lotconn::FileMan)
target_link_options(${TARGET_NAME} PRIVATE
  $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:-fuse-ld=lld>
  $<$<AND:$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:
  -Wl,-z,relro,-z,now
  -Wl,--as-needed
  >
  # MSVC / clang-cl linker hardening
  $<$<OR:$<CXX_COMPILER_ID:MSVC>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:
  /guard:cf
  /NXCOMPAT
  >
)
