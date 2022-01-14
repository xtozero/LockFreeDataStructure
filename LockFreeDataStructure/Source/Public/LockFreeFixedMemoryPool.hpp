#pragma once

#include "SizedTypes.h"

#include <iostream>
#include <atomic>
#include <new>

template <typename T, uint32 MaxTotalItems, uint32 ItemsPerPage, uint32 PaddingForCacheLine>
class LockFreeFixedMemoryPool
{
public:
	uint32 Allocate( uint32 count = 1 )
	{
		uint32 baseIdx = std::atomic_fetch_add( reinterpret_cast<std::atomic_uint32_t*>( &m_index ), count );
		if ( baseIdx + count > MaxTotalItems )
		{
			throw std::bad_alloc( );
		}

		for ( uint32 i = baseIdx, end = baseIdx + count; i < end; ++i )
		{
			new ( GetRawItem( i ) ) T( );
		}

		return baseIdx;
	}

	T* GetItem( uint32 index )
	{
		if ( index == 0 )
		{
			return nullptr;
		}

		uint32 blockIdx = index / ItemsPerPage;
		uint32 itemIdx = index % ItemsPerPage;

		return &m_blocks[blockIdx][itemIdx];
	}

	LockFreeFixedMemoryPool( )
	{
		++m_index;
		for ( uint32 i = 0; i < NumBlocks; ++i )
		{
			m_blocks[i] = nullptr;
		}
	}

	~LockFreeFixedMemoryPool( )
	{
		for ( uint32 i = 0; i < NumBlocks; ++i )
		{
			delete[] m_blocks[i];
		}
	}

	LockFreeFixedMemoryPool( const LockFreeFixedMemoryPool& ) = delete;
	LockFreeFixedMemoryPool( LockFreeFixedMemoryPool&& ) = delete;
	LockFreeFixedMemoryPool& operator=( const LockFreeFixedMemoryPool& ) = delete;
	LockFreeFixedMemoryPool& operator=( LockFreeFixedMemoryPool&& ) = delete;

private:
	void* GetRawItem( uint32 index )
	{
		uint32 blockIdx = index / ItemsPerPage;
		uint32 itemIdx = index % ItemsPerPage;

		if ( m_blocks[blockIdx] == nullptr )
		{
			T* newBlock = new T[ItemsPerPage];
			T* expected = nullptr;
			bool success = std::atomic_compare_exchange_strong( reinterpret_cast<volatile std::atomic_ptrdiff_t*>( &m_blocks[blockIdx] ), reinterpret_cast<std::ptrdiff_t*>( &expected ), *reinterpret_cast<std::ptrdiff_t*>( &newBlock ) );

			if ( success == false )
			{
				delete[] newBlock;
			}
		}

		return static_cast<void*>( &m_blocks[blockIdx][itemIdx] );
	}

	constexpr static uint32 NumBlocks = ( MaxTotalItems + ItemsPerPage - 1 ) / ItemsPerPage;

	uint8 padding1[PaddingForCacheLine];
	uint32 m_index = 0;
	uint8 padding2[PaddingForCacheLine];
	T* m_blocks[NumBlocks] = {};
	uint8 padding3[PaddingForCacheLine];
};