cmake_minimum_required(VERSION 3.16)

# Test suite configuration for NinjaLibrary
# Copyright (c) 2026 NinjaLibrary. All rights reserved.
# Licensed under the BSD-3-Clause License.

find_package(Qt6 REQUIRED COMPONENTS Core Sql Test QuickTest Concurrent)

# Create Database Unit Test Target
qt_add_executable(test_database
    unit/test_database.cpp
    ../src/database/DatabaseManager.h
    ../src/database/DatabaseManager.cpp
)

target_link_libraries(test_database PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

# Set compile options
target_compile_definitions(test_database PRIVATE
    DATABASE_TEST
)

# Register with CTest
add_test(NAME test_database COMMAND test_database)

# Create Models Unit Test Target
qt_add_executable(test_models
    unit/test_models.cpp
    ../src/database/DatabaseManager.h
    ../src/database/DatabaseManager.cpp
    ../src/models/DocumentModel.h
    ../src/models/DocumentModel.cpp
    ../src/models/ProxyFilter.h
    ../src/models/ProxyFilter.cpp
)

target_link_libraries(test_models PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Test
    Qt6::Concurrent
)

add_test(NAME test_models COMMAND test_models)

# Create Workers Integration Test Target
if(APPLE)
    qt_add_executable(test_workers
        integration/test_workers.cpp
        ../src/database/DatabaseManager.h
        ../src/database/DatabaseManager.cpp
        ../src/controllers/LibraryController.h
        ../src/controllers/LibraryController.cpp
        ../src/workers/ScannerTask.h
        ../src/workers/ScannerTask.cpp
        ../src/workers/OcrTask.h
        ../src/workers/OcrTask.cpp
        ../src/workers/ThumbnailTask.h
        ../src/workers/ThumbnailTask.cpp
        ../src/utils/HashUtils.h
        ../src/utils/HashUtils.cpp
        ../src/utils/PdfUtils.h
        ../src/utils/DocUtils.h
        ../src/utils/OcrUtils.h
        ../src/utils/MacPdfUtils.mm
        ../src/utils/MacDocUtils.mm
        ../src/utils/MacOcrUtils.mm
        ../src/utils/MacBookmarks.h
        ../src/utils/MacBookmarks.mm
    )
    target_link_libraries(test_workers PRIVATE "-framework PDFKit" "-framework AppKit" "-framework Foundation" "-framework Vision")
else()
    qt_add_executable(test_workers
        integration/test_workers.cpp
        ../src/database/DatabaseManager.h
        ../src/database/DatabaseManager.cpp
        ../src/controllers/LibraryController.h
        ../src/controllers/LibraryController.cpp
        ../src/workers/ScannerTask.h
        ../src/workers/ScannerTask.cpp
        ../src/workers/OcrTask.h
        ../src/workers/OcrTask.cpp
        ../src/workers/ThumbnailTask.h
        ../src/workers/ThumbnailTask.cpp
        ../src/utils/HashUtils.h
        ../src/utils/HashUtils.cpp
        ../src/utils/PdfUtils.h
        ../src/utils/DocUtils.h
        ../src/utils/OcrUtils.h
        ../src/utils/LinuxPdfUtils.cpp
        ../src/utils/LinuxDocUtils.cpp
        ../src/utils/LinuxOcrUtils.cpp
    )
    target_link_libraries(test_workers PRIVATE Poppler::Poppler Tesseract::Tesseract)
endif()

target_link_libraries(test_workers PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Test
)

add_test(NAME test_workers COMMAND test_workers)

# Create QML Quick Test Target
qt_add_executable(test_qml_ui
    qml/TestRunner.cpp
)

target_link_libraries(test_qml_ui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest
)

if(APPLE)
    target_link_libraries(test_qml_ui PRIVATE
        NinjaLibrary
        Kaakao
    )
else()
    target_link_libraries(test_qml_ui PRIVATE
        -Wl,--no-as-needed
        NinjaLibrary
        Kaakao
        -Wl,--as-needed
    )
endif()

target_compile_definitions(test_qml_ui PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

add_test(NAME test_qml_ui COMMAND test_qml_ui)

# Create Utils Unit Test Target
qt_add_executable(test_utils
    unit/test_utils.cpp
    ../src/utils/DocUtils.h
)

target_link_libraries(test_utils PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_utils COMMAND test_utils)

