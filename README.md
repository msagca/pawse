<p align="center"><img src="resources/pawse.png" width="96" alt="Pawse paw icon"></p>

<h1 align="center">Pawse</h1>

<p align="center">Lock your keyboard, your mouse or both, so a cat walking across the desk can't type, click or close anything.</p>

Pawse is a small native Windows tray app. Your screen stays on and visible: videos keep playing, downloads keep running. Only the input is paused.

## Features

- **Lock the keyboard, the mouse or both**, each with its own key binding. Press it again to unlock.
- **Optional Windows Hello check** before unlocking, so a cat that hits the binding still can't unlock.
- **You can't lock yourself out.** <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Del</kbd> always works. See [Safety](docs/README.md#safety).
- **The tray icon shows what is locked**, and a short message in the middle of the screen confirms each lock and unlock and tells whoever touches a locked device how to unlock it.
- **Signed updates**, installed only when you choose.

## Install

Download `pawse-<version>-x64-setup.exe` from [Releases](https://github.com/msagca/pawse/releases) and run it. It needs 64-bit Windows 10 or 11 and no administrator rights.

## Usage

A paw appears in the notification area (tray). It turns red while something is locked.

| Default binding | Action |
| --- | --- |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>K</kbd> | Lock or unlock the keyboard |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>M</kbd> | Lock or unlock the mouse |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>L</kbd> | Lock or unlock both |

You can also lock from the tray menu, and unlock a locked keyboard there by choosing **Lock keyboard** again. Change bindings and other options in **Settings…**.

## Documentation

- [docs/README.md](docs/README.md): settings, updates, safety and limitations.
- [src/README.md](src/README.md): building from source and how Pawse works.
- [installer/README.md](installer/README.md): the installer, releases and release signing.

## Contributing

Issues and pull requests are limited to collaborators. If you'd like to help, reach out to the maintainer.

## License

[MIT](LICENSE)
