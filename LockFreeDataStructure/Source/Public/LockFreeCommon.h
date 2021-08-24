#pragma once

#include "LockFreeFixedMemoryPool.hpp"

#include <atomic>
#include <limits>

#include <Windows.h>

struct alignas(8) StampIndex
{
public:
	void Set( unsigned int index, unsigned int stamp )
	{
		m_stampIndex = ( static_cast<unsigned long long>( stamp ) << IndexBits ) | index;
	}

	void SetIndex( unsigned int index )
	{
		Set( index, GetStamp( ) );
	}

	void SetStamp( unsigned int stamp )
	{
		Set( GetIndex( ), stamp );
	}

	unsigned int GetIndex( ) const
	{
		return static_cast<unsigned int>( m_stampIndex );
	}

	unsigned int GetStamp( ) const
	{
		return static_cast<unsigned int>( m_stampIndex >> IndexBits );
	}

	bool CompareExchange( const StampIndex& expected, const StampIndex& desired )
	{
		unsigned long long expectedStampIndex = expected.m_stampIndex;
		return std::atomic_compare_exchange_strong( reinterpret_cast<std::atomic_llong*>( this ), reinterpret_cast<long long*>( &expectedStampIndex ), desired.m_stampIndex );
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
			if constexpr ( sizeof( void* ) == 8 )
			{
				m_stampIndex = std::atomic_load( reinterpret_cast<const std::atomic_llong*>( &other ) );
			}
			else
			{
				unsigned long long expectedStampIndex = m_stampIndex;
				std::atomic_compare_exchange_strong( reinterpret_cast<std::atomic_llong*>( this ), reinterpret_cast<long long*>( &expectedStampIndex ), other.m_stampIndex );
			}
		}

		return *this;
	}
	StampIndex( StampIndex&& other )
	{
		( *this ) = std::move( other );
	}
	StampIndex& operator=( StampIndex&& other )
	{
		if ( this != &other )
		{
			if constexpr ( sizeof( void* ) == 8 )
			{
				m_stampIndex = std::atomic_load( reinterpret_cast<const std::atomic_llong*>( &other.m_stampIndex ) );
				other.m_stampIndex = 0;
			}
			else
			{
				unsigned long long expectedStampIndex = m_stampIndex;
				std::atomic_compare_exchange_strong( reinterpret_cast<std::atomic_llong*>( this ), reinterpret_cast<long long*>( &expectedStampIndex ), other.m_stampIndex );

				expectedStampIndex = other.m_stampIndex;
				std::atomic_compare_exchange_strong( reinterpret_cast<std::atomic_llong*>( &other ), reinterpret_cast<long long*>( &expectedStampIndex ), 0LL );
			}
		}

		return *this;
	}

	friend bool operator==( const StampIndex& lhs, const StampIndex& rhs )
	{
		return lhs.m_stampIndex == rhs.m_stampIndex;
	}

	friend bool operator!=( const StampIndex& lhs, const StampIndex& rhs )
	{
		return !( lhs == rhs );
	}

private:
	static constexpr unsigned int IndexBits = std::numeric_limits<unsigned int>::digits;
	unsigned long long m_stampIndex = 0;
};

struct IndexedLockFreeLink
{
	StampIndex m_nextStampIndex;
	void* m_data;
	unsigned int m_nextIndex;
};

struct LockFreeLinkPolicy
{
	constexpr static unsigned int MaxLockFreeLink = 1 << 26;

	static IndexedLockFreeLink* IndexToLink( unsigned int index )
	{
		return m_linkAllocator.GetItem( index );
	}

	static unsigned int AllocLockFreeLink( );
	static void DeallocLockFreeLink( unsigned int item );

	static LockFreeFixedMemoryPool<IndexedLockFreeLink, MaxLockFreeLink, 16384>& LinkAllocator( )
	{
		return m_linkAllocator;
	}

private:
	static LockFreeFixedMemoryPool<IndexedLockFreeLink, MaxLockFreeLink, 16384> m_linkAllocator;
};