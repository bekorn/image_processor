# ImageProcessor

Finally come up with a project idea to learn about DLLs in C++.

Inspired by this repo (shared on ÜNOG) https://github.com/Gord10/PaletteConverterCSharp

```txt
-The-Gist------------------------------------------------------------------------------------------------
	1- Application will load and show an image.
	2- It will load a DLL that has an image processing function.
	3- Then it will show both the original and the processed image.
	4- It will be possible to modify the image processing function, complile a new DLL and goto 2
---------------------------------------------------------------------------------------------------------


The DLL will implement
	void init(const & image);
	void process(& image);

With the seperate global memory of the DLL, it should be possible to keep a persistant state between init and process calls.


Crude implementation:
- init window and opengl context (w/vsync)
- load image using stb_img and make a texture
- copy the original texture into the processed texture
- show active texture
- on Space
	- switch active texture
- on Enter
	- compile the cpp file into a DLL, load it and call init
	- call process and update processed texture


Possible dependencies:
- stb_img
- GLFW
- GLAD


Extra features:
- Save the processed image
- Basic UI for a diff slider
- Auto reload on file change
- Learn Windows API, ditch GLFW

```