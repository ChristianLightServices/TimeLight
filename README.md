# ClockifyTrayIcons

[![Linux build](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/linux-build.yml/badge.svg)](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/linux-build.yml) [![Windows build](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/windows-build.yml/badge.svg)](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/windows-build.yml) [![macOS build](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/macos-build.yml/badge.svg)](https://github.com/ChristianLightServices/ClockifyTrayIcons/actions/workflows/macos-build.yml)

Ever fancy a simpler way to use Clockify? Here it is: two buttons that sit in your system tray that, when clicked, switch you between Clockify on, Clockify off, and break time.

## Usage

The icon with the power symbol on it will, when clicked, switch Clockify on and off. The other icon will switch Clockify from work time to break time and back.

When you first run ClockifyTrayIcons, it will ask you to enter your API key. This is found under your [account settings](https://clockify.me/user/settings). Then you will be asked to select the workspace to track time on; finally, you will need to select which project you want to use for break time. By default, ClockifyTrayIcons assumes that you want it to use the last non-break time project as the work time project (that is, when starting a work time entry, ClockifyTrayIcons will look at the last entry you made that doesn't have break time as its project and use the project from that entry). This can be changed by right-clicking on either of the icons and selecting "Change default project."

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

## Contributing

Before committing, please run `clang-format -i *.cpp *.h`.
