# Sigma
Sigma is first and foremost a rendering engine architecture pratice ground, and a rendering feature experiment engine second. Its goal is for me to practice tasks that are not usually part of my job.

For the moment, it is Windows 10 & D3D12 only.

## Timeline
### Basis
- VS project setup
- Command line arguments
- External configuration
- Initialize Window
- Initialize DirectX
    - Debug layer
- Swapchain management
    - Windowed/Bordless fullscreen support
    - 2 buffers, waitable object
    - resolution/adapter selection
- Error handling
- Pix integration
- File loading?
- Jobs/Multithreading
- CPU Memory management
- Console commands/IMGUI

### Triangle/Quad
- Pipeline state object
    - Shader loading
    - Caching?
- Resource binding
    - Resource transitions
    - Descriptor management
- Frame graph?
- GPU Memory management
- Basic Triangle/Quad helpers

### 3D Engine
- Scene Description
    - Geometry (LOD)
    - Materials
    - Textures
- Passes Definitions/Frame graph?
- Culling
- Opaque Deferred Lighting
- Forward transparent lighting
- Shadows
- Post Filters
