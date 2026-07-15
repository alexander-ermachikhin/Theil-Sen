# Theil-Sen
Theil–Sen estimator for LabVIEW 2021 SP1

Сan read about this method [here](https://en.wikipedia.org/wiki/Theil%E2%80%93Sen_estimator)

Using "example.vi," you can test the operation of "Theil-Sen.vi." Here's an example:

![Example of use](./Pictures/example.bmp)

You can choose one of three "Sen parameter" options: "Auto" – automatic checking for duplicate X values, "On" – duplicate checking is always enabled, and "Off" – checking is disabled.

![Auto](./Pictures/Sen_parameter_Auto.bmp)

Also, checking for array length equality is enabled by default. It's best to leave it enabled.
Added files for LabVIEW 2010 in a separate folder.

Checks have been added to the Theil-Sen.vi file.

![check_1](./Pictures/check_1.bmp)
![check_2](./Pictures/check_2.bmp)
![check_3](./Pictures/check_3.bmp)

Creating a program in C++ and DLL. A wrapper for the dll has been created.
![Wrapper](./Pictures/wrapper.bmp)
## How to Build the DLL

This project uses **CMake**. To compile the `LabVIEW_TheilSen.dll` on your machine:

1. Ensure you have a C++ compiler installed (MinGW-w64 or MSVC x64) and **CMake** (built into CLion / Visual Studio).
2. Open `CMakeLists.txt` and verify the path to your LabVIEW `cintools` directory:
   ```cmake
   include_directories("C:/Program Files/National Instruments/LabVIEW 2021/cintools")
   ```
   *If you are using a different version of LabVIEW, simply update this path to point to your `cintools` folder.*
3. Switch the build configuration to **Release** mode.
4. Run the build command (`Ctrl + F9` in CLion or `cmake --build . --config Release` via terminal).
5. The compiled, fully standalone `LabVIEW_TheilSen.dll` will be generated in your release folder.
