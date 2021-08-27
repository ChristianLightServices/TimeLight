# ClockifyTrayIcons

[![Linux build](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/linux-build.yml/badge.svg)](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/linux-build.yml) [![Windows build](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/windows-build.yml/badge.svg)](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/windows-build.yml) [![macOS build](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/macos-build.yml/badge.svg)](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/macos-build.yml)

Ever fancy a simpler way to use Clockify? Here it is: two buttons that sit in your system tray that, when clicked, switch you between Clockify on, Clockify off, and break time.

## Building

Make sure you have Qt installed and available. Then, it's a simple matter of:

```
mkdir build && cd build
cmake ..
cmake --build .
```

and you have a binary!

## Getting nightly builds

If you want to get the absolute latest and greatest code, you can try out the CI builds (available from [GitHub Actions](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/)). Please take note of several things:

1. CI builds produce binaries that are built against both Qt 5 and Qt 6. Make sure that you download the appropriate binary for your Qt version.
2. CI builds are not available for all platforms. Linux is not exactly the easiest operating system to distribute binaries for; therefore, you will just have to build your own app against the Qt libraries provided by your distribution.
3. While macOS builds are available, I have no idea if they will work. I don't have a Mac to test app distribution on, so while I am leaving binary uploads in place for now, there is no guarantee of correct functionality provided.

However, if you are running Windows or you can figure out a way to patch the .app files into your macOS install, the CI builds should provide a way for you to replace your current executable with one that has updated or fixed code.
