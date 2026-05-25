# OnTop Windows

A simple utility that shows any window of your choice always-on-top as a live thumbnail clone. Useful for watching videos while working, monitoring background processes, or keeping reference windows visible.

Built with pure Win32 API and DWM Thumbnails — lightweight, no dependencies, single .exe.

> 🇷🇺 [Русская версия](README_RU.md)

---

## Usage

1. Launch — a small window appears
2. Click **Select Window**, pick a window from the list
3. A live clone opens, always on top

- **Resize clone**: hover it + **Alt** + Mouse Wheel (slow) / **Alt+Shift** + Mouse Wheel (fast)
- **Move clone**: drag the title bar
- **Click-through**: **Alt+Shift+O** — toggle mouse passthrough (window becomes non-interactive)
- **Settings** ⚙ — adjust resize speed

---

## Requirements

- Windows 7+

## Build

Open `ontopwindows.slnx` in Visual Studio 2022+, build **Release/x64**.

## License

[MIT](LICENSE)

