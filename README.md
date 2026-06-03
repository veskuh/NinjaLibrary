# NinjaLibrary (v26.6.0)

NinjaLibrary is a premium, local-first document gallery application designed for macOS 12+ (Universal Binaries: Apple Silicon `arm64` and Intel `x86_64`) and Linux. 

## Features
- **iPhoto-Style Grid & iTunes-Style Table Views**: Dual layout canvas using Kaakao.
- **SQLite Metadata Schema**: Fully indexed using SQLite FTS5 for debounced, instant search queries.
- **Background Ingestion & OCR**: High-performance pipeline parsing textual PDFs via Poppler-Qt6 or falling back to Tesseract-OCR for scanned images.
- **macOS Sandbox Persistence**: Objective-C++ security-scoped URL bookmark resolution.
- **Centralized Sidecar Storage**: Centralized metadata backup in `~/.local/share/NinjaLibrary/sidecars/` without polluting user document folders.

## Third-Party Libraries & Attributions

This project relies on the following third-party dependencies:

1. **[Qt 6.8](https://www.qt.io/)** (Core, Gui, Qml, Quick, Sql, Test, QuickTest)
   - *License*: LGPLv3 or Commercial.
   - *Role*: Core application runtime, graphics renderer, and database encapsulation layer.
2. **[SQLite 3](https://sqlite.org/)**
   - *License*: Public Domain.
   - *Role*: Embedded database engine storing watched directories, tags, lists, and documents with FTS5 search capabilities.
3. **[Poppler-Qt6](https://poppler.freedesktop.org/)**
   - *License*: GPLv2 / GPLv3.
   - *Role*: PDF rendering and text parsing for background workers.
4. **[Tesseract OCR](https://github.com/tesseract-ocr/tesseract)**
   - *License*: Apache License 2.0.
   - *Role*: Scanned document fallback character recognition.
5. **[Kaakao Component Library](https://github.com/veskuh/Kaakao)**
   - *License*: BSD-3-Clause.
   - *Role*: Classical Yosemite-Catalina era macOS design component layouts.
6. **[Catch2](https://github.com/catchorg/Catch2)** (Dev only)
   - *License*: Boost Software License 1.0.
   - *Role*: Development testing verification.
