#pragma once

#include "LockFreeCommon.h"

#include <chrono>

template <typename T>
class LockFreeQueue
{
public:
	void Push( T* data )
	{
		unsigned int newData = LockFreeLinkPolicy::AllocLockFreeLink( );
		LockFreeLinkPolicy::IndexToLink( newData )->m_data = data;

		while ( true )
		{
			StampIndex last = m_tail;
			IndexedLockFreeLink* lastLink = LockFreeLinkPolicy::IndexToLink( last.GetIndex( ) );
			StampIndex next = lastLink->m_nextStampIndex;
			if ( last == m_tail )
			{
				unsigned int nextIndex = next.GetIndex( );
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
		using namespace std::chrono;

		while ( true )
		{
			StampIndex first = m_head;
			StampIndex last = m_tail;
			IndexedLockFreeLink* firstLink = LockFreeLinkPolicy::IndexToLink( first.GetIndex( ) );
			StampIndex next = firstLink->m_nextStampIndex;
			if ( first == m_head )
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
		unsigned int sentinel = LockFreeLinkPolicy::AllocLockFreeLink( );
		m_head.SetIndex( sentinel );
		m_tail.SetIndex( sentinel );
	}

	~LockFreeQueue( )
	{
		while ( Pop() != nullptr ) { }
	}

private:
	unsigned char padding[64];
	StampIndex m_head;
	unsigned char padding1[64];
	StampIndex m_tail;
	unsigned char padding2[64];
};