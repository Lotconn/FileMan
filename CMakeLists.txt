cmake_minimum_required(VERSION 3.30...4.3)

include(CMakePrintHelpers)
include(CMakePackageConfigHelpers)

project(fileman
    VERSION 0.2.0
    DESCRIPTION "A file manager library for Qt6 apps written in C++"
    LANGUAGES CXX
    # SPDX_LICENSE MIT
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml)

find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

qt_standard_project_setup(REQUIRES 6.9)

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

qt_add_library(${PROJECT_NAME} "${FM_BUILD_AS}")

if("${FM_BUILD_AS}" STREQUAL "SHARED")
    qt_add_qml_module(${PROJECT_NAME}
        URI             HasanEsa.FileMan
        VERSION         0.2
        SOURCES         fileman.cpp fileman.hpp
        OUTPUT_TARGETS  fileman_output_targets
    )
    install(TARGETS "${PROJECT_NAME}plugin"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/HasanEsa/FileMan"
    )
else()
    qt_add_qml_module(${PROJECT_NAME}
        URI             HasanEsa.FileMan
        VERSION         0.2
        NO_PLUGIN
        SOURCES         fileman.cpp fileman.hpp
        OUTPUT_TARGETS  fileman_output_targets
    )
endif()

qt_query_qml_module(${PROJECT_NAME}
    URI             ${PROJECT_NAME}_uri
    VERSION         ${PROJECT_NAME}_version
    TARGET_PATH     ${PROJECT_NAME}_target_path
    QMLDIR          ${PROJECT_NAME}_qmldir
    TYPEINFO        ${PROJECT_NAME}_typeinfo
)

# add_library(${PROJECT_NAME}
#     ${CMAKE_CURRENT_SOURCE_DIR}/fileman.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fileman.hpp
# )

set(FM_BUILD_EXAMPLES OFF CACHE BOOL "Build examples in the project")

if(FM_BUILD_EXAMPLES)
    add_subdirectory(examples/)
endif()

add_library(HasanEsa::FileMan ALIAS ${PROJECT_NAME})

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
)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt6::Core Qt6::Qml)
target_link_options(${PROJECT_NAME} PRIVATE
    -fuse-ld=lld
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-Wl,-z,relro,-z,now -Wl,--as-needed>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION    ON
    PUBLIC_HEADER                   fileman.hpp
)

install(TARGETS                 ${PROJECT_NAME}
    ${${PROJECT_NAME}_output_targets}
    EXPORT                      FileManTargets
    PUBLIC_HEADER   DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/HasanEsa/FileMan"
)
install(FILES
    "${${PROJECT_NAME}_qmldir}"
    "${${PROJECT_NAME}_typeinfo}"
    DESTINATION "${QT6_INSTALL_QML}/HasanEsa/FileMan"
)
write_basic_package_version_file(
    FileManConfigVersion.cmake
    VERSION         ${PROJECT_VERSION}
    COMPATIBILITY   AnyNewerVersion
)
install(EXPORT  FileManTargets
    FILE        FileManTargets.cmake
    NAMESPACE   HasanEsa::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FileMan"
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/FileManConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FileManConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FileMan"
)
