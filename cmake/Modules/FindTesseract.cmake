# FindTesseract.cmake
# Finds the Tesseract OCR library

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_TESSERACT QUIET tesseract)
endif()

find_path(Tesseract_INCLUDE_DIR
    NAMES tesseract/baseapi.h
    HINTS ${PC_TESSERACT_INCLUDE_DIRS} /opt/homebrew/include /usr/local/include /usr/include
)

find_library(Tesseract_LIBRARY
    NAMES tesseract
    HINTS ${PC_TESSERACT_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tesseract
    REQUIRED_VARS Tesseract_LIBRARY Tesseract_INCLUDE_DIR
)

if(Tesseract_FOUND)
    set(Tesseract_LIBRARIES ${Tesseract_LIBRARY})
    set(Tesseract_INCLUDE_DIRS ${Tesseract_INCLUDE_DIR})

    if(NOT TARGET Tesseract::Tesseract)
        add_library(Tesseract::Tesseract UNKNOWN IMPORTED)
        set_target_properties(Tesseract::Tesseract PROPERTIES
            IMPORTED_LOCATION "${Tesseract_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Tesseract_INCLUDE_DIR}"
        )
    endif()
endif()
