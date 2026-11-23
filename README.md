# Aurora Studio

Aurora Studio is a Qt Quick desktop theme manager for browsing and editing color presets. Its gallery presents the bundled themes as a cinematic cover-flow carousel, with portrait landscape previews and a translucent navy interface.

![Aurora Studio theme gallery](screenshots/aurora-studio-gallery.png)

## Features

- Browse eight bundled presets with a keyboard- and mouse-driven cover-flow gallery.
- Preview theme palettes, descriptions, and generated portrait artwork.
- Create, duplicate, edit, and delete user presets. Bundled presets remain immutable; editing one creates a user copy.
- Edit theme names, descriptions, and palette colors in the live editor.
- Save user presets as JSON in the platform's application data directory.
- Import and export validated Aurora theme JSON files.
- Run the theme repository and bundled-theme validation tests with CTest.

## Build requirements

- CMake 3.21 or newer.
- A C++20 compiler.
- Qt 6 development packages for Quick, Quick Controls 2, Widgets, and Test.
- Python 3 for the bundled theme validation test.
- Ninja is recommended, though another CMake generator can be used.

On Debian or Ubuntu, the core packages are available with:

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev
```

Install the Qt Quick QML runtime modules required by your distribution if they are not installed with the Qt development packages.

## Build, test, and launch

From the project directory:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/aurora-studio
```

The application can also be installed with CMake:

```sh
cmake --install build --prefix "$HOME/.local"
```

The current build embeds the QML interface but resolves bundled theme JSON files from the source checkout used at build time. Keep that checkout in place when using the installed launcher; relocatable theme-data installation is not implemented yet.

## Theme data and artwork

Bundled preset definitions are in [`themes/`](themes/). The JSON schema contains palette values, terminal colors, descriptions, and integration metadata. User-created themes are stored under Qt's `AppDataLocation/themes` directory.

Portrait JPEG previews are in [`assets/`](assets/), alongside the original SVG wallpaper artwork referenced by the bundled presets. The preview-art provenance is documented in [`assets/README.md`](assets/README.md). The preview images are for gallery presentation; applying wallpapers or changing desktop configuration is not part of the current application.

## Current limitations

- **Apply is intentionally disabled.** Aurora Studio does not yet apply theme colors or wallpapers to the desktop or configure Hyprland, DMS, terminals, or other applications.
- Installed builds are not relocatable because bundled theme data is currently referenced from the source tree.
- Theme import and export support Aurora's JSON format only.
- The editor changes theme metadata and color values; it does not edit or generate artwork.

## Development roadmap

1. Design and implement a safe, opt-in desktop-application workflow with preview, validation, and rollback before enabling Apply.
2. Package theme data and assets so installed builds work without the source checkout.
3. Expand automated coverage for the QML gallery, responsive layouts, and import/export edge cases.
4. Explore additional theme integrations only where stable, documented interfaces are available.

## License and redistribution

No project-wide license is included yet. The repository previously contained no license or source provenance records, so the code and pre-existing SVG artwork have not been verified for release under MIT or another reuse license. Until a license is selected and ownership/provenance is confirmed, no project-wide permission to reuse, modify, or redistribute these files is granted. The generated portrait previews have separate provenance notes in `assets/README.md`; that does not settle the rights for the other project files.
