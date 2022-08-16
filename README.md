# TimeLight

[![Linux build](https://github.com/ChristianLightServices/TimeLight/actions/workflows/linux-build.yml/badge.svg)](https://github.com/ChristianLightServices/TimeLight/actions/workflows/linux-build.yml) [![Windows build](https://github.com/ChristianLightServices/TimeLight/actions/workflows/windows-build.yml/badge.svg)](https://github.com/ChristianLightServices/TimeLight/actions/workflows/windows-build.yml) [![macOS build](https://github.com/ChristianLightServices/TimeLight/actions/workflows/macos-build.yml/badge.svg)](https://github.com/ChristianLightServices/TimeLight/actions/workflows/macos-build.yml)

Ever fancy a simpler way to use your favorite time tracking service? Here it is: a button that sits in your system tray and, when clicked, switches your timer on and off. It supports both Clockify and TimeCamp; furthermore, its code is designed to make it trivial to add support for other time services. As if that weren't cool enough, it can automatically set your presence in Microsoft Teams to indicate to others whether you are working or not.

## Usage

To start a time entry, simply click the button. It will change color from red to green. Clicking it again will stop the running entry, sending the button back to red.

When you first run TimeLight, you will need to select which time service to track time on. Next, you will be asked to enter your API key. This key will likely be found under your account settings. Finally, you will be asked to select the workspace to track time on. By default, TimeLight assumes that you want it to use the last project (that is, when starting a work time entry, TimeLight will look at the last entry you made and use the project from that entry). This can be changed in the "Default project" tab of the settings dialog.

Right-clicking the button will provide you with several options. The most important of these options is "Settings". The settings dialog allows you to change accounts or workspaces, modify the default project settings, and change app settings.

A few settings deserve a bit of explanation. On the "Default project" page, there is setting that is labeled "Use a separate break time project". If enabled, this setting will cause TimeLight to display two buttons. The first button will allow you to turn your time service on or off, while the second button is designed to allow you to switch from your normal project to a specific project designated as a break time project. Of course, this setting could be used for other projects as well.

The "Interval between updates of data" setting on the "App settings" tab allows you to configure how often TimeLight checks the status of your time entries on the web server. If you are using TimeCamp, it is recommended to set this to a large value, as checking too often will result in TimeLight being temporarily blocked; Clockify users should not have to worry about this, because Clockify allows apps to perform 10 checks per second.

## Teams integration

In order to use the Teams integration, go to the "Microsoft Teams" page in the settings dialog and enable the integration. You will be taken to your web browser in order to authenticate TimeLight.

In order to allow TimeLight to set your Teams status, close the Teams app or reset your Teams status. Please note that the Away status may not work correctly due to the way Microsoft handles custom statuses.

### ⚠ Warning ⚠
The Teams integration has not been fully tested and may stop working several months after authentication. Any bugs will be fixed as soon as they are discovered.

## Requirements

This project depends on the following libraries:

- [Qt](https://qt.io) 6.2 or higher
- [nlohmann/json](https://github.com/nlohmann/json) 3.10.5 or higher
- [qtkeychain](https://github.com/frankosterfeld/qtkeychain) 0.13.2 or higher
- [SingleApplication](https://github.com/itay-grudev/SingleApplication) (bundled as a submodule)
- [spdlog](https://github.com/gabime/spdlog) 1.10.0 or higher
- [backward-cpp](https://github.com/bombela/backward-cpp)

`nlohmann/json`, `qtkeychain`, `spdlog`, and `backward-cpp` will automatically be fetched from GitHub by CMake if you do not provide them in your build environment.

Additionally, building the application requires the following:

- [CMake](https://cmake.org) 3.23 or higher
- A [C++20-capable compiler](https://en.cppreference.com/w/cpp/compiler_support/20) (the latest Clang, MSVC, and GCC all work)
- [Qt Installer Framework](https://doc.qt.io/qtinstallerframework/index.html) 4.x (if you are building the installer)

## Getting the code

Since SingleApplication is bundled as a submodule, cloning the source should generally look like this:

```
$ git clone https://github.com/ChristianLightServices/TimeLight.git --recurse-submodules
```

Alternately, if you cloned without fetching submodules, just do this:

```
$ git submodule update --init --recursive
```

## Building

### Binaries

Make sure you have Qt 6.2 or newer installed and available. Then, it's a simple matter of:

```
$ mkdir build && cd build
$ cmake ..
$ cmake --build . --config Release
```

and you have a binary!

### Installer

#### ⚠ The installer has been developed for Windows only; while it will likely run on other platforms, it may not work correctly. ⚠

Rerun CMake in the build directory, like so:

```
$ cd build
$ cmake -DGENERATE_INSTALLER=ON ..
$ cmake --build . --config release --target package
```

An installer will be generated in `build`. Its name will vary slightly from platform to platform, but will look something like `TimeLight-2.0.0-win64.exe` on Windows.

## Getting nightly builds

If you want to get the absolute latest and greatest code, you can try out the CI builds (available from [GitHub Actions](https://github.com/ChristianLightServices/TimeLight/actions/)). Please take note of several things:

1. The CI builds do not ship Qt libraries. You will have to manually install Qt and make the libraries available to the binary.
2. CI builds produce binaries that are built against various versions of Qt. Make sure that you download the appropriate binary for your Qt version.
3. CI builds are not available for all platforms. Linux is not exactly the easiest operating system to distribute binaries for; therefore, you will just have to build your own app against the Qt libraries provided by your distribution.
4. While macOS builds are available, I have no idea if they will work. I don't have a Mac to test app distribution on, so while I am leaving binary uploads in place for now, there is no guarantee of correct functionality provided.

However, if you are running Windows or you can figure out a way to patch the .app files into your macOS install, the CI builds should provide a way for you to replace your current executable with one that has updated or fixed code.

## Contributing

Before committing, please run the following commands:

```
$ ./format.sh
$ ./translations.sh
```

These scripts were built with Linux in mind; contributions for a set of Windows scripts are welcome.
