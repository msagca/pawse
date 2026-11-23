# Using Pawse

## Locking and unlocking

Each key binding locks its device and, pressed again, unlocks it. You can also lock from the tray menu. While only the keyboard is locked, you can unlock it there too: click the paw and choose **Lock keyboard** again. A locked mouse unlocks only with a key binding, so a paw on the keyboard can't undo it.

Hover over the tray icon to see what is locked. A message in the middle of the screen confirms each lock and unlock. When someone presses a locked key or uses a locked mouse, it appears again to say what is locked and how to unlock it. It ignores clicks, never takes focus and goes away a few seconds after the last attempt.

## Settings

Open **Settings** from the tray menu, or start Pawse again while it's running. The window can be resized and maximized, but not made smaller than its default size. Until Pawse exits, it opens at the size it had when you last closed it.

- **Key bindings.** Click a box and press the new keys. <kbd>Esc</kbd> cancels and <kbd>Backspace</kbd> clears the binding. Every binding needs <kbd>Ctrl</kbd>, <kbd>Alt</kbd> or <kbd>Win</kbd>, so it's hard to press by accident. The exceptions are keys that are rarely typed: <kbd>F1</kbd> to <kbd>F24</kbd>, <kbd>Pause</kbd> and <kbd>Scroll Lock</kbd>. A binding can be left empty, and its device is then locked from the tray menu. Because the menu can't unlock a locked mouse, the mouse can only be locked while the mouse binding or the binding for both is set.
- **Also require Windows Hello.** After the binding, Windows asks for your PIN, fingerprint or face. This needs one of them set up in *Windows Settings > Accounts > Sign-in options*. Without Windows Hello, the key binding alone unlocks. Pawse never sees your PIN: Windows shows the prompt and only tells Pawse whether it was you.
- **Unlock automatically after N minutes.** This is a safety net, off by default.
- **Show a message on lock, on unlock and when a locked device is used**, and how many seconds it stays (3 by default).
- **Start Pawse when I sign in.**
- **Check for updates.** On by default.

Settings are stored in `%APPDATA%\Pawse\settings.ini`.

## Updates

Pawse asks GitHub for the latest release 15 seconds after it starts and then once a day. This is the only time it uses the network, and it sends nothing but its version number.

When there's a new version, a notification appears and the tray menu gets an **Update to vx.y.z** item. Pawse never installs anything until you click the notification or choose that item. It then downloads the installer, checks its signature, installs it silently and starts again. If the signature doesn't match, nothing is installed. You can't update while something is locked.

## Safety

Pawse is built so that a locked computer can always be unlocked:

- **<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Del</kbd> always works.** No app can block it. From there, open Task Manager and end `pawse.exe`; all locks go away with it.
- **The Windows Hello prompt times out.** While it's open, the keyboard is let through so you can type your PIN. After 60 seconds without an answer, Pawse locks again.
- **If Windows Hello is turned off or removed**, the key binding alone unlocks.
- **If Pawse stops responding** while something is locked, a separate watchdog thread removes the locks after a few seconds. Windows also removes input hooks that stop responding.
- **Optional automatic unlock** after a set number of minutes.

## Limitations

- **Apps running as administrator:** Windows doesn't let a normal app block input meant for an elevated window, such as Task Manager. Run Pawse as administrator if you need that.
- **Touchscreen and pen:** input from these may still get through, depending on the device and the app.
- **Shortcuts Windows reserves:** <kbd>Win</kbd>+<kbd>L</kbd> and <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Del</kbd> can't be blocked. Both only lock the session or open the security screen, which is harmless.
