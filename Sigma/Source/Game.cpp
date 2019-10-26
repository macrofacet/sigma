#include "stdafx.h"
#include "Game.h"
#include "Defines.h"


namespace Sigma {

	Game::Game(std::string title, int width, int height, HINSTANCE hInstance) : 
		m_title(title), 
		m_windowWidth(width), 
		m_windowHeight(height),
		m_hInstance(hInstance)
	{
		SetupWindow();
		SetupD3D();

		ShowWindow(m_hWindow, SW_SHOWNORMAL);
		UpdateWindow(m_hWindow);
	}


	Game::~Game(void)
	{
		CleanD3D();
		CleanWindow();
	}


	void Game::SetupWindow()
	{
		WNDCLASSEX wcex;

		std::string windowClassName = "SigmaMainWindow";

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = Game::StaticWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = m_hInstance;
		wcex.hIcon = nullptr;
		wcex.hIconSm = nullptr;
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = (LPCSTR)windowClassName.c_str();

		RegisterClassEx(&wcex);

		RECT clientRect = { 0, 0, m_windowWidth, m_windowHeight };
		AdjustWindowRect(&clientRect, WS_OVERLAPPEDWINDOW, FALSE);

		m_hWindow = CreateWindow((LPCSTR)windowClassName.c_str(), (LPCSTR)m_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, NULL, NULL, m_hInstance, NULL);

		SetWindowLongPtr(m_hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	}

	void Game::CleanWindow()
	{
		if (m_hWindow)
			CloseHandle(m_hWindow);
	}

	int Game::Run()
	{
		MSG msg;
		
		bool running = true;

		// Main message loop:
		while (running)
		{

			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					running = false;
			}
			
			if (m_windowWidth != m_bufferWidth || m_windowHeight != m_bufferHeight)
			{
				ResizeSwapChainBuffers();
			}
			
			GameLoop();
		}


		return (int)msg.wParam;
	}

	Frame Game::GetNewFrame()
	{
		PIXScopedEvent(PIX_COLOR_INDEX(1), "New frame");
		
		{
			PIXScopedEvent(PIX_COLOR_INDEX(2), "Waiting for free back buffer");
			// Make sure we have a free back buffer
			WaitForSingleObject(m_swapChainWait, INFINITE);
		}

		Frame frame;
		frame.m_commandAllocator = m_commandAllocators[m_currentFrame];
		frame.m_commandList = m_commandLists[m_currentFrame];
		frame.m_fenceValue = ++m_lastSubmittedFrameFenceValue;
		frame.m_renderTarget = m_renderTargets[m_currentBuffer];
		frame.m_renderTargetsHandle = m_renderTargetsHandles[m_currentBuffer];

		// Make sure GPU is done with our previous usage of this command allocator before resetting it
		// This assumes that command allocators are used in order, and their command lists are executed when the
		// GPU reach the end of the frame fence
		if (frame.m_fenceValue >= kNumFrames && m_endOfFrameFence->GetCompletedValue() < frame.m_fenceValue - kNumFrames)
		{
			PIXScopedEvent(PIX_COLOR_INDEX(2), "Waiting for CL exec");
			m_endOfFrameFence->SetEventOnCompletion(frame.m_fenceValue - kNumFrames, m_fenceEvent);
			WaitForSingleObject(m_fenceEvent, INFINITE);
			PIXNotifyWakeFromFenceSignal(m_fenceEvent);
		}

		frame.m_commandAllocator->Reset();
		frame.m_commandList->Reset(frame.m_commandAllocator.Get(), nullptr);

		return frame;
	}


	void Game::GameLoop()
	{
		PIXBeginEvent(m_commandQueue.Get(), PIX_COLOR_INDEX(0), "Frame %d", m_frameCounter);
		Frame frame = GetNewFrame();
		PIXBeginEvent(frame.m_commandList.Get(), PIX_COLOR_INDEX(0), "CMDList %d", frame.m_commandList.Get());
	
		{
			D3D12_RESOURCE_TRANSITION_BARRIER transition = {};
			transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			transition.pResource = frame.m_renderTarget.Get();
			transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Transition = transition;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			frame.m_commandList->ResourceBarrier(1, &barrier);
		}

		const float clearColor[4] = { 1.0f, 1.0f, 0.f, 1.0f };
		frame.m_commandList->ClearRenderTargetView(frame.m_renderTargetsHandle, clearColor, 0, nullptr);

		{
			D3D12_RESOURCE_TRANSITION_BARRIER transition = {};
			transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			transition.pResource = frame.m_renderTarget.Get();
			transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Transition = transition;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			frame.m_commandList->ResourceBarrier(1, &barrier);
		}

		PIXEndEvent(frame.m_commandList.Get());
		frame.m_commandList->Close();
		ID3D12CommandList* commandLists[] = { frame.m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(1, commandLists);

		m_commandQueue->Signal(m_endOfFrameFence.Get(), frame.m_fenceValue); 
		m_swapChain->Present(1, 0);
		
		m_currentFrame = (m_currentFrame + 1) % kNumFrames;
		m_currentBuffer = m_swapChain->GetCurrentBackBufferIndex();
		m_frameCounter++;
		PIXEndEvent(m_commandQueue.Get());
	}

	void Game::SetupD3D()
	{
		unsigned int dxgiFactoryFlags = 0;

#ifdef _DEBUG
		ComPtr<ID3D12Debug> debugController;
		D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)debugController.GetAddressOf());
		debugController->EnableDebugLayer();

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()));
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

		
		
#endif

		ComPtr<IDXGIFactory3> factory;
		CreateDXGIFactory2(dxgiFactoryFlags, __uuidof(IDXGIFactory3), (void**)factory.GetAddressOf());

		// Enumerate all adapters
		unsigned i = 0;
		ComPtr<IDXGIAdapter1> adapter;
		std::vector<ComPtr<IDXGIAdapter1>> adapters;
		while (factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND)
		{
			adapters.push_back(adapter);
			++i;
		}

		ComPtr<IDXGIAdapter3> selectedAdapter = nullptr;
		for (auto adapter : adapters)
		{
			unsigned int i = 0;
			ComPtr<IDXGIOutput> output;
			while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
			{
				ComPtr<IDXGIOutput6> output6;
				output->QueryInterface(__uuidof(IDXGIOutput6), (void**)output6.GetAddressOf());
				++i;
			}

			// TODO : Some filtering here
			if (true)
			{
				adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)selectedAdapter.GetAddressOf());
				break;
			}
		}

		// TODO : error handling in case there's no matching adapter
		if (selectedAdapter == nullptr)
			return;

		// Fetch memory information
		{
			DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
			selectedAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo);
			std::cout << memoryInfo.Budget << std::endl;
		}
		// Create logical device
		D3D12CreateDevice(selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)m_device.GetAddressOf());
		
		// Create command queue
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		m_device->CreateCommandQueue(&commandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)m_commandQueue.GetAddressOf());

		// Create swap chain, aiming for minimum latency with a waitable object and two frame buffer
		m_bufferWidth = m_windowWidth;
		m_bufferHeight = m_windowHeight;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_bufferWidth;
		swapChainDesc.Height = m_bufferHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = kNumBuffers;
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

		ComPtr<IDXGISwapChain1> swapChain1;
		factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hWindow, &swapChainDesc, nullptr, nullptr, swapChain1.GetAddressOf());
		
		swapChain1.As(&m_swapChain);

		m_swapChain->SetMaximumFrameLatency(kNumFrames);
		m_swapChainWait = m_swapChain->GetFrameLatencyWaitableObject();

		// Create render target views for the swap chain buffers
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		rtvHeapDesc.NumDescriptors = kNumBuffers;
		m_device->CreateDescriptorHeap(&rtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)m_rtvHeap.GetAddressOf());

		unsigned rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

		for (int i = 0; i < kNumBuffers; i++)
		{
			m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)m_renderTargets[i].GetAddressOf());
			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
			m_renderTargetsHandles[i] = rtvHandle;
			rtvHandle.ptr += rtvDescriptorSize;
		}

		// Create a command allocator for each frame (and a command list - we could create more than one)
		// Command Allocator needs to be alive as long as the GPU is using it,
		// so if we want two frames, we need one command allocator for the in-flight frame
		// and one for the one we are building now
		for (int i = 0; i < kNumFrames; i++)
		{			
			// Create command allocator
			m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)m_commandAllocators[i].GetAddressOf());

			m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[i].Get(), nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)m_commandLists[i].GetAddressOf());
			m_commandLists[i]->Close();
		}

		// Fence inserted at the end of the frame (before Present)
		m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)m_endOfFrameFence.GetAddressOf());
		m_lastSubmittedFrameFenceValue = 0;
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	
		// Special fence whenever we need to wait for the GPU to complete everything so far (including Present)
		m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)m_haltFence.GetAddressOf());
		m_haltFenceValue = 0;
		m_haltFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();
		m_frameCounter = 0;			
	}

	void Game::CleanD3D()
	{
		WaitForGPU();
	}

	void Game::ResizeSwapChainBuffers()
	{
		// To resize the swap chain buffers, we need to stop submitting commands to their
		// associated command allocators and wait for the GPU to finish everything (including Present call)
		WaitForGPU();
		
		for (int i = 0; i < kNumFrames; i++)
		{
			DXSafeCall(m_commandAllocators[i]->Reset());
		}

		for (int i = 0; i < kNumBuffers; i++)
		{
			if (m_renderTargets[i] != nullptr)
			{
				m_renderTargets[i].Reset();
			}
		}

		m_bufferWidth = m_windowWidth;
		m_bufferHeight = m_windowHeight;

		DXSafeCall(m_swapChain->ResizeBuffers(kNumBuffers, m_bufferWidth, m_bufferHeight, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

		unsigned rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

		for (int i = 0; i < kNumBuffers; i++)
		{
			m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)m_renderTargets[i].GetAddressOf());
			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
			m_renderTargetsHandles[i] = rtvHandle;
			rtvHandle.ptr += rtvDescriptorSize;
		}


		m_currentBuffer = m_swapChain->GetCurrentBackBufferIndex();
	}

	// Blocking call - Waits for the GPU to complete all of its work submitted until now
	void Game::WaitForGPU()
	{
		m_commandQueue->Signal(m_haltFence.Get(), ++m_haltFenceValue);
		auto currentValue = m_haltFence->GetCompletedValue();
		if (currentValue < m_haltFenceValue)
		{
			m_haltFence->SetEventOnCompletion(m_haltFenceValue, m_haltFenceEvent);
			WaitForSingleObject(m_haltFenceEvent, INFINITE);
			PIXNotifyWakeFromFenceSignal(m_haltFenceEvent);
		}
	}

	LRESULT CALLBACK Game::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		PAINTSTRUCT ps;
		HDC hdc;

		switch (message)
		{
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			break;
		case WM_DESTROY:
			m_hWindow = nullptr;
			PostQuitMessage(0);
			break;
		case WM_SIZE:
		{
			int nWidth = LOWORD(lParam);
			int nHeight = HIWORD(lParam);
			m_windowWidth = nWidth;
			m_windowHeight = nHeight;
			break;
		}
		case WM_MOUSEMOVE:
		{
			break;
		}
		case WM_LBUTTONDOWN:
			break;
		case WM_LBUTTONUP:
			break;
		case WM_RBUTTONDOWN:
			break;
		case WM_RBUTTONUP:
			break;
		case WM_MBUTTONDOWN:
			break;
		case WM_MBUTTONUP:
			break;
		case WM_KEYDOWN:
			break;
		case WM_KEYUP:
		{
			UINT8 keyCode = static_cast<UINT8>(wParam);
			if (keyCode == VK_SPACE)
			{
				BOOL fullscreenState = false;
				m_swapChain->GetFullscreenState(&fullscreenState, nullptr);
				m_swapChain->SetFullscreenState(!fullscreenState, nullptr);
			}
			break;
		}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	}


	LRESULT CALLBACK Game::StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		Game* game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		if (game != nullptr)
			return game->WndProc(hWnd, message, wParam, lParam);
		else
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
}