# Setup Screen — SquareLine Studio Naming Guide

## Convention

`ui_<type><Context><Detail>` — all lowercase camelCase after the prefix.

| Prefix | Widget type |
|---|---|
| `ui_lbl` | Label |
| `ui_txt` | Textarea |
| `ui_btn` | Button |
| `ui_swt` | Switch |
| `ui_kb` | Keyboard |
| `ui_pnl` | Panel / container |
| `ui_screen` | Screen |

---

## Screen Object

| Component | Name |
|---|---|
| Screen | `ui_screensetup` |

---

## Schedule Section

These names **must match exactly** — they are already wired in `app_ui.cpp`.

| Component | Name | Type |
|---|---|---|
| Section label | `ui_lblScheduleTitle` | Label |
| Enable label | `ui_lblScheduleEnable` | Label |
| Enable toggle | `ui_swtScheduleEnable` | Switch |
| Sleep time panel | `ui_pnlSleepTime` | Panel |
| Sleep label | `ui_lblSleepLabel` | Label |
| Sleep time input | `ui_txtSleepTime` | Textarea |
| Sleep AM/PM label | `ui_lblSleepAmPm` | Label |
| Sleep AM/PM toggle | `ui_swtSleepAmPm` | Switch |
| Wake time panel | `ui_pnlWakeTime` | Panel |
| Wake label | `ui_lblWakeLabel` | Label |
| Wake time input | `ui_txtWakeTime` | Textarea |
| Wake AM/PM label | `ui_lblWakeAmPm` | Label |
| Wake AM/PM toggle | `ui_swtWakeAmPm` | Switch |
| Save button | `ui_btnSaveSchedule` | Button |
| Save label (inside btn) | `ui_lblSave` | Label |
| Shared number keypad | `ui_kbSchedule` | Keyboard |

---

## Navigation

| Component | Name | Notes |
|---|---|---|
| Button on setup screen to go back | `ui_btnSetupBack` | Navigates to previous screen |
| Button on other screens pointing here | `ui_btnGoToSetupScreen` | Add to whichever screen links to setup |

---

## Future Setup Items (when needed)

Follow the same pattern:

| Purpose | Name | Type |
|---|---|---|
| Zip code label | `ui_lblZipCode` | Label |
| Zip code input | `ui_txtZipCode` | Textarea |
| WiFi SSID label | `ui_lblWifiSSID` | Label |
| WiFi SSID input | `ui_txtWifiSSID` | Textarea |
| WiFi password label | `ui_lblWifiPassword` | Label |
| WiFi password input | `ui_txtWifiPassword` | Textarea |
| Schedule section panel | `ui_pnlSetupSchedule` | Panel |
| Network section panel | `ui_pnlSetupNetwork` | Panel |

---

## After Import

Run `/import-gui` and the following will be wired up automatically or flagged for manual wiring:

1. `ui_screensetup_screen_init()` added to `ui_init()` and `ui_destroy()` in `src/ui.c`
2. Gesture navigation entries added in `app_ui.cpp` (swipe left/right to reach setup screen)
3. Schedule callbacks already reference the exact widget names above — no changes needed if names match
