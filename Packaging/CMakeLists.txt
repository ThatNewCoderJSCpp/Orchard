cmake_minimum_required(VERSION 3.18)
project(orchard_packaging NONE)

set(ORCHARD_STAGE "" CACHE PATH "Folder holding everything that goes in the package, laid out as it is installed")
set(ORCHARD_VERSION "" CACHE STRING "Orchard version")
set(ORCHARD_PLATFORM "" CACHE STRING "Platform name used in package file names, like linux-x86_64 or windows-ucrt64")
set(ORCHARD_PACKAGE_SUFFIX "" CACHE STRING "Text put after the platform in package file names, like -with-compiler")
set(ORCHARD_GENERATORS "TGZ" CACHE STRING "Package formats cpack produces")

foreach(v STAGE VERSION PLATFORM)
    if(NOT ORCHARD_${v})
        message(FATAL_ERROR "Set ORCHARD_${v}; see .github/workflows/release.yml for how packages are made")
    endif()
endforeach()
get_filename_component(ORCHARD_STAGE "${ORCHARD_STAGE}" ABSOLUTE)
if(NOT EXISTS "${ORCHARD_STAGE}/bin")
    message(FATAL_ERROR "${ORCHARD_STAGE} has no bin folder; make it with .github/scripts/stage.sh")
endif()

if(ORCHARD_PLATFORM MATCHES "arm64")
    set(_orchard_arch arm64)
elseif(ORCHARD_PLATFORM MATCHES "i686")
    set(_orchard_arch i686)
else()
    set(_orchard_arch x86_64)
endif()

install(DIRECTORY "${ORCHARD_STAGE}/" DESTINATION . USE_SOURCE_PERMISSIONS)

set(CPACK_PACKAGE_NAME Orchard)
set(CPACK_PACKAGE_VENDOR ThatNewCoderJSCpp)
set(CPACK_PACKAGE_CONTACT "ThatNewCoderJSCpp <https://github.com/ThatNewCoderJSCpp/Orchard>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ThatNewCoderJSCpp/Orchard")
set(CPACK_PACKAGE_VERSION ${ORCHARD_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Orchard: a light, fast 2D and 3D game engine and editor")
set(CPACK_PACKAGE_DESCRIPTION "Orchard is a 2D and 3D game engine and editor built with Fizmo, Grove and Walnut. Scripts can be Walnut, C, C++, C#, Java or Lua files, or visual graphs that turn into Walnut.")
set(CPACK_PACKAGE_FILE_NAME Orchard-${ORCHARD_VERSION}-${ORCHARD_PLATFORM}${ORCHARD_PACKAGE_SUFFIX})
set(CPACK_GENERATOR ${ORCHARD_GENERATORS})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_STRIP_FILES OFF)
set(CPACK_THREADS 0)
set(CPACK_PACKAGE_EXECUTABLES "orchard;Orchard")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/CPackOptions.cmake")

set(_orchard_scripts "${CMAKE_BINARY_DIR}/package-scripts")
file(MAKE_DIRECTORY "${_orchard_scripts}/deb")
foreach(script postinst prerm)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/linux/deb-${script}.in" "${_orchard_scripts}/tmp/${script}" @ONLY)
    file(COPY "${_orchard_scripts}/tmp/${script}" DESTINATION "${_orchard_scripts}/deb"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/linux/rpm-post.sh.in" "${_orchard_scripts}/rpm-post.sh" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/linux/rpm-preun.sh.in" "${_orchard_scripts}/rpm-preun.sh" @ONLY)

set(CPACK_DEBIAN_PACKAGE_NAME orchard)
set(CPACK_DEBIAN_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}.deb)
set(CPACK_DEBIAN_PACKAGE_SECTION games)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "g++ | clang")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${_orchard_scripts}/deb/postinst;${_orchard_scripts}/deb/prerm")

set(CPACK_RPM_PACKAGE_NAME orchard)
set(CPACK_RPM_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}.rpm)
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_LICENSE "See https://github.com/ThatNewCoderJSCpp/Orchard")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_AUTOPROV OFF)
set(CPACK_RPM_PACKAGE_SUGGESTS "gcc-c++")
set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /opt)
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${_orchard_scripts}/rpm-post.sh")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${_orchard_scripts}/rpm-preun.sh")
if(_orchard_arch STREQUAL "arm64")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE aarch64)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
elseif(_orchard_arch STREQUAL "i686")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE i686)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
else()
    set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
endif()

set(CPACK_NSIS_PACKAGE_NAME "Orchard ${ORCHARD_VERSION}")
set(CPACK_NSIS_DISPLAY_NAME "Orchard")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Orchard")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Orchard")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/ThatNewCoderJSCpp/Orchard")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\orchard.exe")
set(CPACK_CREATE_DESKTOP_LINKS orchard)
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "orchard.exe")
set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/windows/orchard.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/windows/orchard.ico")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Orchard.Project' '' 'Orchard project'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Orchard.Project\\\\DefaultIcon' '' '$INSTDIR\\\\bin\\\\orchard.exe,0'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Orchard.Project\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\orchard.exe\\\" \\\"%1\\\"'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\.orchard\\\\OpenWithProgids' 'Orchard.Project' ''
    ReadRegStr $0 SHCTX 'Software\\\\Classes\\\\.orchard' ''
    StrCmp $0 '' 0 +2
    WriteRegStr SHCTX 'Software\\\\Classes\\\\.orchard' '' 'Orchard.Project'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Directory\\\\shell\\\\Orchard' '' 'Open with Orchard'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Directory\\\\shell\\\\Orchard' 'Icon' '$INSTDIR\\\\bin\\\\orchard.exe'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Directory\\\\shell\\\\Orchard\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\orchard.exe\\\" \\\"%V\\\"'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Directory\\\\Background\\\\shell\\\\Orchard' '' 'Open with Orchard'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Directory\\\\Background\\\\shell\\\\Orchard' 'Icon' '$INSTDIR\\\\bin\\\\orchard.exe'
    WriteRegStr SHCTX 'Software\\\\Classes\\\\Directory\\\\Background\\\\shell\\\\Orchard\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\orchard.exe\\\" \\\"%V\\\"'
    WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\orchard.exe' '' '$INSTDIR\\\\bin\\\\orchard.exe'
    WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\orchard.exe' 'Path' '$INSTDIR\\\\bin'
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    DeleteRegKey SHCTX 'Software\\\\Classes\\\\Orchard.Project'
    DeleteRegValue SHCTX 'Software\\\\Classes\\\\.orchard\\\\OpenWithProgids' 'Orchard.Project'
    ReadRegStr $0 SHCTX 'Software\\\\Classes\\\\.orchard' ''
    StrCmp $0 'Orchard.Project' 0 +2
    DeleteRegValue SHCTX 'Software\\\\Classes\\\\.orchard' ''
    DeleteRegKey /ifempty SHCTX 'Software\\\\Classes\\\\.orchard\\\\OpenWithProgids'
    DeleteRegKey /ifempty SHCTX 'Software\\\\Classes\\\\.orchard'
    DeleteRegKey SHCTX 'Software\\\\Classes\\\\Directory\\\\shell\\\\Orchard'
    DeleteRegKey SHCTX 'Software\\\\Classes\\\\Directory\\\\Background\\\\shell\\\\Orchard'
    DeleteRegKey SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\orchard.exe'
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
")
if(_orchard_arch STREQUAL "i686")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
else()
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
endif()

include(CPack)
