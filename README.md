# NinjaLibrary

NinjaLibrary is a local-first document gallery application designed for macOS (Universal Binaries: Apple Silicon `arm64` and Intel `x86_64`) and Linux.

---

## Features
- **iPhoto-Style Grid & iTunes-Style Table Views**: Dual layout canvas using Kaakao.
- **SQLite Metadata Schema**: Fully indexed using SQLite FTS5 for debounced, fast queries.
- **Background Ingestion & OCR**: Pipeline parsing textual PDFs via Poppler-Qt6 or falling back to Tesseract-OCR for scanned images.
- **macOS Sandbox Persistence**: Objective-C++ security-scoped URL bookmark resolution.
- **Centralized Sidecar Storage**: Centralized metadata backup in `~/.local/share/NinjaLibrary/sidecars/`.

---


## Third-Party Libraries & Attributions

This project relies on the following third-party dependencies:

1. **[Qt 6.8](https://www.qt.io/)** (Core, Gui, Qml, Quick, Sql, Test, QuickTest)
   - *License*: LGPLv3.
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

---

## Development and Build Instructions

### Dependencies
Ensure the following tools are installed (typically via Homebrew on macOS):
- `cmake`
- `qt6` (Qt 6.8+ recommended)
- `tesseract`
- `poppler`

### Compiling and Running
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Automated Tests
The project features a comprehensive test suite covering the database schema, models, workers, and QML QuickTests.
```bash
ctest --output-on-failure
```

### Automation Scripts
The project includes shell scripts inside the `scripts/` directory to simplify development and packaging:
- **[build_and_run.sh](scripts/build_and_run.sh)**: Automates debug system configuration, compilation, CTest execution, and runs the application.
  - Run fresh configuration and clean build: `./scripts/build_and_run.sh --clean`
  - Run fast incremental build and run: `./scripts/build_and_run.sh`
- **[build_mac_release.sh](scripts/build_mac_release.sh)** (macOS only): Compiles a standalone release-mode build and packages it inside a signed `.app` bundle and compressed `.dmg` disk image.
  - Generates application icons (`.icns`) from raw assets.
  - Copies and packages OCR trained language files directly inside bundle resources.
  - Resolves and copies Qt and library dependencies via `macdeployqt`.
  - Performs ad-hoc codesigning with Sandbox entitlements.

## Platform Directories & Standard Storage
The application utilizes platform-standard paths configured dynamically via `QStandardPaths`:
- **SQLite Database (`library.db`)**: Located in standard app local data storage:
  - macOS: `~/Library/Application Support/net.veskuh/NinjaLibrary/`
  - Linux: `~/.local/share/net.veskuh.NinjaLibrary/`
- **Centralized Sidecar Storage (`*.ninja`)**: Located inside a `/sidecars/` subdirectory under the App Local Data path.
- **Thumbnail Cache (`*.png`)**: Located in standard app cache storage:
  - macOS: `~/Library/Caches/NinjaLibrary/thumbnails/`
  - Linux: `~/.cache/NinjaLibrary/thumbnails/`

## Architectural Choices

### 1. Local-First Ingestion & Scanning Pipeline
* **Asynchronous Directory Traversal**: Folder scanning runs asynchronously inside background threads (`ScannerTask` executing on the global `QThreadPool`), preventing blocking of the QML UI thread.
* **Cryptographic Deduplication & Change Tracking**: Every file's SHA-256 hash is computed. Hashing, text extraction, and PDF parsing occur completely outside of database write transactions to minimize lock contention. The database uses these hashes to track file movements, renames, and contents uniquely.
* **OCR Scheduling**: For images or PDFs lacking native text layer annotations (fewer than 10 words extracted), the system queues an asynchronous `OcrTask` via `OcrTask` (which uses Tesseract OCR) to extract text content, and updates a centered progress bar in the main view's status bar.

### 2. SQLite Database & Centralized Sidecars
* **WAL Journaling Mode**: SQLite is configured with Write-Ahead Logging (`PRAGMA journal_mode = WAL;`) and synchronous mode set to `NORMAL`. This allows concurrent readers (e.g., QML UI thread reading document lists) and a single background writer to operate without lock conflicts.
* **Immediate Write Transactions**: Database writes in background worker threads use `BEGIN IMMEDIATE TRANSACTION` to prevent write lock upgrade deadlocks.
* **Model Refresh Debouncing**: Model refreshes on the UI thread are debounced via a `QTimer` inside `DocumentModel` (200ms window) to coalesce concurrent background indexer finished signals, preventing UI thread blocking and read-lock starvation.
* **Centralized `.ninja` Sidecars**: User customizations (ratings, tags, notes) are optionally backed up inside centralized JSON-formatted sidecar files stored in the standard app data directory (e.g., `~/.local/share/NinjaLibrary/sidecars/` on Linux/macOS).
  - *No Directory Pollution*: Storing sidecars centrally avoids cluttering user directories with hidden files.
  - *Self-Healing Re-indexing*: If the database is rebuilt, scanning the watched directories again reads the file hashes, locates the corresponding `<SHA256>.ninja` sidecar, and automatically restores all user-generated tags, ratings, and notes.

### 3. macOS Sandboxed Deployment & Security
* **App Sandboxing**: NinjaLibrary complies with macOS App Sandbox requirements. To access directories outside of the app container, the application utilizes Objective-C++ (`MacBookmarks.mm`) to resolve security-scoped bookmarks.
* **Security-Scoped Bookmarks**: When the user adds a watched directory, the app requests the OS for a security-scoped URL bookmark and persists this bookmark as a `BLOB` in the database. On subsequent app launches, the application resolves these bookmarks dynamically to regain authorized access to files outside its sandbox without prompting the user.
* **Standalone Packaging**: The build pipeline (`build_mac_release.sh`) compiles a release-mode build (targeting macOS 12+), copies OCR trained data files (`eng.traineddata`) directly into the bundle's `Resources` directory, uses `macdeployqt` to resolve and embed shared Qt libraries, performs ad-hoc codesigning with Sandbox entitlements, and packages the result into a `.dmg` installer.

## Codebase Structure

- **[src/](src/)**: Core application sources:
  - **[app/](src/app/)**: Main entry points and application-wide bootstrapper.
  - **[components/](src/components/)**: Small reusable QML UI elements (e.g. `DocumentCard`, `TagPill`).
  - **[controllers/](src/controllers/)**: C++ controller layer (`LibraryController`) managing workflows, folder indexing signals, and background operations.
  - **[database/](src/database/)**: C++ SQLite encapsulation layer (`DatabaseManager`) handling connections, migrations, and schema setups.
  - **[models/](src/models/)**: QML-bound data models (`DocumentModel`) and sorting/filtering proxies (`ProxyFilter`).
  - **[panels/](src/panels/)**: Left sidebar navigation and inspector panel QML layouts.
  - **[utils/](src/utils/)**: C++ static utilities for file hashing, text extraction, and PDF rendering.
  - **[views/](src/views/)**: Primary screen views (e.g., `GridCanvas`, `TableCanvas`, and the main window QML files).
  - **[workers/](src/workers/)**: Background tasks (`ScannerTask`, `OcrTask`, `ThumbnailTask`) running asynchronously on the thread pool.

- **[tests/](tests/)**: Verification suites:
  - **[unit/](tests/unit/)**: C++ unit tests for database schemas and models.
  - **[integration/](tests/integration/)**: C++ integration tests for file scanners and OCR task execution.
  - **[qml/](tests/qml/)**: QML TestCase files and C++ test runner testing UI responsiveness, status bar indicators, and dialog properties.

---

## Keyboard Shortcuts

NinjaLibrary is supports keyboard-centric:

| Shortcut | Action |
| --- | --- |
| `Cmd + O` | Add Watched Folder |
| `Cmd + Q` / `Ctrl + Q` | Quit Application |
| `Cmd + F` / `Ctrl + F` | Focus Search Box |
| `Cmd + Opt + S` / `Ctrl + Alt + S` | Toggle Sidebar Visibility |
| `Cmd + Opt + I` / `Ctrl + Alt + I` | Toggle Inspector Panel Visibility |
| `Cmd + 1` / `Ctrl + 1` | Switch to Grid View |
| `Cmd + 2` / `Ctrl + 2` | Switch to Table View |
| `Cmd + M` | Minimize Window |
| `Cmd + ,` | Open Preferences Dialog |
| `Esc` | Clear Search, Selection, and Reset Input Focus |

---

## Coding convetions

1. **Code Formatting**: Format C++ code using standard style rules (ClangFormat). Ensure QML files are clean and follow standard spacing convention.
2. **Adding Tests**: Any new features should be accompanied by comprehensive tests under the `tests/` directory (either C++ unit tests or QML QuickTests).
3. **Running Verification**: Before submitting, run the local verification suite:
   ```bash
   ./scripts/build_and_run.sh
   ```
   Ensure 100% test pass rate and that code coverage remains above the **80%** threshold.

---

## License

This project is licensed under the terms of the **BSD 3-Clause License**. See [LICENSE](LICENSE) for details.


