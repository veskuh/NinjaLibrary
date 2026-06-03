# FindPoppler.cmake
# Finds the Poppler Qt6 library

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_POPPLER_QT6 QUIET poppler-qt6)
endif()

find_path(Poppler_INCLUDE_DIR
    NAMES poppler-qt6.h
    PATH_SUFFIXES poppler/qt6
    HINTS ${PC_POPPLER_QT6_INCLUDE_DIRS} /opt/homebrew/include /usr/local/include /usr/include
)

find_library(Poppler_LIBRARY
    NAMES poppler-qt6
    HINTS ${PC_POPPLER_QT6_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Poppler
    REQUIRED_VARS Poppler_LIBRARY Poppler_INCLUDE_DIR
)

if(Poppler_FOUND)
    set(Poppler_LIBRARIES ${Poppler_LIBRARY})
    set(Poppler_INCLUDE_DIRS ${Poppler_INCLUDE_DIR})

    if(NOT TARGET Poppler::Poppler)
        add_library(Poppler::Poppler UNKNOWN IMPORTED)
        set_target_properties(Poppler::Poppler PROPERTIES
            IMPORTED_LOCATION "${Poppler_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Poppler_INCLUDE_DIR}"
        )
    endif()
endif()
