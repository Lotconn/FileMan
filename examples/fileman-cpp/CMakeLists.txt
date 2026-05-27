#[[

                          MIT License

                      Copyright HasanEsa 2026

This is the CMakeLists file for the example interactive cli app demonstrating
the integration of FileMan class in a Qt/C++ implementation.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

]]

#[KEY:Minimum 3.16 recommended]
cmake_minimum_required(VERSION 3.16...4.3)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(fileman-cpp
        DESCRIPTION "Example C++ program implementing FileMan"
        LANGUAGES CXX
    )

#[KEY:BEGIN:Recommended]
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#[KEY:END]
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    qt_policy(SET QTP0003 NEW)
#[KEY:Recommended]
    qt_standard_project_setup(REQUIRES 6.9)
endif()

if(TARGET FileMan)
    message(STATUS "Building fileman-cpp example in-tree with root project")
else()
#[KEY]
    find_package(FileMan REQUIRED)
endif()

set(default_build_type "Release")

if((CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR) AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
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

qt_add_executable(fileman-cpp main.cpp)
# target_compile_definitions(fileman-cpp PRIVATE
#     QT_ENABLE_STRICT_MODE_UP_TO=0x060900
# )
# target_compile_features(fileman-cpp PRIVATE cxx_std_17)
target_compile_options(fileman-cpp PRIVATE
    -pipe -Wp,-D_FORTIFY_SOURCE=3 -Wp,-D_GLIBCXX_ASSERTIONS -fexceptions
    -fstack-protector-strong -fasynchronous-unwind-tables
    -fstack-clash-protection -fcf-protection -mno-omit-leaf-frame-pointer
    $<$<CONFIG:Release>:-O3>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-O2 -g -grecord-gcc-switches>
)
#[KEY:Include HasanEsa::FileMan]
target_link_libraries(fileman-cpp PRIVATE Qt::Core HasanEsa::FileMan)
target_link_options(fileman-cpp PRIVATE
    -fuse-ld=lld
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-Wl,-z,relro,-z,now -Wl,--as-needed>
)

set_target_properties(fileman-cpp PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)