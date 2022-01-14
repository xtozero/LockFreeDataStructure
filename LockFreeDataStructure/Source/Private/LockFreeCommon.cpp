#include "LockFreeCommon.h"

#include <limits>
#include <optional>
#include <vector>

LockFreeFixedMemoryPool<IndexedLockFreeLink, LockFreeLinkPolicy::MaxLockFreeLink, 16384, LockFreeCacheLineBytes> LockFreeLinkPolicy::m_linkAllocator;

template<uint32 PaddingForCacheLine = LockFreeCacheLineBytes>
class LockFreeLinkFreeList
{
public:
	void Push( uint32 index )
	{
		while ( true )
		{
			StampIndex curHead = m_head;
			StampIndex newHead;
			newHead.Set( index, curHead.GetStamp( ) + 1 );
			LockFreeLinkPolicy::IndexToLink( index )->m_nextIndex = curHead.GetIndex( );
			if ( m_head.CompareExchange( curHead, newHead ) )
			{
				break;
			}
		}
	}

	uint32 Pop( )
	{
		uint32 index = 0;

		while ( true )
		{
			StampIndex curHead = m_head;
			index = curHead.GetIndex( );
			if ( index == 0 )
			{
				break;
			}

			IndexedLockFreeLink* link = LockFreeLinkPolicy::IndexToLink( index );
			StampIndex newHead;
			newHead.Set( link->m_nextIndex, curHead.GetStamp( ) + 1 );
			if ( m_head.CompareExchange( curHead, newHead ) )
			{
				link->m_nextIndex = 0;
				break;
			}
		}

		return index;
	}

private:
	uint8 padding1[PaddingForCacheLine];
	StampIndex m_head;
	uint8 padding2[PaddingForCacheLine];
};

class LockFreeLinkAllocator
{
public:
	unsigned int Alloc( )
	{
		ThreadLocalCache& cache = GetLocalCache( );

		if ( cache.m_partialBundle == 0 )
		{
			if ( cache.m_fullBundle != 0 )
			{
				cache.m_partialBundle = cache.m_fullBundle;
				cache.m_fullBundle = 0;
			}
			else
			{
				cache.m_partialBundle = m_freelistBundles.Pop( );
				if ( cache.m_partialBundle == 0 )
				{
					uint32 baseIndex = LockFreeLinkPolicy::LinkAllocator( ).Allocate( NumPerBundle );

					for ( uint32 i = 0; i < NumPerBundle; ++i )
					{
						IndexedLockFreeLink* link = LockFreeLinkPolicy::IndexToLink( baseIndex + i );
						link->m_nextIndex = 0;
						link->m_data = (void*)( (uptrint)cache.m_partialBundle );
						cache.m_partialBundle = baseIndex + i;
					}
				}
			}
			cache.m_numPartial = NumPerBundle;
		}

		uint32 index = cache.m_partialBundle;
		IndexedLockFreeLink* link = LockFreeLinkPolicy::IndexToLink( index );
		cache.m_partialBundle = (uint32)( (uptrint)( link->m_data ) );
		--cache.m_numPartial;
		link->m_data = nullptr;

		return index;
	}

	void Dealloc( uint32 index )
	{
		ThreadLocalCache& cache = GetLocalCache( );
		if ( cache.m_numPartial >= NumPerBundle )
		{
			if ( cache.m_fullBundle != 0 )
			{
				m_freelistBundles.Push( cache.m_fullBundle );
			}

			cache.m_fullBundle = cache.m_partialBundle;
			cache.m_partialBundle = 0;
			cache.m_numPartial = 0;
		}

		IndexedLockFreeLink* link = LockFreeLinkPolicy::IndexToLink( index );
		link->m_nextStampIndex.SetIndex( 0 );
		link->m_nextIndex = 0;
		link->m_data = (void*)( ( uint64 )cache.m_partialBundle );

		cache.m_partialBundle = index;
		++cache.m_numPartial;
	}

	LockFreeLinkAllocator( ) = default;

	~LockFreeLinkAllocator( )
	{
		m_tlsSlot = 0;
	}

	LockFreeLinkAllocator( const LockFreeLinkAllocator& ) = delete;
	LockFreeLinkAllocator( LockFreeLinkAllocator&& ) = delete;
	LockFreeLinkAllocator& operator=( const LockFreeLinkAllocator& ) = delete;
	LockFreeLinkAllocator& operator=( LockFreeLinkAllocator&& ) = delete;

private:
	struct ThreadLocalCache
	{
		uint32 m_fullBundle = 0;
		uint32 m_partialBundle = 0;
		uint32 m_numPartial = 0;
	};

	ThreadLocalCache& GetLocalCache( )
	{
		if ( m_tlsSlot == UninitializedSlot )
		{
			m_tlsSlot = m_tlsCache.size( );
			m_tlsCache.emplace_back( );
		}

		return m_tlsCache[m_tlsSlot];
	}

	static constexpr int32 NumPerBundle = 64;
	static constexpr std::size_t UninitializedSlot = (std::numeric_limits<std::size_t>::max)();

	std::size_t m_tlsSlot = UninitializedSlot;
	static thread_local std::vector<ThreadLocalCache> m_tlsCache;

	LockFreeLinkFreeList<> m_freelistBundles;
};

thread_local std::vector<LockFreeLinkAllocator::ThreadLocalCache> LockFreeLinkAllocator::m_tlsCache;

static LockFreeLinkAllocator g_lockFreeLinkAllocator;

uint32 LockFreeLinkPolicy::AllocLockFreeLink( )
{
	return g_lockFreeLinkAllocator.Alloc( );
}

void LockFreeLinkPolicy::DeallocLockFreeLink( uint32 item )
{
	g_lockFreeLinkAllocator.Dealloc( item );
}