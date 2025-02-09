# BMP Color Quantization Tool

## Overview
A simple command-line tool to convert a 16-bit BMP image to an 8-bit BMP using the median cut algorithm.

## Features
- Reads 16-bit BMP images
- Reduces colors to 256 using median cut
- Saves as an 8-bit BMP file

## Usage
1. Place `input.bmp` in the same directory.
2. Run the program to generate `output.bmp`.

## Input and Output
### Original 16-bit BMP (Input)
![Input Image](input.png)

### Converted 8-bit BMP (Output)
![Output Image](output.png)

## Compilation

### Using Visual Studio
1. Open Visual Studio and create a new C++ Console Application.
2. Add `bmp_quantizer.cpp` to the project.
3. Set the language standard to C++17 in project settings.
4. Build and run the project.

### Using MSVC
Compile with MSVC:
```sh
cl /EHsc /std=c++17 bmp_quantizer.cpp
```

## License
MIT License

