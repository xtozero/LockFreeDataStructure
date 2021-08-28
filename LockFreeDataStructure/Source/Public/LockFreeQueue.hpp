#pragma once

#include "LockFreeCommon.h"
#include "SizedTypes.h"

#include <chrono>

template <typename T, uint32 PaddingForCacheLine = LockFreeCacheLineBytes>
class LockFreeQueue
{
public:
	void Push( T* data )
	{
		uint32 newData = LockFreeLinkPolicy::AllocLockFreeLink( );
		LockFreeLinkPolicy::IndexToLink( newData )->m_data = data;

		while ( true )
		{
			StampIndex last = m_tail;
			IndexedLockFreeLink* lastLink = LockFreeLinkPolicy::IndexToLink( last.GetIndex( ) );
			StampIndex next = lastLink->m_nextStampIndex;
			StampIndex lastForTest = m_tail;
			if ( last == lastForTest )
			{
				uint32 nextIndex = next.GetIndex( );
				if ( next.GetIndex( ) == 0 )
				{
					StampIndex newNext;
					newNext.SetStamp( next.GetStamp( ) + 1 );
					newNext.SetIndex( newData );

					if ( lastLink->m_nextStampIndex.CompareExchange( next, newNext ) )
					{
						StampIndex newTail;
						newTail.SetStamp( last.GetStamp( ) + 1 );
						newTail.SetIndex( newData );

						m_tail.CompareExchange( last, newTail );
						return;
					}
				}
				else
				{
					StampIndex newTail;
					newTail.SetStamp( last.GetStamp( ) + 1 );
					newTail.SetIndex( next.GetIndex( ) );

					m_tail.CompareExchange( last, newTail );
				}
			}
		}
	}

	T* Pop( )
	{
		while ( true )
		{
			StampIndex first = m_head;
			StampIndex last = m_tail;
			IndexedLockFreeLink* firstLink = LockFreeLinkPolicy::IndexToLink( first.GetIndex( ) );
			StampIndex next = firstLink->m_nextStampIndex;
			StampIndex firstForTest = m_head;
			if ( first == firstForTest )
			{
				if ( first.GetIndex( ) == last.GetIndex( ) )
				{
					if ( next.GetIndex() == 0 )
					{
						return nullptr;
					}

					StampIndex newTail;
					newTail.SetStamp( last.GetStamp( ) + 1 );
					newTail.SetIndex( next.GetIndex( ) );

					m_tail.CompareExchange( last, newTail );
				}
				else
				{
					IndexedLockFreeLink* nextLink = LockFreeLinkPolicy::IndexToLink( next.GetIndex( ) );
					auto data = reinterpret_cast<T*>( nextLink->m_data );

					StampIndex newHead;
					newHead.SetStamp( first.GetStamp( ) + 1 );
					newHead.SetIndex( next.GetIndex( ) );

					if ( m_head.CompareExchange( first, newHead ) )
					{
						LockFreeLinkPolicy::DeallocLockFreeLink( first.GetIndex( ) );
						return data;
					}
				}
			}
		}
	}

	LockFreeQueue( )
	{
		uint32 sentinel = LockFreeLinkPolicy::AllocLockFreeLink( );
		m_head.SetIndex( sentinel );
		m_tail.SetIndex( sentinel );
	}

	~LockFreeQueue( )
	{
		while ( Pop() != nullptr ) { }
	}

private:
	uint8 padding1[PaddingForCacheLine];
	StampIndex m_head;
	uint8 padding2[PaddingForCacheLine];
	StampIndex m_tail;
	uint8 padding3[PaddingForCacheLine];
};