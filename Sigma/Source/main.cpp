#include "stdafx.h"
#include "Game.h"


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow)
{
	SetThreadDescription(GetCurrentThread(), L"MainThread");
	Sigma::Game Game("Sigma", 768, 480, hInstance);
	Game.Run();
	return 0;
}