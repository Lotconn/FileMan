#[[

                          MIT License

                      Copyright HasanEsa 2026

This is the CMakeLists file for the example app demonstrating modern use of the
FileMan library as a QML module. This approach is recommended over setting
context properties from C++ backend to QML namespace.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

]]

#[KEY:Minimum 3.16 recommended]
cmake_minimum_required(VERSION 3.16...4.3)
set(TARGET_NAME fileman-qml)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(${TARGET_NAME}
		DESCRIPTION "Example QML program implementing FileMan"
		LANGUAGES CXX
	)

#[KEY:BEGIN:Recommended]
	set(CMAKE_CXX_EXTENSIONS			 OFF)
	set(CMAKE_CXX_STANDARD					17)
	set(CMAKE_CXX_STANDARD_REQUIRED	ON)
#[KEY:END]

	set(CMAKE_EXPORT_COMPILE_COMMANDS	ON)
	find_program(CCACHE_PROGRAM ccache)
	if(CCACHE_PROGRAM)
		set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
	endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

#[KEY:Recommended]
	qt_standard_project_setup(REQUIRES 6.9)
endif()

if(TARGET FileMan)
	message(STATUS "Building fileman-qml example in-tree with root project")

#[KEY:When using for submodule; also add "add_subdirectory(path/to/HasanEsa/FileMan)"]
	set(FM_QML_DEP TARGET FileMan)
	mark_as_advanced(FM_QML_DEP)
else()

#[KEY:BEGIN:When using system installed instance]
	find_package(FileMan REQUIRED)
	if(DEFINED FileMan_FOUND)
		message(STATUS "Found FileMan: TRUE")
	endif()
	set(FM_QML_DEP HasanEsa.FileMan)
#[KEY:END]

	mark_as_advanced(FM_QML_DEP)
endif()

set(default_build_type "Release")

if((CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR) AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS
		"Setting build type to '${default_build_type}' as none was specified."
	)
	set(CMAKE_BUILD_TYPE "${default_build_type}"
		CACHE
		STRING	"Choose the type of build."
		FORCE
	)
	set_property(CACHE CMAKE_BUILD_TYPE
		PROPERTY
			STRINGS
				"Debug"
				"MinSizeRel"
				"Release"
				"RelWithDebInfo"
	)
endif()

qt_add_executable(${TARGET_NAME} main.cpp)

#[KEY:BEGIN:FM_QML_DEP shows alternate options for two possible implementations]
qt_add_qml_module(${TARGET_NAME}
	URI						FilemanQml
	QML_FILES			main.qml
	DEPENDENCIES	${FM_QML_DEP}
)
#[KEY:END]

set(FM_PLUGIN_TARGET FileManplugin)

if(TARGET HasanEsa::FileManplugin)
	set(FM_PLUGIN_TARGET HasanEsa::FileManplugin)
endif()

get_target_property(fm_plugin_type ${FM_PLUGIN_TARGET} TYPE)
if(fm_plugin_type STREQUAL "STATIC_LIBRARY")
	target_compile_definitions(${TARGET_NAME} PRIVATE FM_STATIC_PLUGIN)

#[KEY:Required to link FileManplugin explicitly for static building]
	target_link_libraries(${TARGET_NAME} PRIVATE ${FM_PLUGIN_TARGET})
else()

#[KEY:For shared library/regular builds]
	target_link_libraries(${TARGET_NAME} PRIVATE HasanEsa::FileMan)
endif()
unset(fm_plugin_type)

	# target_compile_definitions(${TARGET_NAME} PRIVATE
	#   QT_ENABLE_STRICT_MODE_UP_TO=0x060900
	# )
	# target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
target_compile_options(${TARGET_NAME} PRIVATE
	-Wp,-D_FORTIFY_SOURCE=3
	-Wp,-D_GLIBCXX_ASSERTIONS
	-fexceptions
	-fstack-protector-strong
	-fasynchronous-unwind-tables
	-fstack-clash-protection
	-fcf-protection
	-mno-omit-leaf-frame-pointer
	-pipe
	$<$<CONFIG:Release>:-O3>
	$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-O2 -g -grecord-gcc-switches>
)
target_link_libraries(${TARGET_NAME} PRIVATE
	Qt::Core
	Qt::Qml
	Qt::Quick
	Qt::Widgets
)
target_link_options(${TARGET_NAME} PRIVATE
	-fuse-ld=lld
	$<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-Wl,-z,relro,-z,now -Wl,--as-needed>
)

set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
