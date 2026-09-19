include_guard(GLOBAL)

function(pae_lab_require_toolchain)
  if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "^Visual Studio " OR
     NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Standalone Lab requires Windows x64 MSVC with a Visual Studio generator")
  endif()
  if(NOT CMAKE_VS_PLATFORM_TOOLSET STREQUAL "v142" OR
     NOT CMAKE_GENERATOR_TOOLSET MATCHES "(^|,)version=14\\.29\\.30133($|,)")
    message(FATAL_ERROR "Standalone Lab requires -T v142,version=14.29.30133")
  endif()
endfunction()

function(pae_lab_import_qt)
  set(qconfig "${PAE_QT_ROOT}/include/QtCore/qconfig.h")
  if(NOT EXISTS "${qconfig}")
    message(FATAL_ERROR "PAE_QT_ROOT lacks include/QtCore/qconfig.h")
  endif()
  file(STRINGS "${qconfig}" version_line REGEX "^#define QT_VERSION_STR ")
  if(NOT version_line MATCHES "\"5\\.13\\.[0-9]+\"")
    message(FATAL_ERROR "PAE_QT_ROOT must be Qt 5.13.x; got ${version_line}")
  endif()
  foreach(module IN ITEMS Core Gui Widgets)
    add_library("PAE_Qt5${module}" SHARED IMPORTED GLOBAL)
    add_library("PAE::Qt5${module}" ALIAS "PAE_Qt5${module}")
    set_target_properties("PAE_Qt5${module}" PROPERTIES
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
      IMPORTED_IMPLIB_DEBUG "${PAE_QT_ROOT}/lib/debug/Qt5${module}d.lib"
      IMPORTED_LOCATION_DEBUG "${PAE_QT_ROOT}/bin/debug/Qt5${module}d.dll"
      IMPORTED_IMPLIB_RELEASE "${PAE_QT_ROOT}/lib/release/Qt5${module}.lib"
      IMPORTED_LOCATION_RELEASE "${PAE_QT_ROOT}/bin/release/Qt5${module}.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${PAE_QT_ROOT}/include;${PAE_QT_ROOT}/include/Qt${module}")
    foreach(file IN ITEMS
      "${PAE_QT_ROOT}/lib/debug/Qt5${module}d.lib" "${PAE_QT_ROOT}/bin/debug/Qt5${module}d.dll"
      "${PAE_QT_ROOT}/lib/release/Qt5${module}.lib" "${PAE_QT_ROOT}/bin/release/Qt5${module}.dll")
      if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Missing fixed Qt input: ${file}")
      endif()
    endforeach()
  endforeach()
  set_property(TARGET PAE_Qt5Core APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QT_CORE_LIB)
  set_property(TARGET PAE_Qt5Gui APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QT_GUI_LIB)
  set_property(TARGET PAE_Qt5Gui APPEND PROPERTY INTERFACE_LINK_LIBRARIES PAE::Qt5Core)
  set_property(TARGET PAE_Qt5Widgets APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QT_WIDGETS_LIB)
  set_property(TARGET PAE_Qt5Widgets APPEND PROPERTY INTERFACE_LINK_LIBRARIES PAE::Qt5Gui PAE::Qt5Core)
  foreach(file IN ITEMS "${PAE_QT_ROOT}/bin/debug/platforms/qwindowsd.dll" "${PAE_QT_ROOT}/bin/release/platforms/qwindows.dll")
    if(NOT EXISTS "${file}")
      message(FATAL_ERROR "Missing fixed Qt platform plugin: ${file}")
    endif()
  endforeach()
endfunction()
