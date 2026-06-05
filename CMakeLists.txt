cmake_minimum_required(VERSION 3.16)

# Standard source file copyright headers are required by the specification.
# Copyright (c) 2026 NinjaLibrary. All rights reserved.
# Licensed under the BSD-3-Clause License.

project(NinjaLibrary VERSION 26.6.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Define custom CMake module path for Poppler and Tesseract Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

# Required Qt 6.8 components (Core5Compat and ShaderTools are required by Kaakao)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Sql Core5Compat Test QuickTest ShaderTools)

# Find third party libraries
find_package(Tesseract REQUIRED)

if(APPLE)
    # macOS build uses native PDFKit and AppKit frameworks for PDF thumbnail/text handling
    message(STATUS "macOS build: Using native PDFKit and AppKit frameworks. No Poppler dependency needed.")
    set(HAVE_PDFKIT 1)
else()
    # Linux build uses standard Poppler-Qt6
    message(STATUS "Linux build: Using Poppler-Qt6 package.")
    find_package(Poppler REQUIRED)
    set(HAVE_POPPLER 1)
endif()

# Check for testing flag
option(ENABLE_TESTING "Build testing targets" ON)
if(ENABLE_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS QuickTest)
endif()

# Check for coverage flag
option(ENABLE_COVERAGE "Enable code coverage profiling" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        message(STATUS "Activating code coverage profiling flags (-fprofile-arcs -ftest-coverage)")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage flags not configured for compiler ID: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Build the Kaakao component library submodule (source subdirectory only)
add_subdirectory(3rdparty/Kaakao/src 3rdparty/Kaakao/src)

# Set deployment targets for macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    option(BUILD_UNIVERSAL "Build macOS Universal Binary" OFF)
    if(BUILD_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    else()
        message(STATUS "Building for native host architecture (BUILD_UNIVERSAL is OFF).")
    endif()
endif()

# Define the NinjaLibrary application target
if(APPLE)
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/NinjaLibrary.icns")
    if(EXISTS "${ICON_FILE}")
        qt_add_executable(NinjaLibraryApp MACOSX_BUNDLE
            src/app/NinjaLibraryApp.cpp
            ${ICON_FILE}
        )
        set_target_properties(NinjaLibraryApp PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
            MACOSX_BUNDLE_ICON_FILE "NinjaLibrary.icns"
            OUTPUT_NAME "NinjaLibrary"
        )
        set_source_files_properties(${ICON_FILE} PROPERTIES
            MACOSX_BUNDLE_RESOURCE TRUE
        )
    else()
        qt_add_executable(NinjaLibraryApp MACOSX_BUNDLE
            src/app/NinjaLibraryApp.cpp
        )
        set_target_properties(NinjaLibraryApp PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
            OUTPUT_NAME "NinjaLibrary"
        )
    endif()
else()
    qt_add_executable(NinjaLibraryApp
        src/app/NinjaLibraryApp.cpp
    )
    set_target_properties(NinjaLibraryApp PROPERTIES
        OUTPUT_NAME "NinjaLibrary"
    )
endif()

# Set up the QML module for our application logic
qt_add_qml_module(NinjaLibrary
    URI NinjaLibrary
    VERSION 26.6.0
    RESOURCE_PREFIX "/qt/qml"
    SOURCES
        src/controllers/LibraryController.h
        src/controllers/LibraryController.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/models/DocumentModel.h
        src/models/DocumentModel.cpp
        src/models/ProxyFilter.h
        src/models/ProxyFilter.cpp
        src/workers/ScannerTask.h
        src/workers/ScannerTask.cpp
        src/workers/ThumbnailTask.h
        src/workers/ThumbnailTask.cpp
        src/workers/OcrTask.h
        src/workers/OcrTask.cpp
        src/utils/HashUtils.h
        src/utils/HashUtils.cpp
        src/utils/PdfUtils.h
        src/utils/DocUtils.h
    QML_FILES
        src/views/MainWindow.qml
        src/views/AppMenuBar.qml
        src/views/ItemContextMenu.qml
        src/views/DragOverlay.qml
        src/components/DocumentCard.qml
        src/components/TagPill.qml
        src/components/LocalTableRowDelegate.qml
        src/views/GridCanvas.qml
        src/views/TableCanvas.qml
        src/panels/Sidebar.qml
        src/panels/Inspector.qml
        src/panels/AboutDialog.qml
        src/panels/PreferencesDialog.qml
)

# Add platform specific source files
if(APPLE)
    target_sources(NinjaLibrary PRIVATE
        src/utils/MacBookmarks.h
        src/utils/MacBookmarks.mm
        src/utils/MacPdfUtils.mm
        src/utils/MacDocUtils.mm
    )
    target_link_libraries(NinjaLibrary PRIVATE "-framework Foundation")
else()
    target_sources(NinjaLibrary PRIVATE
        src/utils/LinuxPdfUtils.cpp
        src/utils/LinuxDocUtils.cpp
    )
endif()

# Link core dependencies to targets
target_link_libraries(NinjaLibrary PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Sql
    Qt6::Core5Compat
    Tesseract::Tesseract
    Kaakao
)

target_link_libraries(NinjaLibraryApp PRIVATE
    NinjaLibrary
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Sql
    Qt6::Core5Compat
    Tesseract::Tesseract
    Kaakao
)

if(HAVE_POPPLER)
    target_link_libraries(NinjaLibrary PRIVATE Poppler::Poppler)
    target_link_libraries(NinjaLibraryApp PRIVATE Poppler::Poppler)
    target_compile_definitions(NinjaLibrary PRIVATE HAVE_POPPLER)
    target_compile_definitions(NinjaLibraryApp PRIVATE HAVE_POPPLER)
endif()

if(HAVE_PDFKIT)
    target_link_libraries(NinjaLibrary PRIVATE "-framework PDFKit" "-framework AppKit")
    target_link_libraries(NinjaLibraryApp PRIVATE "-framework PDFKit" "-framework AppKit")
    target_compile_definitions(NinjaLibrary PRIVATE HAVE_PDFKIT)
    target_compile_definitions(NinjaLibraryApp PRIVATE HAVE_PDFKIT)
endif()

# Add standard compile options
target_compile_definitions(NinjaLibraryApp PRIVATE
    VERSION_STRING="26.6.0"
)

# Add tests directory
if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()
