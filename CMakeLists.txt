# Qt recommends minimum version of 3.16
cmake_minimum_required(VERSION 3.16...4.3)

# CMakePrintHelpers for printing variables easily
include(CMakePrintHelpers)
include(CMakePackageConfigHelpers)

project(FileMan
  VERSION     1.0.0
  DESCRIPTION "A file manager library for Qt6 apps written in C++"
  LANGUAGES   CXX
  # SPDX_LICENSE  MIT  # SPDX_LICENSE doesn't work before cmake 4.1
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Some IDEs need this for linter servers
set(CMAKE_PROJECT_SPDX_LICENSE "MIT")
set(FM_NS HasanEsa)
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

option(FM_BUILD_EXAMPLES "Build examples in the project" OFF)
set(FM_BUILD_AS "SHARED" CACHE STRING "Build the library as static")
set_property(CACHE FM_BUILD_AS PROPERTY STRINGS "SHARED" "STATIC")

find_package(Qt6 REQUIRED COMPONENTS Core Qml)
find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

qt_standard_project_setup(REQUIRES 6.9)
#
# Strictly standardize on Qt 6.9
#
# These CMake policies are in effect:
# QTP0001: ':/qt/qml/' is the default resource prefix for QML modules (Qt 6.5)
# QTP0002: Target properties that specify Android-specific paths may contain
#          generator expressions. (Qt 6.6)
# QTP0003: Consider the BUILD_SHARED_LIBS value when creating Qt
#          libraries (Qt 6.7)
# QTP0004: Extra directories with QML files in a QML module need extra qmldir
#          files. (Qt 6.8)
# QTP0005: qt_add_qml_module dependency keywords accept CMake targets (Qt 6.8)
#

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS
    "Setting build type to '${default_build_type}' as none was specified."
  )
  set(CMAKE_BUILD_TYPE "${default_build_type}"
    CACHE
    STRING  "Choose the type of build."
    FORCE
  )
  set_property(CACHE
      CMAKE_BUILD_TYPE
    PROPERTY
      STRINGS
        "Debug" "MinSizeRel" "Release" "RelWithDebInfo"
  )
endif()

if(DEFINED BUILD_SHARED_LIBS AND NOT BUILD_SHARED_LIBS)
  set(FM_BUILD_AS "STATIC")
endif()

qt_add_qml_module(${PROJECT_NAME}
  URI     HasanEsa.FileMan
  VERSION 1.0
  ${FM_BUILD_AS}
  SOURCES
    fileman.cpp
    fileman.hpp

  #
  # qt_add_qml_module() creates some object targets that need to be
  # included with the qml module in order for it to work. This is
  # especially necessary on static builds. OUTPUT_TARGETS enlists all
  # those required objects in the variable
  #
  OUTPUT_TARGETS  fm_qml_mod_output_targets
)
set_target_properties(${PROJECT_NAME}plugin
  PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION  ON
)

# Some variables worth noting down
qt_query_qml_module(${PROJECT_NAME}
  URI         fm_qml_mod_uri
  VERSION     fm_qml_mod_version
  TARGET_PATH fm_qml_mod_target_path
  QMLDIR      fm_qml_mod_qmldir
  TYPEINFO    fm_qml_mod_typeinfo
)

# Namespaces are always a good idea
add_library(${FM_NS}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

#
# Disable policies older than Qt 6.9. The format is 0xMMmmrr
# MM = Major (06); mm = minor (09); rr = revision (00)
#
# Refer to https://doc.qt.io/qt-6/qtglobal.html#QT_ENABLE_STRICT_MODE_UP_TO
# for a complete list of disabled APIs
#
target_compile_definitions(${PROJECT_NAME} PRIVATE
  QT_ENABLE_STRICT_MODE_UP_TO=0x060900
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:
    -Wp,-D_FORTIFY_SOURCE=3
    -Wp,-D_GLIBCXX_ASSERTIONS
    -fexceptions
    -fstack-protector-strong
    -fasynchronous-unwind-tables
    -fstack-clash-protection
    -fcf-protection
    -mno-omit-leaf-frame-pointer
    -pipe
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
      -grecord-gcc-switches
    >
  >
)
target_include_directories(${PROJECT_NAME} PUBLIC
  #
  # Since the project layout is flat, CMake needs to know where to look for
  # headers/source files during building and how to handle installing such a
  # flat structure
  #
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

#
# Versionless invokes are preferred
#
# Return types depends on Qt::Core & ::Qml, needs to be PUBLIC
#
target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core Qt::Qml)
target_link_options(${PROJECT_NAME} PRIVATE
  # necessary because ld fails to link on clang+llvm
  $<$<CXX_COMPILER_ID:Clang>:-fuse-ld=lld>
  $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:
    -Wl,-z,relro,-z,now
    -Wl,--as-needed
  >
)
target_sources(${PROJECT_NAME} PUBLIC FILE_SET HEADERS FILES fileman.hpp)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION  ON
    OUTPUT_NAME ${PROJECT_NAME_LOWER} # Outputs (lib)FileMan.* otherwise
    VERSION     ${PROJECT_VERSION}
    SOVERSION   ${PROJECT_VERSION_MAJOR}
)

if(FM_BUILD_EXAMPLES)
  add_subdirectory(examples/fileman-cli-qt)
  add_subdirectory(examples/fileman-qml-legacy)
  add_subdirectory(examples/fileman-qml)
endif()

install(TARGETS
    ${PROJECT_NAME}
    ${fm_qml_mod_output_targets} # install those qml module object targets
  EXPORT  ${PROJECT_NAME}Targets

  ARCHIVE
    COMPONENT devel
  LIBRARY
    COMPONENT runtime

  # QML module objects go under the INSTALL_LIBDIR (/lib, /lib64 etc)
  OBJECTS
    COMPONENT   devel
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/${FM_NS}/${PROJECT_NAME}"

  # This installs the header (.hpp) file properly
  FILE_SET HEADERS
    COMPONENT   devel
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${FM_NS}/${PROJECT_NAME}"
)

#
# The qml module plugin and related files should be installed under
# INSTALL_LIBDIR/qt6/qml as is conventional. QT_QML_OUTPUT_DIR could be manually
# set but QT6_INSTALL_QML already has the structure right
#
install(TARGETS
    "${PROJECT_NAME}plugin"
  EXPORT  ${PROJECT_NAME}Targets
  ARCHIVE
    COMPONENT   devel
    DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
  LIBRARY
    COMPONENT   runtime
    DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
)
install(FILES
    "${fm_qml_mod_qmldir}"
    "${fm_qml_mod_typeinfo}"
  DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
  COMPONENT   runtime
)

# Auto create the *ConfigVersion.cmake file
# requires including CMakePackageConfigHelpers
write_basic_package_version_file(
  ${PROJECT_NAME}ConfigVersion.cmake
  VERSION       ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# Install the handwritten *Targets.cmake file. Library can now be invoked during
# linking as HasanEsa::FileMan
install(EXPORT  ${PROJECT_NAME}Targets
  FILE        ${PROJECT_NAME}Targets.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  COMPONENT   devel
  NAMESPACE   HasanEsa::
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION  "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  COMPONENT devel
)
install(FILES
    README.md
    CHANGELOG.md
  TYPE DOC
  COMPONENT runtime
)
install(FILES
    LICENSE
  DESTINATION "${CMAKE_INSTALL_DATADIR}/license/${FM_NS}/${PROJECT_NAME}"
  COMPONENT   runtime
)
