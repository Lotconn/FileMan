set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

macro(package_add_test TESTNAME)
  # create an executable in which the tests will be stored
  qt_add_executable(${TESTNAME} ${ARGN})

  target_include_directories(${TESTNAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(${TESTNAME} PRIVATE Qt::Core Lotconn::FileMan)
  set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
  add_test(NAME ${TESTNAME} COMMAND ${CMAKE_COMMAND} -E env $<TARGET_FILE:${TESTNAME}>)
endmacro()

macro(qml_add_module_test TESTNAME SOURCES QML_FILES)
  find_package(Qt6 REQUIRED COMPONENTS Core Qml)

  set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/)

  string(REGEX REPLACE "^.*-" "" uri_name ${TESTNAME})

  get_target_property(fm_plugin_type Lotconn::FileMan TYPE)

  qt_add_executable(${TESTNAME} ${SOURCES})

  target_compile_definitions(${TESTNAME} PRIVATE APPNAME="${uri_name}")

  qt_add_qml_module(${TESTNAME}
    URI ${uri_name}
    QML_FILES ${QML_FILES}
    DEPENDENCIES Lotconn.FileMan
  )

  set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
  target_include_directories(${TESTNAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )

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
  target_include_directories(${TESTNAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(${TESTNAME} PRIVATE Qt::Core Qt::Qml Lotconn::FileMan)

  qt_add_resources(${TESTNAME} "qml-files-${app_name}" PREFIX "${app_name}" BASE "${dir_name}" FILES "${QML_FILES}")

  add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
endmacro()

#
# Smoke tests
#

add_test(NAME test-libBuild COMMAND
  "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --parallel
)

package_add_test(test-instance smokeTest/test-instance.cpp)
set_tests_properties(test-instance PROPERTIES PASS_REGULAR_EXPRESSION "^([a-zA-Z]:)*([/\\].*)?libTest\n")

package_add_test(destroyInstance smokeTest/destroyInstance.cpp)
set_tests_properties(destroyInstance PROPERTIES PASS_REGULAR_EXPRESSION "no instance")

package_add_test(test-getFileName smokeTest/publicMethods/test-getFileName.cpp)
set_tests_properties(test-getFileName PROPERTIES PASS_REGULAR_EXPRESSION "^myDoc\\.pdf\n")

package_add_test(test-categoryToString smokeTest/publicMethods/test-categoryToString.cpp)
set_tests_properties(test-categoryToString PROPERTIES PASS_REGULAR_EXPRESSION "^audio\nimage\nvideo\n")

package_add_test(test-getCategoryDir smokeTest/publicMethods/test-getCategoryDir.cpp)
set_tests_properties(test-getCategoryDir PROPERTIES PASS_REGULAR_EXPRESSION "^([a-zA-Z]:)*([/\\].*)?apiTest[/\\]audio\n([a-zA-Z]:)*([/\\].*)?apiTest[/\\]image\n([a-zA-Z]:)*([/\\].*)?apiTest[/\\]video\n")

package_add_test(test-getDataDir smokeTest/publicMethods/test-getDataDir.cpp)
set_tests_properties(test-getDataDir PROPERTIES PASS_REGULAR_EXPRESSION "^([a-zA-Z]:)*([/\\].*)?apiTest\n")

package_add_test(test-addFile smokeTest/publicMethods/test-addFile.cpp)
set_tests_properties(test-addFile PROPERTIES
  PASS_REGULAR_EXPRESSION "\n0 file://(/.*)?apiTest/audio/a00\.ac3(\.lnk)?\n0 file://(/.*)?apiTest/image/i00\.png(\.lnk)?\n0 file://(/.*)?apiTest/audio/a01\.flac(\.lnk)?\n0 file://(/.*)?apiTest/video/v00\.mp4(\.lnk)?\n"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

package_add_test(test-rmFile smokeTest/publicMethods/test-rmFile.cpp)
set_tests_properties(test-rmFile PROPERTIES PASS_REGULAR_EXPRESSION "\n0 0\n")

package_add_test(test-refreshStore smokeTest/publicMethods/test-refreshStore.cpp)

qml_add_module_test(test-qmlModule-getDataDir smokeTest/QmlModule/main.cpp
  smokeTest/QmlModule/getDataDir/Main.qml
)
set_tests_properties(test-qmlModule-getDataDir PROPERTIES
  PASS_REGULAR_EXPRESSION "^qml:[ \t]+([a-zA-Z]:)*([/\\].*)?ModuleTest[/\\]getDataDir\n"
)

qml_add_module_test(test-qmlModule-categoryToString
  smokeTest/QmlModule/main.cpp
  smokeTest/QmlModule/categoryToString/Main.qml
)
set_tests_properties(test-qmlModule-categoryToString PROPERTIES
  PASS_REGULAR_EXPRESSION "^qml:[ \t]+\nqml:[ \t]+audio\nqml:[ \t]+image\nqml:[ \t]+video\nqml:[ \t]+\n"
)

qml_add_module_test(test-qmlModule-addFile smokeTest/QmlModule/main.cpp
  smokeTest/QmlModule/addFile/Main.qml
)
set_tests_properties(test-qmlModule-addFile PROPERTIES
  PASS_REGULAR_EXPRESSION "\nqml:[ \t]+0 file://(/.*)?ModuleTest/addFile/audio/a00\.ac3(\.lnk)?\nqml:[ \t]+0 file://(/.*)?ModuleTest/addFile/image/i00\.png(\.lnk)?\nqml:[ \t]+0 file://(/.*)?ModuleTest/addFile/audio/a01\.flac(\.lnk)?\nqml:[ \t]+0 file://(/.*)?ModuleTest/addFile/video/v00\.mp4(\.lnk)?\n"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

qml_add_module_test(test-qmlModule-rmFile smokeTest/QmlModule/main.cpp smokeTest/QmlModule/rmFile/Main.qml)
set_tests_properties(test-qmlModule-rmFile PROPERTIES
  PASS_REGULAR_EXPRESSION "\nqml:[ \t]+0 0\n"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

qml_add_rootctx_test(test-qmlRootCtx-getDataDir smokeTest/QmlRootCtx/main.cpp
  smokeTest/QmlRootCtx/getDataDir/Main.qml
)
set_tests_properties(test-qmlRootCtx-getDataDir PROPERTIES
  PASS_REGULAR_EXPRESSION "^qml:[ \t]+([a-zA-Z]:)*([/\\].*)?RootCtxTest[/\\]getDataDir\n"
)

qml_add_rootctx_test(test-qmlRootCtx-categoryToString
  smokeTest/QmlRootCtx/main.cpp
  smokeTest/QmlRootCtx/categoryToString/Main.qml
)
set_tests_properties(test-qmlRootCtx-categoryToString PROPERTIES
  PASS_REGULAR_EXPRESSION "^qml:[ \t]+\nqml:[ \t]+audio\nqml:[ \t]+image\nqml:[ \t]+video\nqml:[ \t]+\n"
)

qml_add_rootctx_test(test-qmlRootCtx-addFile smokeTest/QmlRootCtx/main.cpp
  smokeTest/QmlRootCtx/addFile/Main.qml
)
set_tests_properties(test-qmlRootCtx-addFile PROPERTIES
  PASS_REGULAR_EXPRESSION "\nqml:[ \t]+0 file://(/.*)?RootCtxTest/addFile/audio/a00\.ac3(\.lnk)?\nqml:[ \t]+0 file://(/.*)?RootCtxTest/addFile/image/i00\.png(\.lnk)?\nqml:[ \t]+0 file://(/.*)?RootCtxTest/addFile/audio/a01\.flac(\.lnk)?\nqml:[ \t]+0 file://(/.*)?RootCtxTest/addFile/video/v00\.mp4(\.lnk)?\n"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

qml_add_rootctx_test(test-qmlRootCtx-rmFile smokeTest/QmlRootCtx/main.cpp smokeTest/QmlRootCtx/rmFile/Main.qml)
set_tests_properties(test-qmlRootCtx-rmFile PROPERTIES
  PASS_REGULAR_EXPRESSION "\nqml:[ \t]+0 0\n"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)


#
# Black-box tests
#

package_add_test(emptyAppName blackBox/emptyAppName.cpp)
set_tests_properties(emptyAppName PROPERTIES PASS_REGULAR_EXPRESSION "applicationName.* is empty")

package_add_test(noInstance blackBox/noInstance.cpp)
set_tests_properties(noInstance PROPERTIES PASS_REGULAR_EXPRESSION "no instance")

package_add_test(doubleInstance blackBox/doubleInstance.cpp)
set_tests_properties(doubleInstance PROPERTIES PASS_REGULAR_EXPRESSION "only one instance.*exist")
