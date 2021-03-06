// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "SDKDDKVer.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <xstring>
#include <iostream>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#define USE_PIX
#include "pix3.h"

#include <DirectXMath.h>
#include <wrl.h>