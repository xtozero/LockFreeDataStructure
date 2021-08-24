#include "gtest/gtest.h"
#include "LockFreeCommon.h"
#include "LockFreeFixedMemoryPool.hpp"
#include "LockFreeList.hpp"
#include "LockFreeQueue.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <queue>

TEST( Coarse_grained_synchronization, List )
{
	CList<int> list;

	ASSERT_TRUE( list.Add( 1 ) );
	ASSERT_TRUE( list.Add( 10 ) );
	ASSERT_TRUE( list.Add( 7 ) );
	ASSERT_TRUE( list.Add( 9 ) );
	ASSERT_TRUE( list.Add( 2 ) );
	ASSERT_TRUE( list.Add( 4 ) );
	ASSERT_TRUE( list.Add( 5 ) );
	ASSERT_TRUE( list.Add( 8 ) );

	ASSERT_FALSE( list.Add( 10 ) );

	ASSERT_TRUE( list.Remove( 10 ) );
	ASSERT_FALSE( list.Remove( 11 ) );

	ASSERT_TRUE( list.Contains( 9 ) );
	ASSERT_FALSE( list.Contains( 10 ) );

	list.Clear( );
}

TEST( Fine_grained_synchronization, List )
{
	FList<int> list;

	ASSERT_TRUE( list.Add( 1 ) );
	ASSERT_TRUE( list.Add( 10 ) );
	ASSERT_TRUE( list.Add( 7 ) );
	ASSERT_TRUE( list.Add( 9 ) );
	ASSERT_TRUE( list.Add( 2 ) );
	ASSERT_TRUE( list.Add( 4 ) );
	ASSERT_TRUE( list.Add( 5 ) );
	ASSERT_TRUE( list.Add( 8 ) );

	ASSERT_FALSE( list.Add( 10 ) );

	ASSERT_TRUE( list.Remove( 10 ) );
	ASSERT_FALSE( list.Remove( 11 ) );

	ASSERT_TRUE( list.Contains( 9 ) );
	ASSERT_FALSE( list.Contains( 10 ) );

	list.Clear( );
}

TEST( Optimistic_synchronization, List )
{
	OList<int> list;

	ASSERT_TRUE( list.Add( 1 ) );
	ASSERT_TRUE( list.Add( 10 ) );
	ASSERT_TRUE( list.Add( 7 ) );
	ASSERT_TRUE( list.Add( 9 ) );
	ASSERT_TRUE( list.Add( 2 ) );
	ASSERT_TRUE( list.Add( 4 ) );
	ASSERT_TRUE( list.Add( 5 ) );
	ASSERT_TRUE( list.Add( 8 ) );

	ASSERT_FALSE( list.Add( 10 ) );

	ASSERT_TRUE( list.Remove( 10 ) );
	ASSERT_FALSE( list.Remove( 11 ) );

	ASSERT_TRUE( list.Contains( 9 ) );
	ASSERT_FALSE( list.Contains( 10 ) );
}

TEST( Lazy_synchronization, List )
{
	LList<int> list;

	ASSERT_TRUE( list.Add( 1 ) );
	ASSERT_TRUE( list.Add( 10 ) );
	ASSERT_TRUE( list.Add( 7 ) );
	ASSERT_TRUE( list.Add( 9 ) );
	ASSERT_TRUE( list.Add( 2 ) );
	ASSERT_TRUE( list.Add( 4 ) );
	ASSERT_TRUE( list.Add( 5 ) );
	ASSERT_TRUE( list.Add( 8 ) );

	ASSERT_FALSE( list.Add( 10 ) );

	ASSERT_TRUE( list.Remove( 10 ) );
	ASSERT_FALSE( list.Remove( 11 ) );

	ASSERT_TRUE( list.Contains( 9 ) );
	ASSERT_FALSE( list.Contains( 10 ) );
}

//TEST( Lock_free_fixed_memory_pool, MemoryPool )
//{
//	LockFreeFixedMemoryPool<int, 100, 5> pool;
//
//	int i = pool.Allocate( );
//	int* num = pool.GetItem( i );
//	*num = 2;
//}

TEST( Stamp_index, index )
{
	static_assert( alignof( StampIndex ) == 8 );
}

//TEST( Lock_Free_Queue, single_thread )
//{
//	LockFreeQueue<int> lfQueue;
//	lfQueue.Push( (int*)1 );
//	lfQueue.Push( (int*)2 );
//	lfQueue.Push( (int*)3 );
//	lfQueue.Push( (int*)4 );
//	lfQueue.Push( (int*)5 );
//	lfQueue.Push( (int*)6 );
//
//	ASSERT_EQ( lfQueue.Pop( ), (int*)1 );
//	ASSERT_EQ( lfQueue.Pop( ), (int*)2 );
//	ASSERT_EQ( lfQueue.Pop( ), (int*)3 );
//	ASSERT_EQ( lfQueue.Pop( ), (int*)4 );
//	ASSERT_EQ( lfQueue.Pop( ), (int*)5 );
//	ASSERT_EQ( lfQueue.Pop( ), (int*)6 );
//	ASSERT_EQ( lfQueue.Pop( ), nullptr );
//}

TEST( Lock_Free_Queue, multi_thread_1 )
{
	constexpr std::size_t LoopCount = 8'000'000;

	LockFreeQueue<int> lfQueue;

	ASSERT_EQ( lfQueue.Pop( ), nullptr );

	auto job = [&lfQueue, LoopCount]( )
	{
		for ( int i = 1; i <= LoopCount; ++i )
		{
			lfQueue.Push( (int*)i );
		}
	};

	std::thread threads[8];

	auto start = std::chrono::system_clock::now( );
	for ( auto& thread : threads )
	{
		thread = std::thread( job );
	}

	for ( auto& thread : threads )
	{
		thread.join( );
	}
	auto end = std::chrono::system_clock::now( );

	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count( ) << std::endl;

	std::size_t total = 0;
	while ( true )
	{
		if ( lfQueue.Pop( ) != nullptr )
		{
			++total;
		}
		else
		{
			break;
		}
	}

	ASSERT_EQ( total, LoopCount * 8 );
}

TEST( Lock_Free_Queue, multi_thread_2 )
{
	constexpr std::size_t LoopCount = 8'000'000;

	LockFreeQueue<int> lfQueue;

	ASSERT_EQ( lfQueue.Pop( ), nullptr );

	for ( int i = 1; i <= LoopCount * 8; ++i )
	{
		lfQueue.Push( (int*)i );
	}

	auto job = [&lfQueue, LoopCount]( )
	{
		for ( int i = 1; i <= LoopCount; ++i )
		{
			lfQueue.Pop( );
		}
	};

	std::thread threads[8];

	for ( auto& thread : threads )
	{
		thread = std::thread( job );
	}

	for ( auto& thread : threads )
	{
		thread.join( );
	}

	std::size_t total = 0;
	while ( true )
	{
		if ( lfQueue.Pop( ) != nullptr )
		{
			++total;
		}
		else
		{
			break;
		}
	}

	ASSERT_EQ( total, 0 );
}

TEST( Lock_Free_Queue, multi_thread_3 )
{
	constexpr std::size_t LoopCount = 8'000'000;
	std::atomic<std::size_t> count = 0;

	LockFreeQueue<int> lfQueue;

	ASSERT_EQ( lfQueue.Pop( ), nullptr );

	auto job = [&lfQueue, &count, LoopCount]( )
	{
		for ( int i = 1; i <= LoopCount; ++i )
		{
			if ( rand( ) % 2 == 0 )
			{
				if ( lfQueue.Pop( ) != nullptr )
				{
					--count;
				}
			}
			else
			{
				lfQueue.Push( (int*)i );
				++count;
			}
		}
	};

	std::thread threads[8];

	for ( auto& thread : threads )
	{
		thread = std::thread( job );
	}

	for ( auto& thread : threads )
	{
		thread.join( );
	}

	std::size_t total = 0;
	while ( true )
	{
		if ( lfQueue.Pop( ) != nullptr )
		{
			++total;
		}
		else
		{
			break;
		}
	}

	ASSERT_EQ( total, count );
}

template <typename T>
class LockQueue
{
public:
	void Push( T data )
	{
		std::lock_guard lk( m_lock );
		m_queue.push( data );
	}

	std::optional<T> Pop( )
	{
		std::lock_guard lk( m_lock );
		if ( m_queue.empty( ) == false )
		{
			T data = m_queue.front( );
			m_queue.pop( );
			return data;
		}

		return {};
	}

private:
	std::mutex m_lock;
	std::queue<T> m_queue;
};

TEST( Lock_Queue, multi_thread_1 )
{
	constexpr std::size_t LoopCount = 8'000'000;

	LockQueue<int> lfQueue;

	ASSERT_EQ( lfQueue.Pop( ), std::nullopt );

	auto job = [&lfQueue, LoopCount]( )
	{
		for ( int i = 1; i <= LoopCount; ++i )
		{
			lfQueue.Push( i );
		}
	};

	std::thread threads[8];

	auto start = std::chrono::system_clock::now( );
	for ( auto& thread : threads )
	{
		thread = std::thread( job );
	}

	for ( auto& thread : threads )
	{
		thread.join( );
	}
	auto end = std::chrono::system_clock::now( );

	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count( ) << std::endl;

	std::size_t total = 0;
	while ( true )
	{
		if ( lfQueue.Pop( ) )
		{
			++total;
		}
		else
		{
			break;
		}
	}

	ASSERT_EQ( total, LoopCount * 8 );
}

TEST( Lock_Queue, multi_thread_2 )
{
	constexpr std::size_t LoopCount = 8'000'000;

	LockQueue<int> lQueue;

	ASSERT_EQ( lQueue.Pop( ), std::nullopt );

	for ( int i = 1; i <= LoopCount * 8; ++i )
	{
		lQueue.Push( i );
	}

	auto job = [&lQueue, LoopCount]( )
	{
		for ( int i = 1; i <= LoopCount; ++i )
		{
			lQueue.Pop( );
		}
	};

	std::thread threads[8];

	for ( auto& thread : threads )
	{
		thread = std::thread( job );
	}

	for ( auto& thread : threads )
	{
		thread.join( );
	}

	std::size_t total = 0;
	while ( true )
	{
		if ( lQueue.Pop( ) )
		{
			++total;
		}
		else
		{
			break;
		}
	}

	ASSERT_EQ( total, 0 );
}

TEST( Lock_Queue, multi_thread_3 )
{
	constexpr std::size_t LoopCount = 8'000'000;
	std::atomic<std::size_t> count = 0;

	LockQueue<int> lQueue;

	ASSERT_EQ( lQueue.Pop( ), std::nullopt );

	auto job = [&lQueue, &count, LoopCount]( )
	{
		for ( int i = 1; i <= LoopCount; ++i )
		{
			if ( rand( ) % 2 == 0 )
			{
				if ( lQueue.Pop( ) )
				{
					--count;
				}
			}
			else
			{
				lQueue.Push( i );
				++count;
			}
		}
	};

	std::thread threads[8];

	for ( auto& thread : threads )
	{
		thread = std::thread( job );
	}

	for ( auto& thread : threads )
	{
		thread.join( );
	}

	std::size_t total = 0;
	while ( true )
	{
		if ( lQueue.Pop( ) )
		{
			++total;
		}
		else
		{
			break;
		}
	}

	ASSERT_EQ( total, count );
}