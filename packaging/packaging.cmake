# Desktop packaging, included from the top-level CMakeLists.txt only when
# ERDFLOW_PACKAGING is ON. Ordinary development builds never read this file
# and stay exactly as they are.
#
# It turns the erdflow executable into an installable application (a macOS
# .app bundle, a Windows GUI executable with its icon and version details),
# says what `cmake --install` puts where, and prepares the package tools:
#
#   macOS    CPack DragNDrop  -> ERDFlow-<version>-macos-<arch>.dmg
#   Windows  Inno Setup       -> ERDFlow-<version>-windows-x86_64-setup.exe
#   Linux    linuxdeploy      -> ERDFlow-<version>-linux-x86_64.AppImage
#
# Qt stays dynamically linked; macdeployqt and windeployqt (through Qt's own
# deployment script) and linuxdeploy-plugin-qt copy the shared Qt libraries
# and plugins beside the application. Test programs are never installed.
# See packaging/README.md.

include(GNUInstallDirs)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_target_property(erdflow_qt_core_type Qt6::Core TYPE)
if(NOT erdflow_qt_core_type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR "ERDFlow packages require dynamically linked Qt.")
endif()

set(ERDFLOW_BUNDLE_IDENTIFIER "app.erdflow.ERDFlow" CACHE STRING
    "macOS bundle identifier. Must not change once ERDFlow has been distributed.")

# Where the source code of the Qt libraries in the packages is offered. Until
# it is set, QT-LGPL-INFORMATION.md in every package says the package must not
# be distributed (docs/QT_LGPL_COMPLIANCE.md, section 4).
set(ERDFLOW_QT_SOURCE_OFFER "" CACHE STRING
    "Text for QT-LGPL-INFORMATION.md saying where the Qt source code is offered")
option(ERDFLOW_ALLOW_INCOMPLETE_NOTICES
    "Package even when a component's licence is not established; for local test builds only" OFF)

# The copyright line comes from LICENSE, so there is one source for it.
file(STRINGS "${CMAKE_SOURCE_DIR}/LICENSE" erdflow_copyright_lines REGEX "^Copyright")
list(GET erdflow_copyright_lines 0 ERDFLOW_COPYRIGHT)

# The version every package carries is PROJECT_VERSION. The workflow reads it
# from here for the package file names.
file(WRITE "${CMAKE_BINARY_DIR}/packaging/erdflow-version.txt" "${PROJECT_VERSION}\n")

# The architecture as it appears in package names: x86_64 or arm64.
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES erdflow_arch_count)
    if(erdflow_arch_count GREATER 1)
        message(FATAL_ERROR "ERDFlow packages one architecture per macOS build; set CMAKE_OSX_ARCHITECTURES to arm64 or x86_64.")
    endif()
    set(erdflow_arch "${CMAKE_OSX_ARCHITECTURES}")
else()
    set(erdflow_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${erdflow_arch}" erdflow_arch)
if(erdflow_arch MATCHES "^(amd64|x86_64|x64)$")
    set(ERDFLOW_PACKAGE_ARCH "x86_64")
elseif(erdflow_arch MATCHES "^(arm64|aarch64)$")
    set(ERDFLOW_PACKAGE_ARCH "arm64")
else()
    message(FATAL_ERROR "ERDFlow has no package for the ${erdflow_arch} architecture.")
endif()

set(ERDFLOW_PACKAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/dist")
if(NOT APPLE AND NOT ERDFLOW_PACKAGE_ARCH STREQUAL "x86_64")
    message(FATAL_ERROR "The Windows and Linux installers currently support x86_64 only.")
endif()

# QT-LGPL-INFORMATION.md: the Qt version, the source offer and how to replace
# the Qt libraries.
set(ERDFLOW_QT_VERSION "${Qt6_VERSION}")
set(ERDFLOW_QT_VERSION_MAJOR "${Qt6_VERSION_MAJOR}")
if(ERDFLOW_QT_SOURCE_OFFER)
    set(ERDFLOW_QT_SOURCE_TEXT "${ERDFLOW_QT_SOURCE_OFFER}")
else()
    set(ERDFLOW_QT_SOURCE_TEXT
        "**Not yet provided. This package is a test build and must not be distributed.**\nBefore ERDFlow is distributed, the complete source code of the Qt libraries\nin it will be made available from a location under ERDFlow's control, named\nhere.")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/notices/QT-LGPL-INFORMATION.md.in"
               "${CMAKE_BINARY_DIR}/packaging/QT-LGPL-INFORMATION.md" @ONLY)

set(erdflow_notices
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_SOURCE_DIR}/assets/icons-outline/LICENSE-lucide.txt"
    "${CMAKE_CURRENT_LIST_DIR}/notices/THIRD-PARTY-NOTICES.md"
    "${CMAKE_BINARY_DIR}/packaging/QT-LGPL-INFORMATION.md"
    "${CMAKE_CURRENT_LIST_DIR}/notices/LGPL-3.0.txt"
    "${CMAKE_CURRENT_LIST_DIR}/notices/GPL-3.0.txt")

# write-notices.cmake: THIRD-PARTY-LICENSES.txt for the finished package,
# made from what the package contains (notices/package_notices.py).
set(ERDFLOW_NOTICES_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/notices/package_notices.py")
set(ERDFLOW_NOTICES_ARGUMENTS "")
if(NOT ERDFLOW_QT_SOURCE_OFFER)
    string(APPEND ERDFLOW_NOTICES_ARGUMENTS
        " --release-blocker \"Exact corresponding Qt source and build information are not yet offered under ERDFlow control.\"")
endif()
if(APPLE)
    set(ERDFLOW_NOTICES_DIR "Contents/Resources/Licenses")
    set(ERDFLOW_NOTICES_APPLICATION "ERDFlow")
elseif(WIN32)
    set(ERDFLOW_NOTICES_DIR "licenses")
    set(ERDFLOW_NOTICES_APPLICATION "erdflow.exe")
    string(APPEND ERDFLOW_NOTICES_ARGUMENTS
        " --release-blocker \"Verify the exact Visual C++ runtime versions, redistribution entitlement and terms, and the final Inno Setup installer components.\""
        " --declared \"msvcp140*.dll,vcruntime140*.dll,concrt140.dll,vccorlib140.dll|Microsoft Visual C++ runtime|-|Terms: see THIRD-PARTY-NOTICES.md.\"")
else()
    set(ERDFLOW_NOTICES_DIR "usr/${CMAKE_INSTALL_DATADIR}/doc/erdflow")
    set(ERDFLOW_NOTICES_APPLICATION "erdflow")
    # ICU comes with Qt for Linux. The pattern names the version whose licence
    # text is in notices/licenses, so a Qt with another ICU stops the build
    # until that text is checked.
    string(APPEND ERDFLOW_NOTICES_ARGUMENTS
        " --release-blocker \"AppImage runtime static dependencies, source/relinking obligations, AppRun and final linuxdeploy-added files still require audit.\""
        " --declared \"libicu*.so.73*|ICU 73.2, from the Qt binary package for Linux|LicenseRef-ICU-73.2|\""
        " --component \"AppImage runtime (AppImage type2-runtime 20251108)|LicenseRef-AppImage-type2-runtime|Starts the AppImage. See THIRD-PARTY-NOTICES.md for the libraries it contains.\""
        " --debian-system-libraries")
endif()
if(ERDFLOW_ALLOW_INCOMPLETE_NOTICES)
    string(APPEND ERDFLOW_NOTICES_ARGUMENTS " --allow-incomplete")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/notices/write-notices.cmake.in"
               "${CMAKE_BINARY_DIR}/packaging/write-notices.cmake" @ONLY)

if(APPLE)
    # ERDFlow.app, with its icon and Info.plist. The executable inside is named
    # ERDFlow; tests still find it through the erdflow target.
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(ERDFLOW_PLIST_MINIMUM_SYSTEM
            "<key>LSMinimumSystemVersion</key>\n  <string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string>")
    else()
        set(ERDFLOW_PLIST_MINIMUM_SYSTEM "")
    endif()
    set(erdflow_icns "${CMAKE_CURRENT_LIST_DIR}/icons/erdflow.icns")
    target_sources(erdflow PRIVATE "${erdflow_icns}")
    set_source_files_properties("${erdflow_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(erdflow PROPERTIES
        MACOSX_BUNDLE ON
        OUTPUT_NAME ERDFlow
        INSTALL_RPATH "@executable_path/../Frameworks"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME ERDFlow
        MACOSX_BUNDLE_GUI_IDENTIFIER "${ERDFLOW_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE erdflow.icns
        MACOSX_BUNDLE_COPYRIGHT "${ERDFLOW_COPYRIGHT}")

    install(TARGETS erdflow BUNDLE DESTINATION .)
    install(FILES ${erdflow_notices} DESTINATION ERDFlow.app/Contents/Resources/Licenses)
elseif(WIN32)
    # A windowed program, with its icon and version details compiled in.
    set(ERDFLOW_WINDOWS_ICON "${CMAKE_CURRENT_LIST_DIR}/icons/erdflow.ico")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/windows/erdflow.rc.in"
                   "${CMAKE_BINARY_DIR}/packaging/erdflow.rc" @ONLY)
    target_sources(erdflow PRIVATE "${CMAKE_BINARY_DIR}/packaging/erdflow.rc")
    set_target_properties(erdflow PROPERTIES WIN32_EXECUTABLE ON)

    install(TARGETS erdflow RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${erdflow_notices} DESTINATION licenses)

    # The Visual C++ runtime goes beside erdflow.exe, so a per-user install
    # needs no administrator rights. The Universal CRT is part of Windows 10
    # and later and is not copied.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)

    # The Inno Setup script, pointed at the folder `cmake --install` fills.
    set(ERDFLOW_STAGE_DIR "${CMAKE_BINARY_DIR}/stage")
    file(TO_NATIVE_PATH "${ERDFLOW_STAGE_DIR}" ERDFLOW_STAGE_DIR_NATIVE)
    file(TO_NATIVE_PATH "${ERDFLOW_PACKAGE_OUTPUT_DIR}" ERDFLOW_PACKAGE_OUTPUT_DIR_NATIVE)
    file(TO_NATIVE_PATH "${ERDFLOW_WINDOWS_ICON}" ERDFLOW_WINDOWS_ICON_NATIVE)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/windows/erdflow.iss.in"
                   "${CMAKE_BINARY_DIR}/packaging/erdflow.iss" @ONLY)
else()
    # A /usr-style tree for an AppImage's AppDir: the program, its desktop
    # entry and icon, and the notices. linuxdeploy adds the Qt libraries.
    install(TARGETS erdflow RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "${CMAKE_CURRENT_LIST_DIR}/linux/erdflow.desktop"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES "${CMAKE_CURRENT_LIST_DIR}/icons/erdflow.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
    install(FILES ${erdflow_notices} DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/erdflow)
endif()

# macOS and Windows: Qt's deployment script runs macdeployqt or windeployqt on
# the installed application, copying the shared Qt libraries and plugins it
# needs. It runs after the files above are in place. Qt's own translations are
# left out (ERDFlow is in English); the compiler runtime is handled above.
#
# On Windows, windeployqt would also add the Mesa software OpenGL renderer and
# the DirectX shader compilers. ERDFlow draws with Qt's raster engine and uses
# neither OpenGL nor Direct3D, so they are left out, together with their
# licences. It would add the TUIO touch plugin (plugin type "generic") too,
# and with it Qt Network and Network's TLS and network-information plugins;
# ERDFlow uses no network, so those are left out as well, as the macOS bundle
# leaves them out (2026-10-15).
if(WIN32)
    set(erdflow_deploy_tool_options DEPLOY_TOOL_OPTIONS
        --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler
        --skip-plugin-types generic --no-network)
else()
    set(erdflow_deploy_tool_options "")
endif()
if(APPLE OR WIN32)
    qt_generate_deploy_app_script(
        TARGET erdflow
        OUTPUT_SCRIPT erdflow_deploy_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        ${erdflow_deploy_tool_options})
    install(SCRIPT "${erdflow_deploy_script}")
endif()

# macOS: remove what macdeployqt copied but ERDFlow does not use, refuse
# GPL-only Qt components, write THIRD-PARTY-LICENSES.txt, then sign the bundle
# ad hoc (macos/finish-bundle.cmake).
if(APPLE)
    install(CODE "
        set(ERDFLOW_BUNDLE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/ERDFlow.app\")
        set(ERDFLOW_WRITE_NOTICES \"${CMAKE_BINARY_DIR}/packaging/write-notices.cmake\")
        include(\"${CMAKE_CURRENT_LIST_DIR}/macos/finish-bundle.cmake\")
    ")
elseif(WIN32)
    install(CODE "
        set(ERDFLOW_NOTICES_ROOT \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
        include(\"${CMAKE_BINARY_DIR}/packaging/write-notices.cmake\")
    ")
endif()

if(APPLE)
    set(CPACK_PACKAGE_NAME ERDFlow)
    set(CPACK_PACKAGE_VENDOR ERDFlow)
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "ERDFlow-${PROJECT_VERSION}-macos-${ERDFLOW_PACKAGE_ARCH}")
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME "ERDFlow ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT UDZO)
    set(CPACK_PACKAGE_DIRECTORY "${ERDFLOW_PACKAGE_OUTPUT_DIR}")
    include(CPack)
endif()
