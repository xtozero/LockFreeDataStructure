#pragma once

#include <iostream>
#include <atomic>
#include <new>

template <typename T, unsigned int MaxTotalItems, unsigned int ItemsPerPage>
class LockFreeFixedMemoryPool
{
public:
	unsigned int Allocate( unsigned int count = 1 )
	{
		unsigned int baseIdx = std::atomic_fetch_add( reinterpret_cast<std::atomic_uint32_t*>( &m_index ), count );
		if ( baseIdx + count > MaxTotalItems )
		{
			throw std::bad_alloc( );
		}

		for ( unsigned int i = baseIdx, end = baseIdx + count; i < end; ++i )
		{
			new ( GetRawItem( i ) ) T( );
		}

		return baseIdx;
	}

	T* GetItem( unsigned int index )
	{
		if ( index == 0 )
		{
			return nullptr;
		}

		unsigned int blockIdx = index / ItemsPerPage;
		unsigned int itemIdx = index % ItemsPerPage;

		return &m_blocks[blockIdx][itemIdx];
	}

	LockFreeFixedMemoryPool( )
	{
		++m_index;
		for ( unsigned int i = 0; i < NumBlocks; ++i )
		{
			m_blocks[i] = nullptr;
		}
	}

	~LockFreeFixedMemoryPool( )
	{
		for ( unsigned int i = 0; i < NumBlocks; ++i )
		{
			delete[] m_blocks[i];
		}
	}

	LockFreeFixedMemoryPool( const LockFreeFixedMemoryPool& ) = delete;
	LockFreeFixedMemoryPool( LockFreeFixedMemoryPool&& ) = delete;
	LockFreeFixedMemoryPool& operator=( const LockFreeFixedMemoryPool& ) = delete;
	LockFreeFixedMemoryPool& operator=( LockFreeFixedMemoryPool&& ) = delete;

private:
	void* GetRawItem( unsigned int index )
	{
		unsigned int blockIdx = index / ItemsPerPage;
		unsigned int itemIdx = index % ItemsPerPage;

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

	constexpr static unsigned int NumBlocks = ( MaxTotalItems + ItemsPerPage - 1 ) / ItemsPerPage;

	unsigned char padding[64];
	unsigned int m_index = 0;
	unsigned char padding1[64];
	T* volatile m_blocks[NumBlocks] = {};
	unsigned char padding2[64];
};