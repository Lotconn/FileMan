#[[

                                         MIT License
                
                                     Copyright Lotconn 2026

This is the CMakeLists file for the example app demonstrating modern use of the FileMan library as a
QML module. This approach is recommended over setting context properties from C++ backend to QML
namespace.

The key directives to extract from this example for library integration are highlighted with [KEY]
inline comment on the preceeding line. For a subsequent block, the highlighted part starts with
[KEY:BEGIN] and ends with [KEY:END]. Additional notes are appended to the specifier like
[KEY:BEGIN:extra comments] or [KEY::extra comments].

]]

#[KEY:Minimum 3.24 recommended]
cmake_minimum_required(VERSION 3.24...4.4)

include(CheckIPOSupported)

set(target_name fileman-qml)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(${target_name} DESCRIPTION "Example QML program implementing FileMan" LANGUAGES CXX)

  #[KEY:BEGIN:Recommended]
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_CXX_STANDARD 20)
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

set(compile_flags "")
set(linker_flags "")
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL MSVC)
  set(compile_flags /EHsc /GS /Oy- /W3 "${cf_protection_flag_msvc}")
  # MSVC / clang-cl linker hardening
  set(linker_flags "${cf_protection_flag_msvc}")

elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  set(compile_flags
      -Wall
      -Wextra
      -Wp,-D_FORTIFY_SOURCE=3
      -Wp,-D_GLIBCXX_ASSERTIONS
      -fexceptions
      -fstack-protector-strong
      -fasynchronous-unwind-tables
      -fstack-clash-protection
      "${cf_protection_flag}"
      -mno-omit-leaf-frame-pointer
      -pipe)

  # necessary because ld fails to link on clang+llvm
  if(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    set(linker_flags -fuse-ld=lld)
  endif()

  if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
    list(APPEND compile_flags -grecord-gcc-switches -O1)
  else()
    # ELF-specific linker hardening
    list(APPEND linker_flags -Wl,-z,relro,-z,now -Wl,--as-needed)
  endif()
endif()

#[KEY:Qml is Required]
find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

  #[KEY:Recommended]
  qt_standard_project_setup(REQUIRES 6.12)
endif()

if(TARGET FileMan)
  message(STATUS "Building ${target_name} example in-tree with root project")
else()

  #[KEY:BEGIN:Use the system installed instance]
  find_package(FileMan REQUIRED)
  #[KEY:END]

  if(DEFINED FileMan_FOUND)
    message(STATUS "Found FileMan: TRUE")
  endif()
endif()

set(default_build_type "Release")
if((CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
   AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS "Debug" "MinSizeRel" "Release" "RelWithDebInfo")
endif()

qt_add_executable(${target_name} main.cpp)

check_ipo_supported(RESULT result OUTPUT output)
if(result)
  set_target_properties(${target_name} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
  message(STATUS "Not enabling IPO: ${output}")
endif()

#[KEY:BEGIN]
qt_add_qml_module(${target_name} URI FilemanQml QML_FILES main.qml DEPENDENCIES Lotconn.FileMan)
#[KEY:END]

set(fm_plugin_target FileManplugin)

if(TARGET Lotconn::FileManplugin)
  set(fm_plugin_target Lotconn::FileManplugin)
endif()

get_target_property(fm_plugin_type ${fm_plugin_target} TYPE)
if(fm_plugin_type STREQUAL "STATIC_LIBRARY")
  target_compile_definitions(${target_name} PRIVATE FM_STATIC_PLUGIN)

  #[KEY:Link against FileManplugin for static building]
  target_link_libraries(${target_name} PRIVATE ${fm_plugin_target})
else()

  #[KEY:Link against the library for shared-libs/regular builds]
  target_link_libraries(${target_name} PRIVATE Lotconn::FileMan)
endif()
unset(fm_plugin_type)

target_compile_definitions(${target_name} PRIVATE QT_ENABLE_STRICT_MODE_UP_TO=0x061200)
target_compile_options(${target_name} PRIVATE "${compile_flags}")
target_link_libraries(${target_name} PRIVATE Qt::Core Qt::Qml Qt::Quick Qt::Widgets)
target_link_options(${target_name} PRIVATE "${linker_flags}")
