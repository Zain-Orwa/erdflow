# Finishes ERDFlow.app after macdeployqt, at install time (packaging.cmake runs
# it after Qt's deployment script). Called with ERDFLOW_BUNDLE set to the
# installed bundle.
#
# macdeployqt copies every plugin in the Qt installation's plugin folders that
# might apply, and whatever those plugins need. A Qt installation with more
# modules than ERDFlow uses (Homebrew's, for one) then puts unwanted, and
# sometimes GPL-only, Qt components into the bundle. This step:
#
#   1. removes the Qt Virtual Keyboard plugin, which is GPL-only and unused;
#   2. removes any plugin whose libraries are not in the bundle, since it
#      could never load;
#   3. keeps only the frameworks and libraries that ERDFlow and its remaining
#      plugins actually load, and removes the rest;
#   4. stops the build if a GPL-only Qt module or a library from outside the
#      bundle is still referenced;
#   5. writes THIRD-PARTY-LICENSES.txt for what is left in the bundle
#      (ERDFLOW_WRITE_NOTICES, generated from notices/write-notices.cmake.in),
#      before signing, since the signature covers it; and
#   6. signs the bundle and checks the signature: with the Developer ID named
#      by the ERDFLOW_CODESIGN_IDENTITY environment variable, with the hardened
#      runtime and a secure timestamp, as notarization needs (2026-10-15); with
#      none named, ad hoc, as Apple silicon requires before it will run it.

if(NOT ERDFLOW_BUNDLE OR NOT IS_DIRECTORY "${ERDFLOW_BUNDLE}")
    message(FATAL_ERROR "finish-bundle: no bundle at '${ERDFLOW_BUNDLE}'")
endif()

set(contents "${ERDFLOW_BUNDLE}/Contents")
set(frameworks "${contents}/Frameworks")
set(plugins "${contents}/PlugIns")
file(GLOB executables LIST_DIRECTORIES false "${contents}/MacOS/*")

# Whether a path lies inside a folder; a plain prefix test, so folder names
# need no escaping.
function(erdflow_is_inside path folder out)
    string(LENGTH "${folder}/" length)
    string(SUBSTRING "${path}" 0 ${length} start)
    if(start STREQUAL "${folder}/")
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# The libraries one Mach-O file loads, as bundle paths. Library paths that are
# part of macOS (/System, /usr/lib) are left out; any other absolute path is
# returned unchanged so it can be reported. A library's own install name, which
# otool -L lists among the rest, is not something it loads and is left out too.
function(erdflow_loaded_libraries file out)
    execute_process(COMMAND otool -L "${file}" OUTPUT_VARIABLE listing RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "finish-bundle: cannot inspect dependencies of ${file}")
    endif()
    execute_process(COMMAND otool -D "${file}" OUTPUT_VARIABLE own_name_listing)
    string(REPLACE "\n" ";" own_name_lines "${own_name_listing}")
    list(LENGTH own_name_lines own_name_count)
    set(own_name "")
    if(own_name_count GREATER 1)
        list(GET own_name_lines 1 own_name)
        string(STRIP "${own_name}" own_name)
    endif()
    string(REPLACE "\n" ";" lines "${listing}")
    list(REMOVE_AT lines 0)
    set(result "")
    foreach(line IN LISTS lines)
        # Universal Qt frameworks print a header for each architecture. Only
        # dependency rows have compatibility versions; headers are not paths.
        if(NOT line MATCHES "^[ \t]+.* \\(compatibility version")
            continue()
        endif()
        string(STRIP "${line}" line)
        string(REGEX REPLACE " \\(compatibility version.*$" "" path "${line}")
        if(path STREQUAL "" OR path STREQUAL own_name
           OR path MATCHES "^/System/" OR path MATCHES "^/usr/lib/")
            continue()
        endif()
        if(path MATCHES "^@(rpath|executable_path|loader_path)/(.*)$")
            set(relative "${CMAKE_MATCH_2}")
            string(REGEX REPLACE "^(\\.\\./)*(Frameworks/)?" "" relative "${relative}")
            set(path "${frameworks}/${relative}")
        endif()
        list(APPEND result "${path}")
    endforeach()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

# 1. Qt Virtual Keyboard is GPL-only, and ERDFlow has no use for an on-screen
#    keyboard.
file(GLOB virtual_keyboard "${plugins}/platforminputcontexts/*virtualkeyboard*")
foreach(plugin IN LISTS virtual_keyboard)
    message(STATUS "finish-bundle: removing ${plugin} (Qt Virtual Keyboard is GPL-only)")
    file(REMOVE "${plugin}")
endforeach()

# 2. Plugins whose libraries were never copied into the bundle.
file(GLOB_RECURSE plugin_files LIST_DIRECTORIES false "${plugins}/*.dylib")
foreach(plugin IN LISTS plugin_files)
    erdflow_loaded_libraries("${plugin}" needs)
    foreach(need IN LISTS needs)
        erdflow_is_inside("${need}" "${frameworks}" bundled_need)
        if(bundled_need AND NOT EXISTS "${need}")
            file(RELATIVE_PATH shown "${contents}" "${plugin}")
            message(STATUS "finish-bundle: removing ${shown}, which needs a library that is not in the bundle")
            file(REMOVE "${plugin}")
            break()
        endif()
    endforeach()
endforeach()

# 3. Everything reachable from the executable and the remaining plugins.
file(GLOB_RECURSE plugin_files LIST_DIRECTORIES false "${plugins}/*.dylib")
set(pending ${executables} ${plugin_files})
set(reached "")
set(outside "")
while(pending)
    list(POP_FRONT pending file)
    if(file IN_LIST reached)
        continue()
    endif()
    list(APPEND reached "${file}")
    erdflow_loaded_libraries("${file}" needs)
    foreach(need IN LISTS needs)
        erdflow_is_inside("${need}" "${frameworks}" bundled_need)
        if(bundled_need)
            if(NOT EXISTS "${need}")
                message(FATAL_ERROR "finish-bundle: ${file} loads missing library ${need}")
            endif()
            if(EXISTS "${need}" AND NOT need IN_LIST reached)
                list(APPEND pending "${need}")
            endif()
        else()
            list(APPEND outside "${need}")
        endif()
    endforeach()
endwhile()

file(GLOB bundled LIST_DIRECTORIES true "${frameworks}/*")
foreach(entry IN LISTS bundled)
    get_filename_component(name "${entry}" NAME)
    set(used FALSE)
    foreach(file IN LISTS reached)
        erdflow_is_inside("${file}" "${frameworks}/${name}" inside_entry)
        if(file STREQUAL "${frameworks}/${name}" OR inside_entry)
            set(used TRUE)
            break()
        endif()
    endforeach()
    if(NOT used)
        message(STATUS "finish-bundle: removing Frameworks/${name}, which nothing loads")
        file(REMOVE_RECURSE "${entry}")
    endif()
endforeach()

# 4. Nothing GPL-only, and nothing loaded from outside the bundle.
if(outside)
    list(REMOVE_DUPLICATES outside)
    list(JOIN outside "\n  " outside_shown)
    message(FATAL_ERROR "finish-bundle: ERDFlow.app loads libraries from outside the bundle:\n  ${outside_shown}")
endif()
file(GLOB_RECURSE everything LIST_DIRECTORIES true "${contents}/*")
foreach(entry IN LISTS everything)
    if(entry MATCHES "(QtVirtualKeyboard|qtvirtualkeyboard|QtCharts|QtDataVisualization|QtGraphs|QtQuick3D|QtLottie)")
        message(FATAL_ERROR "finish-bundle: GPL-only Qt component in the bundle: ${entry}")
    endif()
endforeach()

# 5. The licences of everything the bundle now contains.
if(ERDFLOW_WRITE_NOTICES)
    set(ERDFLOW_NOTICES_ROOT "${ERDFLOW_BUNDLE}")
    include("${ERDFLOW_WRITE_NOTICES}")
endif()

# 6. The signature, then check it.
set(identity "$ENV{ERDFLOW_CODESIGN_IDENTITY}")
if(identity)
    execute_process(COMMAND codesign --force --deep --options runtime --timestamp --sign "${identity}"
                            "${ERDFLOW_BUNDLE}" RESULT_VARIABLE status)
    set(signed "with ${identity}")
else()
    execute_process(COMMAND codesign --force --deep --sign - "${ERDFLOW_BUNDLE}" RESULT_VARIABLE status)
    set(signed "ad hoc")
endif()
if(NOT status EQUAL 0)
    message(FATAL_ERROR "finish-bundle: signing ${signed} failed")
endif()
execute_process(COMMAND codesign --verify --deep --strict "${ERDFLOW_BUNDLE}" RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "finish-bundle: the signature of ERDFlow.app does not verify")
endif()
message(STATUS "finish-bundle: ERDFlow.app finished and signed ${signed}")
