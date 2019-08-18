#pragma once
/*
Goal :
- excercise in rendering engine architecture
- tool to quickly sketch rendering techniques and try API features

steps
1. draw a triangle on screen
- setup swapchain
- setup command buffers
- setup shader compilation
- setup pipeline and draw
2. draw 3d game




==== Big Engine Architecture ====
group objects into pass
objects need to have a definition of a geometry and a (partial) shader for each pass
each pass do whatever it wants
- allocating temporary resources (lifetime management : frame, view, scene/level, game)
- issuing draw/dispatch calls
*/




/*
I want :
- Vsync
- Two swap chain buffers
- CPU and GPU in parallel

CPU is preparing draw calls, then call present
GPU execute those draw calls and renders to the backbuffer
Swap chain flip on vsync


CPU0 | CPU1 | CPU2 | CPU3 |
            ---
     | GPU0-B | GPU1-A |
	                Q---------------F
	          Q---------------F
	 Q--------------F (waits for B's resource transition from GPU)
|    A    |    A    |    B    |    A    |
                 
*/