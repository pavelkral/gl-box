# OpenGL tiny framework


# Building

The project is built using qmake or cmake.

**NOTE:** 

## Build from Source

### Dependencies

* [irrKlang](https://www.ambiera.com/irrklang/index.html) 
* [stb image](https://github.com/nothings/stb/blob/master/stb_image.h)
* [assimp](https://github.com/assimp/assimp)
* [glad](https://github.com/Dav1dde/glad)
* [ThreadPool](https://github.com/progschj/ThreadPool)
* [glm](https://github.com/g-truc/glm)
* [glfw](https://github.com/glfw/glfw)
* [imgui](https://github.com/glfw/glfw)

### Build Dependencies

- Qmake
- CMAKE

### Build Steps camke
```
$ mkdir build
$ cd build
$ cmake --build . -- -j8

```
### Build Steps qmake
```
$ qmake
$ make
```

