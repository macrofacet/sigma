#pragma once
#include "stdafx.h"


using Microsoft::WRL::ComPtr;

class LinearHeapAllocator
{
public:
	LinearHeapAllocator(ComPtr<ID3D12Device> device, ComPtr<ID3D12Heap> heap):m_device(device), m_heap(heap), m_offset(0)
	{
		auto desc = m_heap->GetDesc();
		m_size = desc.SizeInBytes;
	}

	ID3D12Resource* Allocate(const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES states, const D3D12_CLEAR_VALUE* pOptimizedClearValue)
	{
		D3D12_RESOURCE_ALLOCATION_INFO info = m_device->GetResourceAllocationInfo(0, 1, desc);

		auto alignedOffset = (m_offset + info.Alignment - 1) & ~(info.Alignment - 1);

		if(alignedOffset + info.SizeInBytes < m_size)
		{
			ID3D12Resource* resource;
			m_device->CreatePlacedResource(m_heap.Get(), alignedOffset, desc, states, pOptimizedClearValue, IID_PPV_ARGS(&resource));
			m_offset = alignedOffset + info.SizeInBytes;
			return resource;
		}
		else
		{
			return nullptr;
		}
	}

	void Reset()
	{
		m_offset = 0;
	}

private:
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Heap> m_heap;
	UINT64 m_offset;
	UINT64 m_size;
};