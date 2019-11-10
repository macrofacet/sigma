#pragma once
#include "stdafx.h"


using Microsoft::WRL::ComPtr;

class LinearAllocator
{
public:
	LinearAllocator(ComPtr<ID3D12Device> device, ComPtr<ID3D12Heap> heap):m_device(device), m_heap(heap), m_offset(0)
	{
	}

	ID3D12Resource* Allocate(const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES states, const D3D12_CLEAR_VALUE* pOptimizedClearValue)
	{
		D3D12_RESOURCE_ALLOCATION_INFO info = m_device->GetResourceAllocationInfo(0, 1, desc);

		ID3D12Resource* resource;
		m_offset = (m_offset + info.Alignment - 1) & ~(info.Alignment - 1);
		m_device->CreatePlacedResource(m_heap.Get(), m_offset, desc, states, pOptimizedClearValue, IID_PPV_ARGS(&resource));
		m_offset += info.SizeInBytes;
		return resource;
	}

	void Reset()
	{
		m_offset = 0;
	}

private:
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Heap> m_heap;
	UINT64 m_offset;
};