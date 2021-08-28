#pragma once

#include "LockFreeFixedMemoryPool.hpp"
#include "SizedTypes.h"

#include <atomic>
#include <limits>

#include <Windows.h>

constexpr uint32 LockFreeCacheLineBytes = 64;

struct alignas(8) StampIndex
{
public:
	void Set( uint32 index, uint32 stamp )
	{
		m_stampIndex = ( static_cast<uint64>( stamp ) << IndexBits ) | index;
	}

	void SetIndex( uint32 index )
	{
		Set( index, GetStamp( ) );
	}

	void SetStamp( uint32 stamp )
	{
		Set( GetIndex( ), stamp );
	}

	uint32 GetIndex( ) const
	{
		return static_cast<uint32>( m_stampIndex );
	}

	uint32 GetStamp( ) const
	{
		return static_cast<uint32>( m_stampIndex >> IndexBits );
	}

	bool CompareExchange( const StampIndex& expected, const StampIndex& desired )
	{
		uint64 expectedStampIndex = expected.m_stampIndex;
		return std::atomic_compare_exchange_strong( reinterpret_cast<std::atomic_llong*>( this ), reinterpret_cast<int64*>( &expectedStampIndex ), desired.m_stampIndex );
	}

	constexpr StampIndex( ) = default;
	StampIndex( const StampIndex& other )
	{
		( *this ) = other;
	}
	StampIndex& operator=( const StampIndex& other )
	{
		if ( this != &other )
		{
			m_stampIndex = std::atomic_load( reinterpret_cast<const std::atomic_llong*>( &other ) );
		}

		return *this;
	}
	StampIndex( StampIndex&& other ) = delete;
	StampIndex& operator=( StampIndex&& other ) = delete;

	friend bool operator==( const StampIndex& lhs, const StampIndex& rhs )
	{
		return lhs.m_stampIndex == rhs.m_stampIndex;
	}

	friend bool operator!=( const StampIndex& lhs, const StampIndex& rhs )
	{
		return !( lhs == rhs );
	}

private:
	static constexpr uint32 IndexBits = std::numeric_limits<uint32>::digits;
	uint64 m_stampIndex = 0;
};

struct IndexedLockFreeLink
{
	StampIndex m_nextStampIndex;
	void* m_data;
	uint32 m_nextIndex;
};

struct LockFreeLinkPolicy
{
	constexpr static uint32 MaxLockFreeLink = 1 << 26;

	static IndexedLockFreeLink* IndexToLink( uint32 index )
	{
		return m_linkAllocator.GetItem( index );
	}

	static uint32 AllocLockFreeLink( );
	static void DeallocLockFreeLink( uint32 item );

	static LockFreeFixedMemoryPool<IndexedLockFreeLink, MaxLockFreeLink, 16384, LockFreeCacheLineBytes>& LinkAllocator( )
	{
		return m_linkAllocator;
	}

private:
	static LockFreeFixedMemoryPool<IndexedLockFreeLink, MaxLockFreeLink, 16384, LockFreeCacheLineBytes> m_linkAllocator;
};