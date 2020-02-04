#pragma once

#include "stdafx.h"
#include <vector>
#include "Allocator.h"

using Microsoft::WRL::ComPtr;

const short kNumFrames = 2;
const short kNumBuffers = 2;
namespace Sigma
{
	struct Frame
	{
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12Resource> m_renderTarget;
		D3D12_CPU_DESCRIPTOR_HANDLE m_renderTargetsHandle;
		UINT64 m_fenceValue;
	};

	class Game
	{
	public:
		Game(std::string title, int width, int height, HINSTANCE hInstance);
		virtual ~Game(void);
		int Run();

	protected:

		std::string m_title;

		HINSTANCE m_hInstance;
		HWND m_hWindow;

		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<ID3D12GraphicsCommandList> m_commandLists[kNumFrames];
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<ID3D12CommandQueue> m_copyQueue;
		ComPtr<ID3D12CommandAllocator> m_copyCommandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_copyCommandList;
		ComPtr<ID3D12CommandAllocator> m_commandAllocators[kNumFrames];
		ComPtr<ID3D12Device1> m_device;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap;
		ComPtr<ID3D12Resource> m_renderTargets[kNumBuffers];
		D3D12_CPU_DESCRIPTOR_HANDLE m_renderTargetsHandles[kNumBuffers];
		int m_currentFrame;
		int m_currentBuffer;
		HANDLE m_fenceEvent;
		ComPtr<ID3D12Fence> m_endOfFrameFence;
		UINT64 m_lastSubmittedFrameFenceValue;
		HANDLE m_swapChainWait;
		UINT64 m_frameCounter;
		HANDLE m_haltFenceEvent;
		ComPtr<ID3D12Fence> m_haltFence;
		UINT64 m_haltFenceValue;

		ComPtr<ID3D12Resource> m_vertexBuffer;
		ComPtr<ID3D12Resource> m_textureRes;

		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12RootSignature> m_rootSignature;

		int m_windowWidth;
		int m_windowHeight;
		int m_bufferWidth;
		int m_bufferHeight;
		
		ComPtr<ID3D12Heap> m_uploadHeap;
		std::unique_ptr<LinearHeapAllocator> m_uploadAllocator;
		ComPtr<ID3D12Heap> m_heap;

	private:
		void SetupWindow();
		void SetupD3D();
		void CleanD3D();
		void CleanWindow();

		void ResizeSwapChainBuffers();
		void WaitForGPU();
		void WaitForGPUCopy();

		Frame GetNewFrame();
		void GameLoop();

		LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		DWORD WINAPI GameLoop(HANDLE message);
		static DWORD WINAPI StaticGameLoop(LPVOID StartParam);
	};
}