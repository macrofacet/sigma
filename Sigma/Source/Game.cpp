#include "stdafx.h"
#include "Game.h"
#include "Defines.h"

namespace Sigma {
	
	ID3D12Resource* CreateTexture2D(ComPtr<ID3D12Device> device, int width, int height)
	{
		D3D12_RESOURCE_DESC desc;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES texHeapProps;
		texHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		texHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		texHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		texHeapProps.CreationNodeMask = 0;
		texHeapProps.VisibleNodeMask = 0;

		ID3D12Resource* resource;
		device->CreateCommittedResource(
			&texHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&resource));
		return resource;
	}

	ID3D12Resource* CreateUploadBuffer(ComPtr<ID3D12Device> device, ComPtr<ID3D12Resource>& resource, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprint)
	{
		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 0;
		heapProps.VisibleNodeMask = 0;

		UINT64 uploadBufferSize = 0;
		UINT numRows = 0;
		UINT64 rowSizeInBytes = 0;
		device->GetCopyableFootprints(&resource->GetDesc(), 0, 1, 0, footprint, &numRows, &rowSizeInBytes, &uploadBufferSize);

		D3D12_RESOURCE_DESC bufDesc;
		bufDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		bufDesc.Height = 1;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.MipLevels = 1;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.SampleDesc.Quality = 0;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufDesc.Width = uploadBufferSize;
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* uploadBuffer;
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));	
		return uploadBuffer;
	}


	void FillBuffer(ComPtr<ID3D12Resource>& buffer, ComPtr<ID3D12Resource>& resource, unsigned pitchInBytes, char* data)
	{
		auto desc = resource->GetDesc();
		char* cpuData;
		buffer->Map(0, nullptr, (void**)&cpuData);
		for (unsigned i = 0; i < desc.Height; i++)
		{
			memcpy(cpuData, data, desc.Width * 4);
			cpuData += pitchInBytes;
			data += desc.Width;
		}
		buffer->Unmap(0, nullptr);
	}

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

		frame.m_commandList->OMSetRenderTargets(1, &frame.m_renderTargetsHandle, FALSE, nullptr);

		const float clearColor[4] = { 1.0f, 1.0f, 0.f, 1.0f };
		frame.m_commandList->ClearRenderTargetView(frame.m_renderTargetsHandle, clearColor, 0, nullptr);
		
		D3D12_VIEWPORT viewport;
		viewport.Height = (float)m_bufferHeight;
		viewport.Width = (float)m_bufferWidth;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissorRect;
		scissorRect.top = 0;
		scissorRect.bottom = m_bufferHeight;
		scissorRect.left = 0;
		scissorRect.right = m_bufferWidth;

		frame.m_commandList->RSSetViewports(1, &viewport);
		frame.m_commandList->RSSetScissorRects(1, &scissorRect);

		frame.m_commandList->SetPipelineState(m_pipelineState.Get());
		frame.m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
		frame.m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		frame.m_commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		frame.m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		frame.m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		frame.m_commandList->DrawInstanced(3, 1, 0, 0);

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
		D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue));
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
#endif

		ComPtr<IDXGIFactory3> factory;
		CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

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
				output->QueryInterface(IID_PPV_ARGS(&output6));
				++i;
			}

			// TODO : Some filtering here
			if (true)
			{
				adapter->QueryInterface(IID_PPV_ARGS(&selectedAdapter));
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
		D3D12CreateDevice(selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));

		// Create command queue
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue));


		D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
		copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyQueue));
		m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyCommandAllocator));
		m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_copyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_copyCommandList));


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
		m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

		unsigned rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

		for (int i = 0; i < kNumBuffers; i++)
		{
			m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
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
			m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]));

			m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[i].Get(), nullptr, IID_PPV_ARGS(&m_commandLists[i]));
			m_commandLists[i]->Close();
		}

		// Fence inserted at the end of the frame (before Present)
		m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_endOfFrameFence));
		m_lastSubmittedFrameFenceValue = 0;
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		// Special fence whenever we need to wait for the GPU to complete everything so far (including Present)
		m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_haltFence));
		m_haltFenceValue = 0;
		m_haltFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();
		m_frameCounter = 0;



		// CREATE RESOURCES
		/*
		D3D12_HEAP_DESC heapDesc = {};
		heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		heapDesc.SizeInBytes = 128 * 1024 * 1024; // 128 Mb
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapDesc.Properties.VisibleNodeMask = 0;
		heapDesc.Properties.CreationNodeMask = 0;
		
		m_device->CreateHeap(&heapDesc, IID_PPV_ARGS(&m_heap));
		*/

		D3D12_DESCRIPTOR_RANGE1 descRange = {};
		descRange.BaseShaderRegister = 0;
		descRange.RegisterSpace = 0;
		descRange.NumDescriptors = 1;
		descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descRange.OffsetInDescriptorsFromTableStart = 0;
		descRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

		D3D12_ROOT_PARAMETER1 param = {};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		param.DescriptorTable.NumDescriptorRanges = 1;
		param.DescriptorTable.pDescriptorRanges = &descRange;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.RegisterSpace = 0;
		staticSampler.ShaderRegister = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
		rootSignatureDesc.Desc_1_1.pStaticSamplers = &staticSampler;
		rootSignatureDesc.Desc_1_1.NumParameters = 1;
		rootSignatureDesc.Desc_1_1.pParameters = &param;
		ComPtr<ID3DBlob> outputBlob;
		ComPtr<ID3DBlob> errorBlob;
		D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &outputBlob, &errorBlob);
		if (errorBlob != nullptr)
		{
			char* buffer = (char*)errorBlob->GetBufferPointer();
		}
		m_device->CreateRootSignature(0, outputBlob->GetBufferPointer(), outputBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));

		D3D12_INPUT_ELEMENT_DESC inputDescPos = {};
		inputDescPos.SemanticName = "POSITION";
		inputDescPos.SemanticIndex = 0;
		inputDescPos.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		inputDescPos.InputSlot = 0;
		inputDescPos.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputDescPos.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		inputDescPos.InstanceDataStepRate = 0;

		D3D12_INPUT_ELEMENT_DESC inputDescUV = {};
		inputDescUV.SemanticName = "TEXCOORD";
		inputDescUV.SemanticIndex = 0;
		inputDescUV.Format = DXGI_FORMAT_R32G32_FLOAT;
		inputDescUV.InputSlot = 0;
		inputDescUV.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputDescUV.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		inputDescUV.InstanceDataStepRate = 0;

		D3D12_INPUT_ELEMENT_DESC inputs[] = { inputDescPos, inputDescUV };

		HANDLE hFile = CreateFile("PixelShader.cso", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		LARGE_INTEGER  size;
		GetFileSizeEx(hFile, &size);
		std::vector<char> pixelShaderBuffer(size.QuadPart);
		DWORD byteReadPS;
		ReadFile(hFile, pixelShaderBuffer.data(), (DWORD)size.QuadPart, &byteReadPS, nullptr);

		hFile = CreateFile("VertexShader.cso", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		GetFileSizeEx(hFile, &size);
		std::vector<char> vertexShaderBuffer(size.QuadPart);
		DWORD byteReadVS;
		ReadFile(hFile, vertexShaderBuffer.data(), (DWORD)size.QuadPart, &byteReadVS, nullptr);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = m_rootSignature.Get();
		desc.VS.pShaderBytecode = vertexShaderBuffer.data();
		desc.VS.BytecodeLength = vertexShaderBuffer.size();
		desc.PS.pShaderBytecode = pixelShaderBuffer.data();
		desc.PS.BytecodeLength = pixelShaderBuffer.size();
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		desc.DepthStencilState.DepthEnable = false;
		desc.DepthStencilState.StencilEnable = false;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.InputLayout.NumElements = 2;
		desc.InputLayout.pInputElementDescs = inputs;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = UINT_MAX;
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;

		m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipelineState));
				
		// Create the vertex buffer.
		{
			// Define the geometry for a triangle.
			float triangleVertices[] =
			{
				0.0f, 0.25f, 0.0f, 0.0f, 0.0f,
				0.25f, -0.25f, 0.0f, 1.0f, 0.0f,
				-0.25f, -0.25f, 0.0f, 1.0f, 1.0f,
			};

			const UINT vertexBufferSize = sizeof(triangleVertices);

			D3D12_HEAP_PROPERTIES heapProps;
			heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProps.CreationNodeMask = 0;
			heapProps.VisibleNodeMask = 0;

			D3D12_RESOURCE_DESC resDesc;
			resDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resDesc.Height = 1;
			resDesc.DepthOrArraySize = 1;
			resDesc.MipLevels = 1;
			resDesc.SampleDesc.Count = 1;
			resDesc.SampleDesc.Quality = 0;
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resDesc.Width = vertexBufferSize;
			resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resDesc.Format = DXGI_FORMAT_UNKNOWN;
			resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			m_device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_vertexBuffer));

			// Copy the triangle data to the vertex buffer.
			UINT8* pVertexDataBegin;
			D3D12_RANGE readRange{ 0, 0 };
			m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			m_vertexBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
			m_vertexBufferView.StrideInBytes = sizeof(float) * 5;
			m_vertexBufferView.SizeInBytes = vertexBufferSize;

			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			srvHeapDesc.NodeMask = 0;
			srvHeapDesc.NumDescriptors = 1;
			m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));

			m_textureRes = CreateTexture2D(m_device, 128, 128);

			{
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
				ComPtr<ID3D12Resource> uploadBuffer = CreateUploadBuffer(m_device, m_textureRes, &footprint);

				std::vector<int> pixels;
				for (int i = 0; i < 128; i++)
				{
					for (int j = 0; j < 128; j++)
					{
						pixels.push_back(rand() << 8 | rand());
					}
				}

				FillBuffer(uploadBuffer, m_textureRes, footprint.Footprint.RowPitch, (char*)pixels.data());

				D3D12_TEXTURE_COPY_LOCATION Dst = {};
				Dst.pResource = m_textureRes.Get();
				Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				Dst.SubresourceIndex = 0;

				D3D12_TEXTURE_COPY_LOCATION Src = {};
				Src.pResource = uploadBuffer.Get();
				Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				Src.PlacedFootprint = footprint;

				m_copyCommandList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
				m_copyCommandList->Close();
				ID3D12CommandList* commandLists[] = { m_copyCommandList.Get() };
				m_copyQueue->ExecuteCommandLists(1, commandLists);

				WaitForGPUCopy();
				m_copyCommandAllocator->Reset();
				m_copyCommandList->Reset(m_copyCommandAllocator.Get(), nullptr);

				{
					// This could be done at the beginning of the next "real" frame instead of here
					auto frame = GetNewFrame();
					D3D12_RESOURCE_BARRIER barrier = {};
					barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
					barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
					barrier.Transition.pResource = m_textureRes.Get();
					barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					frame.m_commandList->ResourceBarrier(1, &barrier);
					frame.m_commandList->Close();
					ID3D12CommandList* commandLists[] = { frame.m_commandList.Get() };
					m_commandQueue->ExecuteCommandLists(1, commandLists);
					m_commandQueue->Signal(m_endOfFrameFence.Get(), frame.m_fenceValue);
				}
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = -1;

			m_device->CreateShaderResourceView(m_textureRes.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());

		}
	}

	void Game::CleanD3D()
	{
		WaitForGPU();
		WaitForGPUCopy();
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
			m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
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

	void Game::WaitForGPUCopy()
	{
		m_copyQueue->Signal(m_haltFence.Get(), ++m_haltFenceValue);
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