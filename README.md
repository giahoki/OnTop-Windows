# OnTop Windows

A simple utility that shows any window of your choice always-on-top as a live thumbnail clone. Useful for watching videos while working, monitoring background processes, or keeping reference windows visible.

Built with pure Win32 API and DWM Thumbnails — lightweight, no dependencies, single .exe.

> 🇷🇺 [Русская версия](README_RU.md)

---

## Features

- Live DWM thumbnail clone of any window, always on top
- Crop mode to show only a portion of the source window
- Resize clone: **Alt** + Mouse Wheel (slow) / **Alt+Shift** + Mouse Wheel (fast)
- Click-through toggle (**Alt+Shift+O**) — mouse passthrough, window becomes non-interactive
- Customizable resize speed and keyboard bindings
- Auto-update checker
- 7 interface languages: Русский, English, Español, Українська, Français, Deutsch, Polski

## Usage

1. Launch — a small window appears
2. Click **Select Window**, pick a window from the list
3. A live clone opens, always on top

- **Move clone**: drag the title bar
- **Reset clone**: right-click menu → Reset Window
- **Toggle border**: right-click menu → Borders
- **Settings** ⚙ — bindings, resize speed, language, auto-update

## Requirements

- Windows 7+

## Build

Open `ontopwindows.slnx` in Visual Studio 2022+, build **Release/x64**.

## License

[MIT](LICENSE)
