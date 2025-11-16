The code is built against wxWidgets 3.3.1, I'm using Code::Blocks to build it, so use the Code::Blocks to open the file: `svg_canvas.cbp`.

You need to first clone the code repo of the lunasvg: https://github.com/sammycage/lunasvg

```
git clone https://github.com/sammycage/lunasvg.git
```
Then under the MSYS2's MINGW64 platform (You can try other platform such as UCRT64, but I havn't tried it), build this library as shared library.

```
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./bin .
cmake --build build --config Release
cmake --install build
```

Now, you have all the dll files in your local `bin` folder, and header files in `include` folder.

Such as:

```
# tree
.
├── bin
│   ├── liblunasvg.dll
│   └── libplutovg.dll
└── include
    ├── lunasvg
    │   └── lunasvg.h
    └── plutovg
        └── plutovg.h

5 directories, 4 files
```

Copy those files to a sub folder named `liblunasvg`, which will be used in our `svg_canvas.cbp`.

This is the application screen:

![application](app_main_window.png)

you can use the mouse to drag and drop the icons in the canvas.



