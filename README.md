# image_processor

Intended audience: experienced with C++, curious about DLLs.


### How to use

> **Note** Windows only, requires MSVC compiler 2022(tested), 2019, or 2017.

`TODO(bekorn): Tutorial/showcase video`

All the bat files and the program must be run from the project's root directory.

First, run the `config.bat` once. Then run `build.bat` to build the program. `run.bat` will start the program with MrIncredible.png. A window should open with the image. Use 1, 2, 3, 4 to switch between textures (check window title), select a process texture. Pick a file from the proc folder and drop it into the window. The file should compile and execute, result will be saved to the selected texture. Try editing the cpp file. When you press Space, it should rebuild and executed again.

Main program accepts `<img_rel_path> <[optional]proc_abs_path>` arguments, modify `run.bat` for easy use.

`build_dll.bat <proc_abs_path>` will build a process.

If you want to debug a process: delete the build_dll directory if it is generated. Change `rel_args` to `deb_args` in `build_dll.bat`. Build the process. Configure your debugger to provide both the arguments to the main program, and set the working directory correctly (vscode debugging configuration is commited). Run the main program with your debugger.


### What to read

The `///--- Interop` section in [src/main.cpp](src/main.cpp) uses  Windows API to load/bind/free a dll.

[build_dll.bat](build_dll.bat) builds the dll (precompiled headers makes it a bit convoluted).

[src/process.hpp](src/process.hpp) ensures that names are same for both main and dll.

[src/process_wrapper.cpp](src/process_wrapper.cpp) is the actual file that is compiled. It helps to statically check the signatures of exported functions, also makes it easier to write new dlls.


### About

Inspired by this repo (shared on ÜNOG) https://github.com/Gord10/PaletteConverterCSharp

Dependencies:
- GLFW
- GLAD
- stb_img


Extra features:
- [ ] Save the processed image
- [ ] Auto reload on file change


Resources:
- MrIncredible.png, https://disney.fandom.com/wiki/Mr._Incredible/Gallery?file=Incredibles_2_-_Concept_Art_-_Mr._Incredible.jpg
