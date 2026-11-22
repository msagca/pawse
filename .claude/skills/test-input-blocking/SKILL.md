---
name: test-input-blocking
description: Manual test plan for Pawse's keyboard and mouse blocking - stuck modifier keys, unlock bindings that stop working, and unexpected screen locks. Use when changing src/input_blocker.cpp or the lock and unlock flow in src/app.cpp, or when a user reports stuck keys, an unlock that doesn't work, or the screen locking on its own.
---

# Testing input blocking

These cases need a real keyboard and a person at the desk. Walk the user through them one at a time, ask for the result of each, and summarize which cases failed at the end.

## What can go wrong

The keyboard hook in `src/input_blocker.cpp` tracks which keys are down (`key_is_down`) to match bindings, and which held keys may still be released (`pass_key_release`). Two kinds of bug follow from that tracking:

- **Stuck key:** Windows saw a key go down but Pawse blocked its release. Every later click or key press acts as if that key were held.
- **Stale tracking:** Pawse missed a key's release, so it thinks a modifier is still held. Bindings then match with an extra modifier, and the unlock binding does nothing.

The hook never sees input on the secure desktop: the lock screen, Ctrl+Alt+Del and UAC prompts. Releases made there never reach it.

## Setup

1. Build with `cmake --workflow Debug`, exit the installed Pawse from the tray, and run `build/Debug/pawse.exe`.
2. Open the On-Screen Keyboard (`osk.exe`). It highlights the keys Windows thinks are held, so it shows stuck keys.
3. Note the bindings in Settings. The defaults are Ctrl+Alt+K (keyboard), Ctrl+Alt+M (mouse) and Ctrl+Alt+L (both).
4. Set auto-unlock to a few minutes, so a failed unlock can't lock the user out for long.

For each case, record: what was locked, how it was locked, the foreground app, whether it ran elevated, and whether PowerToys was running.

## Stuck keys

Expected in every case: once the key is released, the On-Screen Keyboard shows nothing held, and clicks and scrolls behave normally.

1. **Held modifier, locked from the tray.** Hold one key, lock the keyboard from the tray menu with the mouse, then release the key. Repeat for left and right Ctrl, Alt, Shift and Win, and for AltGr if the layout has it.
2. **Locked with the binding, slow release.** Press the lock binding and keep holding Ctrl+Alt for about a second before letting go.
3. **Locked with the binding, fast release.** Tap the lock binding as quickly as possible.
4. **Extra modifier.** Hold Shift, press the lock binding, then release everything.
5. **Elevated foreground app.** Repeat cases 1 and 2 with an app running as administrator in front, such as Task Manager or an admin terminal. Also check whether keys are blocked at all while that app is in front.
6. **PowerToys.** Repeat cases 1 and 2 with PowerToys running, then again after exiting it.
7. **Fast relock.** Unlock with the binding and, still holding Ctrl+Alt, press K again.

## Unlock that stops working

Expected in every case: the unlock binding works on the first try.

1. **Win+L while locked.** Lock the keyboard, press Win+L, and sign back in. Note whether the PC locked at all, then press the unlock binding.
2. **Ctrl+Alt+Del while locked.** Lock the keyboard, press Ctrl+Alt+Del, choose Cancel, then press the unlock binding.
3. **UAC prompt while locked.** Lock the keyboard, start an app as administrator with the mouse, cancel the prompt, then press the unlock binding.
4. **Lock key still held.** Lock with the binding, keep holding K for a second, release it, then press the unlock binding once.

If the unlock binding fails, press and release Win, Ctrl, Alt and Shift one at a time and try again. If it works after that, the hook kept a modifier as held after its release (stale tracking).

## Unexpected screen locks

When the screen locks twice in a row, or locks without Win+L:

1. Right after signing in, check the On-Screen Keyboard for a held Win key.
2. Note whether Pawse had the keyboard locked before the first screen lock.
3. Check whether Dynamic Lock is on (Settings > Accounts > Sign-in options).
4. Try again with Pawse exited and with PowerToys exited, to see which one is involved.

Pawse has no hooks while nothing is locked, so it can only be involved if a device was locked at the time.
