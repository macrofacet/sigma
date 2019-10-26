#include "stdafx.h"
#include "Game.h"


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow)
{
	SetThreadDescription(GetCurrentThread(), L"MainThread");
	{
		Sigma::Game Game("Sigma", 768, 480, hInstance);
		Game.Run();
	}

#ifdef _DEBUG
	ComPtr<IDXGIDebug> debugController;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug), (void**)debugController.GetAddressOf());
	debugController->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
#endif

	return 0;
}