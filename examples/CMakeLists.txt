cmake_minimum_required(VERSION 3.30...4.3)

project(fileman-cpp
    DESCRIPTION "Example C++ program implementing FileMan"
    LANGUAGES CXX
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(FileMan REQUIRED)
find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

qt_standard_project_setup(REQUIRES 6.9)

set(default_build_type "Release")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
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

add_executable(${PROJECT_NAME} main.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_ENABLE_STRICT_MODE_UP_TO=0x060900
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE
    -pipe -Wp,-D_FORTIFY_SOURCE=3 -Wp,-D_GLIBCXX_ASSERTIONS -fexceptions
    -fstack-protector-strong -fasynchronous-unwind-tables
    -fstack-clash-protection -fcf-protection -mno-omit-leaf-frame-pointer
    $<$<CONFIG:Release>:-O3>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-O2 -g -grecord-gcc-switches>
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core HasanEsa::fileman)
target_link_options(${PROJECT_NAME} PRIVATE
    -fuse-ld=lld
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-Wl,-z,relro,-z,now -Wl,--as-needed>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION    ON
)