#
# CMake & project setup ############################################################################
#

# Qt recommends minimum version of 3.16
cmake_minimum_required(VERSION 3.24...4.4)

project(FileMan
        VERSION 2.1.0
        DESCRIPTION "A file manager library for Qt6 apps written in C++"
        LANGUAGES CXX)

# CheckIPOSupported for opportunistically enabling IPO/LTO 
include(CheckIPOSupported)
include(CMakeDependentOption)
# CMakePrintHelpers for printing variables easily
include(CMakePrintHelpers)
include(CMakePackageConfigHelpers)

#---------------------------------------------------------------------------------------------------
#
# Global variables #################################################################################
#

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Some IDEs need this for linter servers
set(CMAKE_PROJECT_SPDX_LICENSE "MIT")
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/)
set(fm_ns Lotconn)

string(TOLOWER ${PROJECT_NAME} project_name_lower)

## Conditional variables ###########################################################################

option(FM_BUILD_EXAMPLES "Build examples in the project" OFF)
option(FM_BUILD_TESTS "Build tests in the project" OFF)
set(FM_BUILD_AS "SHARED" CACHE STRING "Build the library as static")
set_property(CACHE FM_BUILD_AS PROPERTY STRINGS "SHARED" "STATIC")

if(DEFINED BUILD_SHARED_LIBS AND NOT BUILD_SHARED_LIBS)
  set(FM_BUILD_AS "STATIC" CACHE STRING "Build the library as static" FORCE)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF CACHE BOOL "Build tests in the project")
  endif()

  include(CTest)

  if(BUILD_TESTING)
    set(FM_BUILD_TESTS ON CACHE BOOL "Build tests in the project" FORCE)
  elseif(FM_BUILD_TESTS)
    set(BUILD_TESTING ON CACHE BOOL "Build tests in the project" FORCE)
  endif()
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS "Debug" "MinSizeRel" "Release" "RelWithDebInfo")
endif()

set(cf_protection_flag -fcf-protection=none)
set(cf_protection_flag_msvc "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  message(STATUS "Enabling Intel CET for x86_64/amd64")
  set(cf_protection_flag -fcf-protection)
  set(cf_protection_flag_msvc /guard:cf)
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

configure_file(${PROJECT_SOURCE_DIR}/cmake-configs/ExtraCPackConfig.cmake.in
               ${PROJECT_BINARY_DIR}/ExtraCPackConfig.cmake
               @ONLY)
set(CPACK_PROJECT_CONFIG_FILE ${PROJECT_BINARY_DIR}/ExtraCPackConfig.cmake)
set(CPACK_GENERATOR "TGZ;RPM")
set(CPACK_PACKAGE_VENDOR "Lotconn")

#---------------------------------------------------------------------------------------------------
#
# Package/library setup ############################################################################
#

find_package(Qt6 REQUIRED COMPONENTS Core Qml)

qt_standard_project_setup(REQUIRES 6.12)
#
# Strictly standardize on Qt 6.12
#
# These CMake policies are in effect:
# QTP0001:  ':/qt/qml/' is the default resource prefix for QML modules (Qt 6.5)
# QTP0002: Target properties that specify Android-specific paths may contain generator expressions.
#          (Qt 6.6)
# QTP0003: Consider the BUILD_SHARED_LIBS value when creating Qt libraries (Qt 6.7)
# QTP0004: Extra directories with QML files in a QML module need extra qmldir files. (Qt 6.8)
# QTP0005: qt_add_qml_module dependency keywords accept CMake targets (Qt 6.8)
# QTP0006: (unused) Enables PRIVATE_CODE as the default for Wayland protocol source generation
#          (Qt 6.12)
# QTP0007: Quotes and escapes forwarded argument values in generated deploy scripts (Qt 6.12)
# QTP0008: Forwards deploy script arguments without re-evaluating them. (Qt 6.12)
#

qt_add_qml_module(${PROJECT_NAME}
                  URI Lotconn.FileMan
                  VERSION 2.1 ${FM_BUILD_AS}
                  SOURCES
                    fileman
                    fileman.hpp
                    fileman.cpp
                    fileman_p.hpp
                    fileman_p.cpp
                    internal-helpers.hpp
                    internal-helpers.cpp
                    addFile.cpp
                    categoryToString.cpp
                    getCategoryDir.cpp
                    getDataDir.cpp
                    getFileName.cpp
                    refreshStore.cpp
                    rmFile.cpp
                    #
                    # qt_add_qml_module() creates some object targets that need to be included with 
                    # the qml module in order for it to work. This is especially necessary on static
                    # builds. OUTPUT_TARGETS enlists all those required objects in the variable
                    #
                  OUTPUT_TARGETS fm_qml_mod_output_targets)

# Some variables worth noting down
qt_query_qml_module(${PROJECT_NAME}
                    URI fm_qml_mod_uri
                    VERSION fm_qml_mod_version
                    TARGET_PATH fm_qml_mod_target_path
                    QMLDIR fm_qml_mod_qmldir
                    TYPEINFO fm_qml_mod_typeinfo)

# Namespaces are always a good idea
add_library(${fm_ns}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
# fmt: off
set_target_properties(${PROJECT_NAME} ${PROJECT_NAME}plugin
                      PROPERTIES
                        CXX_EXTENSIONS OFF
                        CXX_STANDARD 20
                        CXX_STANDARD_REQUIRED ON)

set_target_properties(${PROJECT_NAME}
                      PROPERTIES
                        OUTPUT_NAME ${project_name_lower} # Outputs (lib)FileMan.* otherwise
                        VERSION ${PROJECT_VERSION}
                        SOVERSION ${PROJECT_VERSION_MAJOR})
# fmt: on
check_ipo_supported(RESULT result OUTPUT output)
if(result)
  set_target_properties(${PROJECT_NAME} ${PROJECT_NAME}plugin
                        PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
  message(STATUS "Not enabling IPO: ${output}")
endif()

# Use appropriate rpath for Qml modules than the
# "$ORIGIN:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}",
file(RELATIVE_PATH
     plugin_rpath "${CMAKE_INSTALL_PREFIX}/${QT6_INSTALL_QML}/${fm_ns}/${PROJECT_NAME}"
     "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set_target_properties(${PROJECT_NAME}plugin
                      PROPERTIES INSTALL_RPATH "$ORIGIN:$ORIGIN/${plugin_rpath}")

#---------------------------------------------------------------------------------------------------
#
# Target compiling & linking #######################################################################
#

#
# Disable policies older than Qt 6.12. The format is 0xMMmmrr
# MM = Major (06); mm = minor (09); rr = revision (00)
#
# Refer to https://doc.qt.io/qt-6/qtglobal.html#QT_ENABLE_STRICT_MODE_UP_TO
# for a complete list of disabled APIs
#
target_compile_definitions(${PROJECT_NAME}
                           PRIVATE
                             QT_ENABLE_STRICT_MODE_UP_TO=0x061200
                             $<$<STREQUAL:${FM_BUILD_AS},SHARED>:FM_LIB_BUILD>)
target_compile_definitions(${PROJECT_NAME}plugin PRIVATE QT_ENABLE_STRICT_MODE_UP_TO=0x061200)

if(FM_BUILD_AS STREQUAL "STATIC")
  target_compile_definitions(${PROJECT_NAME} PUBLIC FM_STATIC_LIB)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE "${compile_flags}")
target_compile_options(${PROJECT_NAME}plugin PRIVATE "${compile_flags}")

target_include_directories(${PROJECT_NAME}
                           PUBLIC
                             #
                             # Since the project layout is flat, CMake needs to know where to look
                             # for headers/source files during building and how to handle installing
                             # such a flat structure
                             #
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                             $<INSTALL_INTERFACE:include>)
target_include_directories(${PROJECT_NAME}plugin
                           PUBLIC
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                             $<INSTALL_INTERFACE:include>)

#
# Versionless invokes are preferred
#
# Return types depends on Qt::Core & ::Qml, needs to be PUBLIC
#
target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core Qt::Qml)
target_link_options(${PROJECT_NAME} PRIVATE "${linker_flags}")
target_link_options(${PROJECT_NAME}plugin PRIVATE "${linker_flags}")

target_sources(${PROJECT_NAME} PUBLIC FILE_SET HEADERS FILES fileman fileman.hpp)

#---------------------------------------------------------------------------------------------------
#
# External project includes ########################################################################
#

if(FM_BUILD_EXAMPLES)
  add_subdirectory(examples/fileman-cli-qt)
  add_subdirectory(examples/fileman-qml-legacy)
  add_subdirectory(examples/fileman-qml)
endif()

#---------------------------------------------------------------------------------------------------
#
# Testing ##########################################################################################
#

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) AND FM_BUILD_TESTS)
  add_subdirectory(tests)
endif()

#---------------------------------------------------------------------------------------------------
#
# Installing #######################################################################################
#

install(TARGETS
        ${PROJECT_NAME} ${fm_qml_mod_output_targets} # install those qml module object targets
        EXPORT ${PROJECT_NAME}Targets
        ARCHIVE COMPONENT devel
        LIBRARY COMPONENT runtime
        OBJECTS # QML module objects go under the INSTALL_LIBDIR (/lib, /lib64 etc)
          COMPONENT devel
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/${fm_ns}/${PROJECT_NAME}"
          # This installs the header (.hpp) file properly
        FILE_SET HEADERS
          COMPONENT devel
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${fm_ns}/${PROJECT_NAME}")

#
# The qml module plugin and related files should be installed under
# INSTALL_LIBDIR/[qt6/]qml as is conventional. QT_QML_OUTPUT_DIR could be manually
# set but QT6_INSTALL_QML already has the structure right
#
install(TARGETS "${PROJECT_NAME}plugin"
        EXPORT ${PROJECT_NAME}Targets
        ARCHIVE COMPONENT devel DESTINATION "${QT6_INSTALL_QML}/${fm_ns}/${PROJECT_NAME}"
        LIBRARY COMPONENT runtime DESTINATION "${QT6_INSTALL_QML}/${fm_ns}/${PROJECT_NAME}")
install(FILES "${fm_qml_mod_qmldir}" "${fm_qml_mod_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/${fm_ns}/${PROJECT_NAME}"
        COMPONENT runtime)

# Auto create the *ConfigVersion.cmake file
# requires including CMakePackageConfigHelpers
write_basic_package_version_file(${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY SameMajorVersion)

# Install the handwritten *Targets.cmake file. Library can now be invoked during
# linking as Lotconn::FileMan
install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        COMPONENT devel
        NAMESPACE Lotconn::)
install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        COMPONENT devel)

# Install the docs and licenses only if building as a root project
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  install(FILES CHANGELOG.md CONTRIBUTING.md MIGRATING.md README.md TYPE DOC COMPONENT runtime)
  install(FILES LICENSE
          DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${fm_ns}/${PROJECT_NAME}"
          COMPONENT runtime)
endif()

include(CPack)
