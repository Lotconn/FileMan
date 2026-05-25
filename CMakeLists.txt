cmake_minimum_required(VERSION 3.16...4.3)

include(CMakePrintHelpers)
include(CMakePackageConfigHelpers)

project(FileMan
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
set(FM_NS HasanEsa CACHE STRING "Namespace for the project")
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

mark_as_advanced(FORCE FM_NS)
mark_as_advanced(FORCE PROJECT_NAME_LOWER)

find_package(Qt6 REQUIRED COMPONENTS Core Qml)
find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

qt_policy(SET QTP0003 NEW)
qt_policy(SET QTP0005 NEW)
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

if("${FM_BUILD_AS}" STREQUAL "SHARED")
    qt_add_qml_module(${PROJECT_NAME}
        URI             HasanEsa.FileMan
        VERSION         0.2
        ${FM_BUILD_AS}
        SOURCES         fileman.cpp fileman.hpp
        OUTPUT_TARGETS  fm_qml_mod_output_targets
    )
    set_target_properties(${PROJECT_NAME}plugin PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION ON
        OUTPUT_NAME "filemanplugin"
    )
else()
    qt_add_qml_module(${PROJECT_NAME}
        URI             HasanEsa.FileMan
        VERSION         0.2
        ${FM_BUILD_AS}
        NO_PLUGIN
        SOURCES         fileman.cpp fileman.hpp
        OUTPUT_TARGETS  fm_qml_mod_output_targets
    )
endif()
qt_query_qml_module(${PROJECT_NAME}
    URI             fm_qml_mod_uri
    VERSION         fm_qml_mod_version
    TARGET_PATH     fm_qml_mod_target_path
    QMLDIR          fm_qml_mod_qmldir
    TYPEINFO        fm_qml_mod_typeinfo
)

# add_library(${PROJECT_NAME}
#     ${CMAKE_CURRENT_SOURCE_DIR}/fileman.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fileman.hpp
# )

set(FM_BUILD_EXAMPLES OFF CACHE BOOL "Build examples in the project")

if(FM_BUILD_EXAMPLES)
    add_subdirectory(examples/)
endif()

add_library(${FM_NS}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

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
target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core Qt::Qml)
target_link_options(${PROJECT_NAME} PRIVATE
    -fuse-ld=lld
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-Wl,-z,relro,-z,now -Wl,--as-needed>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION    ON
    PUBLIC_HEADER                   fileman.hpp
    OUTPUT_NAME                     ${PROJECT_NAME_LOWER}
)

install(TARGETS                 ${PROJECT_NAME}
    ${fm_qml_mod_output_targets}
    EXPORT                      ${PROJECT_NAME}Targets
    OBJECTS         DESTINATION "${CMAKE_INSTALL_LIBDIR}/${FM_NS}/${PROJECT_NAME}"
    PUBLIC_HEADER   DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${FM_NS}/${PROJECT_NAME}"
)
if("${FM_BUILD_AS}" STREQUAL "SHARED")
    install(TARGETS "${PROJECT_NAME}plugin"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
    )
endif()
install(FILES
    "${fm_qml_mod_qmldir}"
    "${fm_qml_mod_typeinfo}"
    DESTINATION "${QT6_INSTALL_QML}/${FM_NS}/${PROJECT_NAME}"
)
write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION         ${PROJECT_VERSION}
    COMPATIBILITY   AnyNewerVersion
)
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
