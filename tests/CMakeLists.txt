set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

macro(package_add_test TESTNAME)
  # create an executable in which the tests will be stored
  qt_add_executable(${TESTNAME} ${ARGN})
  set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
  target_include_directories(${TESTNAME}
                             PUBLIC
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include>)
  target_link_libraries(${TESTNAME} PRIVATE Qt::Core Lotconn::FileMan)
  add_test(NAME ${TESTNAME} COMMAND ${CMAKE_COMMAND} -E env $<TARGET_FILE:${TESTNAME}>)
endmacro()

macro(qml_add_module_test TESTNAME SOURCES QML_FILES)
  find_package(Qt6 REQUIRED COMPONENTS Core Qml)
  set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/)
  string(REGEX REPLACE "^.*-" "" uri_name ${TESTNAME})
  qt_add_executable(${TESTNAME} ${SOURCES})
  qt_add_qml_module(${TESTNAME} URI ${uri_name} QML_FILES ${QML_FILES} DEPENDENCIES Lotconn.FileMan)
  set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
  target_compile_definitions(${TESTNAME} PRIVATE APPNAME="${uri_name}")
  target_include_directories(${TESTNAME}
                             PRIVATE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include>)

  get_target_property(fm_plugin_type Lotconn::FileMan TYPE)
  if(fm_plugin_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(${TESTNAME} PRIVATE FM_STATIC_PLUGIN)
    target_link_libraries(${TESTNAME} PRIVATE Qt::Core Qt::Qml FileManplugin)
  else()
    target_link_libraries(${TESTNAME} PRIVATE Qt::Core Qt::Qml Lotconn::FileMan)
  endif()
  unset(fm_plugin_type)

  add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
endmacro()

macro(qml_add_rootctx_test TESTNAME SOURCES QML_FILES)
  find_package(Qt6 REQUIRED COMPONENTS Core Qml)
  get_filename_component(dir_name ${QML_FILES} DIRECTORY)
  string(REGEX REPLACE "^.*-" "" app_name ${TESTNAME})
  qt_add_executable(${TESTNAME} ${SOURCES})
  set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
  target_compile_definitions(${TESTNAME} PRIVATE APPNAME="${app_name}")
  target_include_directories(${TESTNAME}
                             PRIVATE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                               $<INSTALL_INTERFACE:include>)
  target_link_libraries(${TESTNAME} PRIVATE Qt::Core Qt::Qml Lotconn::FileMan)
  qt_add_resources(${TESTNAME} "qml-files-${app_name}"
                   PREFIX "${app_name}"
                   BASE "${dir_name}"
                   FILES "${QML_FILES}")

  add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
endmacro()

#
# Smoke tests
#

add_test(NAME test-libBuild COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --parallel)

package_add_test(test-instance smokeTest/test-instance.cpp)
set_tests_properties(test-instance
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^file:\/\/([a-zA-Z]:)*(/.*)?FileManTests/libTest/store\n")

package_add_test(destroyInstance smokeTest/destroyInstance.cpp)
set_tests_properties(destroyInstance PROPERTIES PASS_REGULAR_EXPRESSION "No instance constructed")

package_add_test(test-fileName smokeTest/publicMethods/test-fileName.cpp)
set_tests_properties(test-fileName PROPERTIES PASS_REGULAR_EXPRESSION "^myDoc\\.pdf\n")

package_add_test(test-toString smokeTest/publicMethods/test-toString.cpp)
set_tests_properties(test-toString PROPERTIES PASS_REGULAR_EXPRESSION "^Audio\nImage\nVideo\n")

package_add_test(test-categoryDir smokeTest/publicMethods/test-categoryDir.cpp)
set_tests_properties(test-categoryDir
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^file:\/\/([a-zA-Z]:)*(/.*)?FileManTests/apiTest/store/audios\nfile:\/\/([a-zA-Z]:)*(/.*)?FileManTests/apiTest/store/images\nfile:\/\/([a-zA-Z]:)*(/.*)?FileManTests/apiTest/store/videos\n"
)

package_add_test(test-storeDir smokeTest/publicMethods/test-storeDir.cpp)
set_tests_properties(test-storeDir
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^file:\/\/([a-zA-Z]:)*(/.*)?FileManTests/apiTest/store")

package_add_test(test-addFile smokeTest/publicMethods/test-addFile.cpp)
set_tests_properties(test-addFile
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "\n0 file://(/.*)?FileManTests/apiTest/store/audios/a00\.ac3(\.lnk)?\n0 file://(/.*)?FileManTests/apiTest/store/images/i00\.png(\.lnk)?\n0 file://(/.*)?FileManTests/apiTest/store/audios/a01\.flac(\.lnk)?\n0 file://(/.*)?FileManTests/apiTest/store/videos/v00\.mp4(\.lnk)?\n"
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

package_add_test(test-rmFile smokeTest/publicMethods/test-rmFile.cpp)
set_tests_properties(test-rmFile PROPERTIES PASS_REGULAR_EXPRESSION "\n0 0\n")

package_add_test(test-refreshStore smokeTest/publicMethods/test-refreshStore.cpp)

#
# Qml module tests
#

qml_add_module_test(test-qmlModule-storeDir smokeTest/QmlModule/main.cpp
                    smokeTest/QmlModule/storeDir/Main.qml)
set_tests_properties(test-qmlModule-storeDir
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^qml:[ \t]+file:\/\/([a-zA-Z]:)*(/.*)?FileManModuleTests/storeDir/store\n")

qml_add_module_test(test-qmlModule-categoryToString smokeTest/QmlModule/main.cpp
                    smokeTest/QmlModule/categoryToString/Main.qml)
set_tests_properties(test-qmlModule-categoryToString
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^qml:[ \t]+\nqml:[ \t]+Audio\nqml:[ \t]+Image\nqml:[ \t]+Video\nqml:[ \t]+\n"
)

qml_add_module_test(test-qmlModule-addFile smokeTest/QmlModule/main.cpp
                    smokeTest/QmlModule/addFile/Main.qml)
set_tests_properties(test-qmlModule-addFile
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "\nqml:[ \t]+0 file://(/.*)?FileManModuleTests/addFile/store/audios/a00\.ac3(\.lnk)?\nqml:[ \t]+0 file://(/.*)?FileManModuleTests/addFile/store/images/i00\.png(\.lnk)?\nqml:[ \t]+0 file://(/.*)?FileManModuleTests/addFile/store/audios/a01\.flac(\.lnk)?\nqml:[ \t]+0 file://(/.*)?FileManModuleTests/addFile/store/videos/v00\.mp4(\.lnk)?\n"
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qml_add_module_test(test-qmlModule-rmFile smokeTest/QmlModule/main.cpp
                    smokeTest/QmlModule/rmFile/Main.qml)
set_tests_properties(test-qmlModule-rmFile
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "\nqml:[ \t]+0 0\n" WORKING_DIRECTORY
                       ${CMAKE_CURRENT_SOURCE_DIR})

#
# QML root context tests
#

qml_add_rootctx_test(test-qmlRootCtx-storeDir smokeTest/QmlRootCtx/main.cpp
                     smokeTest/QmlRootCtx/storeDir/Main.qml)
set_tests_properties(test-qmlRootCtx-storeDir
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^qml:[ \t]+file:\/\/([a-zA-Z]:)*(/.*)?FileManRootCtxTests/storeDir/store\n")

qml_add_rootctx_test(test-qmlRootCtx-categoryToString smokeTest/QmlRootCtx/main.cpp
                     smokeTest/QmlRootCtx/categoryToString/Main.qml)
set_tests_properties(test-qmlRootCtx-categoryToString
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "^qml:[ \t]+\nqml:[ \t]+Audio\nqml:[ \t]+Image\nqml:[ \t]+Video\nqml:[ \t]+\n"
)

qml_add_rootctx_test(test-qmlRootCtx-addFile smokeTest/QmlRootCtx/main.cpp
                     smokeTest/QmlRootCtx/addFile/Main.qml)
set_tests_properties(test-qmlRootCtx-addFile
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "\nqml:[ \t]+0 file://(/.*)?FileManRootCtxTests/addFile/store/audios/a00\.ac3(\.lnk)?\nqml:[ \t]+0 file://(/.*)?FileManRootCtxTests/addFile/store/images/i00\.png(\.lnk)?\nqml:[ \t]+0 file://(/.*)?FileManRootCtxTests/addFile/store/audios/a01\.flac(\.lnk)?\nqml:[ \t]+0 file://(/.*)?FileManRootCtxTests/addFile/store/videos/v00\.mp4(\.lnk)?\n"
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qml_add_rootctx_test(test-qmlRootCtx-rmFile smokeTest/QmlRootCtx/main.cpp
                     smokeTest/QmlRootCtx/rmFile/Main.qml)
set_tests_properties(test-qmlRootCtx-rmFile
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "\nqml:[ \t]+0 0\n" WORKING_DIRECTORY
                       ${CMAKE_CURRENT_SOURCE_DIR})

#
# Black-box tests
#

package_add_test(emptyAppName blackBox/emptyAppName.cpp)
set_tests_properties(emptyAppName
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "cannot safely isolate the store. Aborting"
                       FAIL_REGULAR_EXPRESSION "^file://([a-zA-Z]:)*(/.*)?\n")

package_add_test(noInstance blackBox/noInstance.cpp)
set_tests_properties(noInstance
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "No instance constructed" FAIL_REGULAR_EXPRESSION
                       "^file://([a-zA-Z]:)*(/.*)?FileManTests/libTest/store\n")

package_add_test(doubleInstance blackBox/doubleInstance.cpp)
set_tests_properties(doubleInstance
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "only one instance.*exist" FAIL_REGULAR_EXPRESSION
                       "^file://([a-zA-Z]:)*(/.*)?FileManTests/libTest/store\n")

#
# White-box tests
#

package_add_test(add_file_t whiteBox/add_file_t.cpp)
set_tests_properties(add_file_t
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "^Copied: true\nReferenced: true\nMoved: true\n")

package_add_test(rm_file_t whiteBox/rm_file_t.cpp)
set_tests_properties(rm_file_t
                     PROPERTIES
                       PASS_REGULAR_EXPRESSION "^Copied: true\nReferenced: true\nMoved: true\n")
