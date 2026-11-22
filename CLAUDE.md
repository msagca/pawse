# Pawse

A native Windows tray app (C++20, Win32 and C++/WinRT) that locks the keyboard, the mouse or both.

| Folder | Contents |
| --- | --- |
| `src/` | Sources. See [src/CLAUDE.md](src/CLAUDE.md). |
| `include/` | One header per source file, plus `common.hpp` for shared constants. |
| `resources/` | Icons, the Settings dialog (`pawse.rc`, ids in `resource.hpp`), the manifest and `version.hpp.in`. |
| `installer/` | Inno Setup script and the release signing key. See [installer/README.md](installer/README.md). |
| `docs/` | The user guide: settings, updates, safety and limitations. |
| `tools/` | `make_icons.py`, which generates the icons in `resources/`. |
| `.github/workflows/` | The CI build, and the signed release for `v*` tags. |

## Build and run

From a Developer PowerShell for VS, run `cmake --workflow Release` or `cmake --workflow Debug`. The app is written to `build/<config>/pawse.exe`. There are no automated tests.

To try a build, exit the installed Pawse from its tray menu first. A second copy only opens the running copy's Settings and exits. Manual test cases for input blocking are in the `test-input-blocking` skill.

## Conventions

- Headers use `.hpp` and are included with angle brackets, such as `#include <app.hpp>`.
- `.claude/hooks/format.py` runs clang-format with the repo's `.clang-format` after each edit.
- Document user-visible behavior in `docs/README.md` and internals in `src/README.md`, in the same change.
- The version is in `CMakeLists.txt` (`project(... VERSION x.y.z)`) and in the example command at the top of `installer/pawse.iss`. Bump both in a commit of its own named "Bump version to x.y.z". Releasing is described in [installer/README.md](installer/README.md#publishing-a-release).
- Commit subjects are imperative and in sentence case, with no prefix.
