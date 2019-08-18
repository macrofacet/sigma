#pragma once

#include "stdafx.h"
#include <string>
#include <vector>

#ifdef _DEBUG
#define DXSafeCall(x)												\
		{															\
			HRESULT hr = (x);										\
			if(FAILED(hr))											\
			{														\
				char * errDesc = new char[255];						\
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, NULL, (LPTSTR) errDesc, 255, NULL);		\
				OutputDebugString(errDesc);							\
				__debugbreak();										\
			}														\
		}															
#else
#define DXSafeCall(x) x
#endif


#ifdef _SIGMA_STATIC
#define MAKINA_API
#else
#ifdef _SIGMA_EXPORT
#define SIGMA_API __declspec(dllexport)
#else
#define SIGMA_API __declspec(dllimport)
#endif
#endif