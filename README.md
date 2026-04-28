# clouds

Cloud Rendering using vulkan

ONLY LINUX RIGHT NOW!!!

Supports gltf loading right now.

Lighting:
  - Blinn-Phong lighting model
  - Directional lights
  - Point lights
  - Spot lights

# Build

Build and run application

```c
make
./build/main
```

Clean source files

```c
make clean
```

Clean everything (external libaries)

```c
make full_clean
```

# dependencies
- cgltf
- glfw
- vulkan
- shaderc
- FastNoiseLite
- stbi
