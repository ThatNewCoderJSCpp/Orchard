cmake_minimum_required(VERSION 3.18)
project(orchard VERSION 1.1.1 DESCRIPTION "Orchard: a light, fast 2D and 3D game engine and editor built with Fizmo, Grove and Walnut" LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ORCHARD_BUILD_EDITOR "Build the Orchard editor (fetches Fizmo, Grove and Walnut); turn off to build only Heartwood and its tests" ON)
option(ORCHARD_BUILD_TESTS "Build the Orchard tests" ON)
option(ORCHARD_BUILD_XYLEM "Build Xylem, Orchard's physics (fetches Fizmo)" ON)
option(ORCHARD_BUILD_CHLOROPHYLL "Build Chlorophyll, Orchard's lighting (fetches Fizmo)" ON)
option(ORCHARD_BUNDLE "Run orchard-host --bundle as part of the build to make orchard with its window built in (turn off when cross-compiling)" ON)
option(ORCHARD_PACKAGE "Link orchard so it runs on machines without the build's C++ runtime, for release downloads" OFF)
set(ORCHARD_FIZMO_TAG "v1.2.1" CACHE STRING "Fizmo git tag to fetch")
set(ORCHARD_GROVE_TAG "v1.0.1" CACHE STRING "Grove git tag to fetch")
set(ORCHARD_WALNUT_TAG "v1.1.0" CACHE STRING "Walnut git tag to fetch")
set(ORCHARD_ANDROID_LIBS_DIR "${CMAKE_BINARY_DIR}/android-libs" CACHE PATH "Where the Android build puts <abi>/liborchard_android.so for grove build --runtime-lib")

if(ANDROID)
    set(ORCHARD_BUNDLE OFF CACHE BOOL "" FORCE)
    set(ORCHARD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ORCHARD_WITH_JAVA OFF CACHE BOOL "" FORCE)
    set(ORCHARD_PACKAGE OFF CACHE BOOL "" FORCE)
endif()

if(WIN32)
    enable_language(RC)
endif()

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

function(orchard_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

configure_file(cmake/version.hpp.in "${CMAKE_BINARY_DIR}/generated/orchard/version.hpp" @ONLY)

add_library(orchard_heartwood STATIC
    Source/orchard/math.cpp
    Source/orchard/scene.cpp
    Source/orchard/project.cpp
    Source/orchard/graph.cpp
    Source/orchard/viewport.cpp
    Source/orchard/scripting.cpp
    Source/orchard/axes.cpp)
add_library(orchard::heartwood ALIAS orchard_heartwood)
target_compile_features(orchard_heartwood PUBLIC cxx_std_17)
target_include_directories(orchard_heartwood PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(orchard_heartwood PUBLIC ${CMAKE_DL_LIBS})
target_compile_definitions(orchard_heartwood PRIVATE "ORCHARD_SOURCE_INCLUDE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/Include\"")
orchard_warnings(orchard_heartwood)

option(ORCHARD_WITH_LUA "Embed Lua so scripts can be written in Lua (fetches Lua's source)" ON)
option(ORCHARD_WITH_JAVA "Let scripts be written in Java when a JDK's headers are found" ON)
set(ORCHARD_LUA_TAG "v5.4.7" CACHE STRING "Lua git tag to fetch")

target_sources(orchard_heartwood PRIVATE Source/orchard/script_dotnet.cpp)

include(FetchContent)
if(ORCHARD_WITH_LUA)
    FetchContent_Declare(orchard_lua
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG        ${ORCHARD_LUA_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(orchard_lua)
    add_library(orchard_lua STATIC "${orchard_lua_SOURCE_DIR}/onelua.c")
    set_target_properties(orchard_lua PROPERTIES LINKER_LANGUAGE C POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(orchard_lua PRIVATE MAKE_LIB)
    if(NOT WIN32)
        target_compile_definitions(orchard_lua PRIVATE LUA_USE_POSIX)
        target_link_libraries(orchard_lua PUBLIC m)
    endif()
    target_include_directories(orchard_lua SYSTEM PUBLIC "${orchard_lua_SOURCE_DIR}")
    target_sources(orchard_heartwood PRIVATE Source/orchard/script_lua.cpp)
    target_link_libraries(orchard_heartwood PUBLIC orchard_lua)
    target_compile_definitions(orchard_heartwood PRIVATE ORCHARD_WITH_LUA)
endif()

if(ORCHARD_WITH_JAVA)
    find_package(JNI QUIET)
    if(JAVA_INCLUDE_PATH)
        target_sources(orchard_heartwood PRIVATE Source/orchard/script_java.cpp)
        target_include_directories(orchard_heartwood SYSTEM PRIVATE "${JAVA_INCLUDE_PATH}" "${JAVA_INCLUDE_PATH2}")
        target_compile_definitions(orchard_heartwood PRIVATE ORCHARD_WITH_JAVA)
    else()
        message(STATUS "Orchard: no JDK headers found, so Java scripts are off (set JAVA_HOME to turn them on)")
    endif()
endif()

if(ORCHARD_BUILD_TESTS)
    enable_testing()
    add_executable(orchard_tests Tests/orchard_tests.cpp)
    target_link_libraries(orchard_tests PRIVATE orchard_heartwood)
    orchard_warnings(orchard_tests)
    add_test(NAME orchard_tests COMMAND orchard_tests)
    if(ORCHARD_WITH_LUA)
        target_compile_definitions(orchard_tests PRIVATE ORCHARD_TEST_LUA)
    endif()
endif()

if(NOT ORCHARD_BUILD_EDITOR AND NOT ORCHARD_BUILD_XYLEM AND NOT ORCHARD_BUILD_CHLOROPHYLL)
    return()
endif()

set(ORCHARD_FETCH_EXTRA "")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
    list(APPEND ORCHARD_FETCH_EXTRA SYSTEM)
endif()

FetchContent_Declare(fizmo
    GIT_REPOSITORY https://github.com/ThatNewCoderJSCpp/Fizmo.git
    GIT_TAG        ${ORCHARD_FIZMO_TAG}
    GIT_SHALLOW    TRUE
    ${ORCHARD_FETCH_EXTRA})

if(ORCHARD_BUILD_EDITOR)
    set(GROVE_BUILD_RUNTIME ON CACHE BOOL "" FORCE)
    set(GROVE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    if(ANDROID)
        set(GROVE_WITH_WALNUT OFF CACHE BOOL "" FORCE)
    else()
        set(GROVE_WITH_WALNUT ON CACHE BOOL "" FORCE)
    endif()
    set(GROVE_WALNUT_TAG ${ORCHARD_WALNUT_TAG} CACHE STRING "" FORCE)
    set(GROVE_FIZMO_TAG ${ORCHARD_FIZMO_TAG} CACHE STRING "" FORCE)
    FetchContent_Declare(grove
        GIT_REPOSITORY https://github.com/ThatNewCoderJSCpp/Grove.git
        GIT_TAG        ${ORCHARD_GROVE_TAG}
        GIT_SHALLOW    TRUE
        ${ORCHARD_FETCH_EXTRA})
    FetchContent_MakeAvailable(fizmo grove)
else()
    FetchContent_MakeAvailable(fizmo)
endif()

if(TARGET fizmo::physics)
    set(ORCHARD_FIZMO_PHYSICS fizmo::physics)
else()
    set(ORCHARD_FIZMO_PHYSICS fizmo::fizmo)
endif()

add_library(orchard_xylem STATIC Source/xylem/xylem.cpp)
add_library(orchard::xylem ALIAS orchard_xylem)
target_link_libraries(orchard_xylem PUBLIC orchard_heartwood ${ORCHARD_FIZMO_PHYSICS})
orchard_warnings(orchard_xylem)

if(ORCHARD_BUILD_TESTS)
    add_executable(xylem_tests Tests/xylem_tests.cpp)
    target_link_libraries(xylem_tests PRIVATE orchard_xylem)
    orchard_warnings(xylem_tests)
    add_test(NAME xylem_tests COMMAND xylem_tests)
endif()

if(TARGET fizmo::lighting)
    set(ORCHARD_FIZMO_LIGHTING fizmo::lighting)
else()
    set(ORCHARD_FIZMO_LIGHTING fizmo::fizmo)
endif()

if(ORCHARD_BUILD_CHLOROPHYLL OR ORCHARD_BUILD_EDITOR)
    add_library(orchard_chlorophyll STATIC Source/chlorophyll/chlorophyll.cpp)
    add_library(orchard::chlorophyll ALIAS orchard_chlorophyll)
    target_link_libraries(orchard_chlorophyll PUBLIC orchard_heartwood ${ORCHARD_FIZMO_LIGHTING})
    orchard_warnings(orchard_chlorophyll)

    if(ORCHARD_BUILD_TESTS)
        add_executable(chlorophyll_tests Tests/chlorophyll_tests.cpp)
        target_link_libraries(chlorophyll_tests PRIVATE orchard_chlorophyll orchard_xylem)
        orchard_warnings(chlorophyll_tests)
        add_test(NAME chlorophyll_tests COMMAND chlorophyll_tests)
    endif()
endif()

if(NOT ORCHARD_BUILD_EDITOR)
    return()
endif()

if(ANDROID)
    add_library(orchard_android SHARED Editor/orchard.cpp Editor/gpu_view.cpp Editor/particle_view.cpp Editor/axes_preview.cpp)
    target_link_libraries(orchard_android PRIVATE orchard_heartwood orchard_xylem orchard_chlorophyll grove::core grove::runtime)
    target_link_options(orchard_android PRIVATE
        "LINKER:-z,max-page-size=16384"
        "LINKER:-u,ANativeActivity_onCreate"
        "LINKER:--gc-sections"
        "$<$<NOT:$<CONFIG:Debug>>:LINKER:--strip-debug>"
        "LINKER:--version-script=${grove_SOURCE_DIR}/cmake/android_exports.map")
    set_property(TARGET orchard_android APPEND PROPERTY LINK_DEPENDS "${grove_SOURCE_DIR}/cmake/android_exports.map")
    set_target_properties(orchard_android PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${ORCHARD_ANDROID_LIBS_DIR}/${ANDROID_ABI}")
    orchard_warnings(orchard_android)
    return()
endif()

add_executable(orchard-host Editor/orchard.cpp Editor/gpu_view.cpp Editor/particle_view.cpp Editor/axes_preview.cpp)
target_link_libraries(orchard-host PRIVATE orchard_heartwood orchard_xylem orchard_chlorophyll grove::core grove::runtime)
target_compile_definitions(orchard-host PRIVATE "ORCHARD_SOURCE_UI_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/UI\"")
orchard_warnings(orchard-host)
if(WIN32)
    set_target_properties(orchard-host PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    set(ORCHARD_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/windows/orchard.ico")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Packaging/windows/orchard.rc.in" "${CMAKE_BINARY_DIR}/generated/orchard.rc" @ONLY)
    target_sources(orchard-host PRIVATE "${CMAKE_BINARY_DIR}/generated/orchard.rc")
endif()
if(ORCHARD_PACKAGE)
    if(MSVC)
        message(FATAL_ERROR "ORCHARD_PACKAGE needs a GCC or Clang toolchain")
    elseif(MINGW)
        target_link_options(orchard-host PRIVATE -static)
    elseif(NOT APPLE)
        target_link_options(orchard-host PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endif()

add_custom_command(TARGET orchard-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/UI" "$<TARGET_FILE_DIR:orchard-host>/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Include/orchard/script" "$<TARGET_FILE_DIR:orchard-host>/include/orchard/script"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Include/orchard/walnut" "$<TARGET_FILE_DIR:orchard-host>/include/orchard/walnut")

file(GLOB_RECURSE ORCHARD_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.grove" "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.bark" "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.wal")
set(ORCHARD_BUNDLED "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/orchard${CMAKE_EXECUTABLE_SUFFIX}")
set(ORCHARD_BUNDLE_DEPENDS orchard-host)
if(TARGET walnut)
    list(APPEND ORCHARD_BUNDLE_DEPENDS walnut)
endif()
if(ORCHARD_BUNDLE)
    add_custom_command(OUTPUT "${ORCHARD_BUNDLED}"
        COMMAND ${CMAKE_COMMAND} -E env "ORCHARD_UI_DIR=${CMAKE_CURRENT_SOURCE_DIR}/UI" $<TARGET_FILE:orchard-host> --bundle "${ORCHARD_BUNDLED}"
        DEPENDS ${ORCHARD_BUNDLE_DEPENDS} ${ORCHARD_UI_FILES}
        COMMENT "Building orchard with its window and compiled script built in"
        VERBATIM)
    add_custom_target(orchard ALL DEPENDS "${ORCHARD_BUNDLED}")
    install(PROGRAMS "${ORCHARD_BUNDLED}" DESTINATION bin)
    install(DIRECTORY Include/orchard/script Include/orchard/walnut DESTINATION include/orchard)
endif()
