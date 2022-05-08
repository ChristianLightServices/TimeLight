# TimeLight

[![Linux build](https://github.com/ChristianLightServices/TimeLight/actions/workflows/linux-build.yml/badge.svg)](https://github.com/ChristianLightServices/TimeLight/actions/workflows/linux-build.yml) [![Windows build](https://github.com/ChristianLightServices/TimeLight/actions/workflows/windows-build.yml/badge.svg)](https://github.com/ChristianLightServices/TimeLight/actions/workflows/windows-build.yml) [![macOS build](https://github.com/ChristianLightServices/TimeLight/actions/workflows/macos-build.yml/badge.svg)](https://github.com/ChristianLightServices/TimeLight/actions/workflows/macos-build.yml)

Ever fancy a simpler way to use your favorite time tracking service? Here it is: a button that sits in your system tray and, when clicked, switches your timer on and off. It supports both Clockify and TimeCamp; furthermore, its code is designed to make it trivial to add support for other time services.

## Usage

To start a time entry, simply click the button. It will change color from red to green. Clicking it again will stop the running entry, sending the button back to red.

When you first run TimeLight, you will need to select which time service to track time on. Next, you will be asked to enter your API key. This key will likely be found under your account settings. Finally, you will be asked to select the workspace to track time on. By default, TimeLight assumes that you want it to use the last project (that is, when starting a work time entry, TimeLight will look at the last entry you made and use the project from that entry). This can be changed in the "Default project" tab of the settings dialog.

Right-clicking the button will provide you with several options. The most important of these options is "Settings". The settings dialog allows you to change accounts or workspaces, modify the default project settings, and change app settings.

A few settings deserve a bit of explanation. On the "Default project" page, there is setting that is labeled "Use a separate break time project". If enabled, this setting will cause TimeLight to display two buttons. The first button will allow you to turn your time service on or off, while the second button is designed to allow you to switch from your normal project to a specific project designated as a break time project. Of course, this setting could be used for other projects as well.

The "Interval between updates of data" setting on the "App settings" tab allows you to configure how often TimeLight checks the status of your time entries on the web server. If you are using TimeCamp, it is recommended to set this to a large value, as checking too often will result in TimeLight being temporarily blocked; Clockify users should not have to worry about this, because Clockify allows apps to perform 10 checks per second.

## Building

Make sure you have Qt 6.2 or newer installed and available. Then, it's a simple matter of:

```
mkdir build && cd build
cmake ..
cmake --build .
```

and you have a binary!

## Getting nightly builds

### ⚠ Warning: macOS builds are temporarily disabled. ⚠

If you want to get the absolute latest and greatest code, you can try out the CI builds (available from [GitHub Actions](https://github.com/ChristianLightServices/TimeLight/actions/)). Please take note of several things:

1. The CI builds do not ship Qt libraries. You will have to manually install Qt and make the libraries available to the binary.
2. CI builds produce binaries that are built against various versions of Qt. Make sure that you download the appropriate binary for your Qt version.
3. CI builds are not available for all platforms. Linux is not exactly the easiest operating system to distribute binaries for; therefore, you will just have to build your own app against the Qt libraries provided by your distribution.
4. While macOS builds are available, I have no idea if they will work. I don't have a Mac to test app distribution on, so while I am leaving binary uploads in place for now, there is no guarantee of correct functionality provided.

However, if you are running Windows or you can figure out a way to patch the .app files into your macOS install, the CI builds should provide a way for you to replace your current executable with one that has updated or fixed code.

## Contributing

Before committing, please run `clang-format -i *.cpp *.h`.
