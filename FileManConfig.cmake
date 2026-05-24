include(CMakeFindDependencyMacro)

find_dependency(Qt6 REQUIRED COMPONENTS Core Qml)

include("${CMAKE_CURRENT_LIST_DIR}/FileManTargets.cmake")