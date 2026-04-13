Import updated SquareLine Studio GUI files into the display_node project.

## Steps

1. **Read** `filelist.txt` from the export folder to discover all generated files:
   - Export root: `/Users/trafferty/src/SquareLineProjects/crowdash_gui/export`
   - Project root: `/Users/trafferty/src/crowdash/display_node`

2. **Copy** each generated file to its destination in display_node using the mapping below. Use the Bash `cp` command.

   | Export path (relative to export root) | Destination (relative to project root) |
   |---|---|
   | `ui.c` | `src/ui.c` |
   | `ui_helpers.c` | `src/ui_helpers.c` |
   | `components/ui_comp_hook.c` | `src/ui_comp_hook.c` |
   | `screens/ui_screen*.c` | `src/ui_screen*.c` (strip `screens/` prefix) |
   | `fonts/ui_font_*.c` | `src/fonts/ui_font_*.c` (strip `fonts/` prefix) |
   | `images/ui_img_*.c` | `src/images/ui_img_*.c` (strip `images/` prefix) |
   | `ui.h` | `include/ui.h` |
   | `ui_helpers.h` | `include/ui_helpers.h` |
   | `ui_events.h` | `include/ui_events.h` |
   | `screens/ui_screen*.h` | `include/ui_screen*.h` (strip `screens/` prefix) |

   **Never copy** `CMakeLists.txt`, `filelist.txt`, or `project.info`.
   **Never touch** `src/app_ui.cpp`, `include/app_ui.h`, or `src/main.cpp`.

3. **Fix SquareLine include paths** — the exporter always generates paths that don't match PlatformIO's layout. Apply these fixes with `sed` after every import:

   a. In `include/ui.h`, strip the `screens/` prefix from screen header includes:
   ```bash
   sed -i '' 's|#include "screens/\(ui_screen[^"]*\)"|#include "\1"|g' include/ui.h
   ```

   b. In all generated `.c` files, fix `#include "../ui.h"` → `#include "ui.h"`:
   ```bash
   sed -i '' 's|#include "\.\./ui\.h"|#include "ui.h"|g' \
     src/ui.c src/ui_helpers.c src/ui_comp_hook.c \
     src/ui_screen*.c src/fonts/ui_font_*.c src/images/ui_img_*.c
   ```

4. **Detect new or removed screens**: Compare the screen headers now present in `include/` with the `#include` directives in `src/app_ui.cpp`. Report any screens that are new (not yet included in app_ui.cpp) or missing (included but no longer exported). Do not edit app_ui.cpp automatically — flag it for the user.

5. **Show a git diff summary** of what changed:
   ```
   git -C /Users/trafferty/src/crowdash diff --stat display_node/
   ```

6. **Report** which files were copied, whether any screen changes need attention in `app_ui.cpp`, and confirm the project is ready to build.
