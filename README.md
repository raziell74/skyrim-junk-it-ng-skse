# CommonLibSSE NG

Because this uses [alandtse/CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG), it supports Skyrim SE, AE, GOG, and VR.

Hook IDs and offsets must still be found manually for each version.

# Requirements

## Runtime (players)

SKSE plugins are version-checked **before** they load. A loader abort that says the DLL is not supported means SKSE rejected the plugin metadata, or a *different* DLL in the same dialog.

- Match **SKSE** to the game executable:
  - Steam Skyrim AE **1.7.99** needs **SKSE 2.3.0**.
  - Steam Skyrim AE **1.6.1170** needs **SKSE 2.2.6** (`skse64_2_02_06`).
- Install [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), Anniversary Edition all-in-one, so the database for your game version is present (`Data/SKSE/Plugins/versionlib-1-7-99-0.bin` or `versionlib-1-6-1170-0.bin`). Junk It declares Address Library compatibility; without that database the plugin can fail after SKSE accepts it.
- If the loader still aborts, open `Documents\My Games\Skyrim Special Edition\SKSE\skse64.log` and confirm the incompatible line names **JunkIt.dll**. If it names another plugin (Address Library, SKSE Menu Framework, Engine Fixes), that other DLL is the one SKSE is rejecting.
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) is required at runtime for the in-game settings pages in the Mod Control Panel.
- Optional: [SkyPrompt](https://www.nexusmods.com/skyrimspecialedition/mods/148703) shows on-screen Mark and Transfer/Sell key prompts in inventory, container, and barter menus. Without it, the existing hotkeys still work.
- Settings live in `Data/SKSE/Plugins/JunkIt.ini`. If that file is missing, Junk It will migrate values from `Data/MCM/Settings/JunkIt.ini` once, then write the new INI. SkyUI / MCM Helper is no longer used.

## Build (developers)

- [Visual Studio 2022](https://visualstudio.microsoft.com/) (_the free Community edition_)
- [`vcpkg`](https://github.com/microsoft/vcpkg)
  - 1. Clone the repository using git OR [download it as a .zip](https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip)
  - 2. Go into the `vcpkg` folder and double-click on `bootstrap-vcpkg.bat`
  - 3. Edit your system or user Environment Variables and add a new one:
    - Name: `VCPKG_ROOT`  
      Value: `C:\path\to\wherever\your\vcpkg\folder\is`
  - The latest version of vcpkg needs a default repository defined in the json. If you're using an older version of vcpkg, simply delete the default repository definition in `vcpkg-configuration.json`

## Opening the project

Once you have Visual Studio 2022 installed, you can open this folder in basically any C++ editor, e.g. [VS Code](https://code.visualstudio.com/) or [CLion](https://www.jetbrains.com/clion/) or [Visual Studio](https://visualstudio.microsoft.com/)
- > _for VS Code, if you are not automatically prompted to install the [C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extensions, please install those and then close VS Code and then open this project as a folder in VS Code_

You may need to click `OK` on a few windows, but the project should automatically run CMake!

It will _automatically_ download [alandtse/CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (via the local vcpkg overlay port) and everything you need to get started making your new plugin!

# Project setup

By default, when this project compiles it will output a `.dll` for your SKSE plugin into the `build/` folder.

If you want to configure this project to output your plugin files
into your Skyrim Special Edition's "`Data`" folder:

- Set the `SKYRIM_FOLDER` environment variable to the path of your Skyrim installation  
  e.g. `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`

If you want to configure this project to output your plugin files
into your "`mods`" folder:  
(_for Mod Organizer 2 or Vortex_)

- Set the `SKYRIM_MODS_FOLDER` environment variable to the path of your mods folder:  
  e.g. `C:\Users\<user>\AppData\Local\ModOrganizer\Skyrim Special Edition\mods`  
  e.g. `C:\Users\<user>\AppData\Roaming\Vortex\skyrimse\mods`
