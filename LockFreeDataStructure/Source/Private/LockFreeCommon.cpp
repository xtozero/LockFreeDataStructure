#include "LockFreeCommon.h"

#include <limits>
#include <optional>
#include <vector>

LockFreeFixedMemoryPool<IndexedLockFreeLink, LockFreeLinkPolicy::MaxLockFreeLink, 16384> LockFreeLinkPolicy::m_linkAllocator;

class LockFreeLinkFreeList
{
public:
	void Push( unsigned int index )
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

	unsigned int Pop( )
	{
		unsigned int index = 0;

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
	unsigned char padding[64];
	StampIndex m_head;
	unsigned char padding1[64];
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
					unsigned int baseIndex = LockFreeLinkPolicy::LinkAllocator( ).Allocate( NumPerBundle );

					for ( unsigned int i = 0; i < NumPerBundle; ++i )
					{
						IndexedLockFreeLink* link = LockFreeLinkPolicy::IndexToLink( baseIndex + i );
						link->m_nextIndex = 0;
						link->m_data = (void*)( ( unsigned long long )cache.m_partialBundle );
						cache.m_partialBundle = baseIndex + i;
					}
				}
			}
			cache.m_numPartial = NumPerBundle;
		}

		unsigned int index = cache.m_partialBundle;
		IndexedLockFreeLink* link = LockFreeLinkPolicy::IndexToLink( index );
		cache.m_partialBundle = (unsigned int)( ( unsigned long long )( link->m_data ) );
		--cache.m_numPartial;
		link->m_data = nullptr;

		return index;
	}

	void Dealloc( unsigned int index )
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
		link->m_data = (void*)( ( unsigned long long )cache.m_partialBundle );

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
		unsigned int m_fullBundle = 0;
		unsigned int m_partialBundle = 0;
		unsigned int m_numPartial = 0;
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

	static constexpr int NumPerBundle = 64;
	static constexpr std::size_t UninitializedSlot = (std::numeric_limits<std::size_t>::max)();

	static thread_local std::size_t m_tlsSlot;
	static thread_local std::vector<ThreadLocalCache> m_tlsCache;

	LockFreeLinkFreeList m_freelistBundles;
};

thread_local std::vector<LockFreeLinkAllocator::ThreadLocalCache> LockFreeLinkAllocator::m_tlsCache;
thread_local std::size_t LockFreeLinkAllocator::m_tlsSlot = UninitializedSlot;

static LockFreeLinkAllocator g_lockFreeLinkAllocator;

unsigned int LockFreeLinkPolicy::AllocLockFreeLink( )
{
	return g_lockFreeLinkAllocator.Alloc( );
}

void LockFreeLinkPolicy::DeallocLockFreeLink( unsigned int item )
{
	g_lockFreeLinkAllocator.Dealloc( item );
}