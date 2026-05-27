# Qt recommends minimum version of 3.16
cmake_minimum_required(VERSION 3.16...4.3)

# CMakePrintHelpers for printing variables easily
include(CMakePrintHelpers)
include(CMakePackageConfigHelpers)

project(FileMan
    VERSION 1.0.0
    DESCRIPTION "A file manager library for Qt6 apps written in C++"
    LANGUAGES CXX
    # SPDX_LICENSE MIT  # SPDX_LICENSE doesn't work before cmake 4.1
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Some IDEs need this for linter servers
set(FM_NS HasanEsa CACHE STRING "Namespace for the project")
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

# Do not show this in normal listing
mark_as_advanced(FORCE FM_NS)
mark_as_advanced(FORCE PROJECT_NAME_LOWER)

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
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING
        "Choose the type of build."
        FORCE
    )
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "MinSizeRel" "Release" "RelWithDebInfo"
    )
endif()

set(FM_BUILD_AS "SHARED" CACHE STRING "Choose the type of build")

qt_add_qml_module(${PROJECT_NAME}
    URI             HasanEsa.FileMan
    VERSION         1.0
    ${FM_BUILD_AS}
    SOURCES         fileman.cpp fileman.hpp
    #
    # qt_add_qml_module() creates some object targets that need to be
    # included with the qml module in order for it to work. This is
    # especially necessary on static builds. OUTPUT_TARGETS enlists all
    # those required objects in the variable
    #
    OUTPUT_TARGETS  fm_qml_mod_output_targets
)
set_target_properties(${PROJECT_NAME}plugin PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ON
)

# Some variables worth noting down
qt_query_qml_module(${PROJECT_NAME}
    URI             fm_qml_mod_uri
    VERSION         fm_qml_mod_version
    TARGET_PATH     fm_qml_mod_target_path
    QMLDIR          fm_qml_mod_qmldir
    TYPEINFO        fm_qml_mod_typeinfo
)

# qt_add_qml_module() handles this part, keep this around for reference
# add_library(${PROJECT_NAME}
#     ${CMAKE_CURRENT_SOURCE_DIR}/fileman.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fileman.hpp
# )

set(FM_BUILD_EXAMPLES OFF CACHE BOOL "Build examples in the project")

if(FM_BUILD_EXAMPLES)
    add_subdirectory(examples/fileman-app)
    add_subdirectory(examples/fileman-cpp)
    add_subdirectory(examples/fileman-qml)
endif()

# Namespaces are always a good idea
add_library(${FM_NS}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

#
# Disable policies older than Qt 6.9. The format is 0xMMmmrr
# MM = Major (06); mm = minor (09); rr = revision (00)
#
# Refer to https://doc.qt.io/qt-6/qtglobal.html#QT_ENABLE_STRICT_MODE_UP_TO
# for a complete list of disabled APIs
#
target_compile_definitions(${PROJECT_NAME} PUBLIC
    QT_ENABLE_STRICT_MODE_UP_TO=0x060900
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE
    -pipe -Wp,-D_FORTIFY_SOURCE=3 -Wp,-D_GLIBCXX_ASSERTIONS -fexceptions
    -fstack-protector-strong -fasynchronous-unwind-tables
    -fstack-clash-protection -fcf-protection -mno-omit-leaf-frame-pointer
    $<$<CONFIG:Release>:-O3>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-O2 -g -grecord-gcc-switches>
    # CMake auto handles MinSizeRel with -Os
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
# Versionless invokes are preferred
target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core Qt::Qml)
target_link_options(${PROJECT_NAME} PRIVATE
    -fuse-ld=lld # necessary because ld fails to link
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-Wl,-z,relro,-z,now -Wl,--as-needed>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION    ON
    PUBLIC_HEADER                   fileman.hpp
    OUTPUT_NAME                     ${PROJECT_NAME_LOWER} # Outputs
                                                          # (lib)FileMan.*
                                                          # otherwise
    VERSION                         ${PROJECT_VERSION}
    SOVERSION                       ${PROJECT_VERSION_MAJOR}
)

install(TARGETS                 ${PROJECT_NAME}
    ${fm_qml_mod_output_targets} # install those qml module object targets
    EXPORT                      ${PROJECT_NAME}Targets
    # QML module objects go under the INSTALL_LIBDIR (/lib, /lib64 etc)
    OBJECTS         DESTINATION "${CMAKE_INSTALL_LIBDIR}/${FM_NS}/${PROJECT_NAME}"
    # This installs the header (.hpp) file properly
    PUBLIC_HEADER   DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${FM_NS}/${PROJECT_NAME}"
)
#
# The qml module plugin and related files should be installed under
# INSTALL_LIBDIR/qt6/qml as is conventional. QT_QML_OUTPUT_DIR could be manually
# set but QT6_INSTALL_QML already has the structure right
#
install(TARGETS "${PROJECT_NAME}plugin"
    EXPORT               ${PROJECT_NAME}Targets
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
)
install(FILES
    "${fm_qml_mod_qmldir}"
    "${fm_qml_mod_typeinfo}"
    DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
)
# Auto create the *ConfigVersion.cmake file
# requires including CMakePackageConfigHelpers
write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION         ${PROJECT_VERSION}
    COMPATIBILITY   AnyNewerVersion
)
# Install the handwritten *Targets.cmake file. Library can now be invoked during
# linking as HasanEsa::FileMan
install(EXPORT  ${PROJECT_NAME}Targets
    FILE        ${PROJECT_NAME}Targets.cmake
    NAMESPACE   HasanEsa::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
