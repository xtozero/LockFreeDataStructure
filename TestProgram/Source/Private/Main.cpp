#include "LockFreeCommon.h"
#include "LockFreeFixedMemoryPool.hpp"
#include "LockFreeList.hpp"
#include "LockFreeQueue.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <queue>

using namespace std::chrono;

HANDLE hMutex;

template <typename T>
class LockQueue
{
public:
	void Push( T data )
	{
		// WaitForSingleObject( hMutex, INFINITE );
		std::lock_guard lk( m_lock );
		m_queue.push( data );
		// ReleaseMutex( hMutex );
	}

	std::optional<T> Pop( )
	{
		// WaitForSingleObject( hMutex, INFINITE );
		std::lock_guard lk( m_lock );
		if ( m_queue.empty( ) == false )
		{
			T data = m_queue.front( );
			m_queue.pop( );
			// ReleaseMutex( hMutex );
			return data;
		}

		// ReleaseMutex( hMutex );
		return {};
	}

private:
	std::mutex m_lock;
	std::queue<T> m_queue;
};

using namespace std;

class NODE {
public:
	int key;
	NODE *next;

	NODE( ) { next = nullptr; }
	NODE( int key_value ) {
		next = nullptr;
		key = key_value;
	}
	~NODE( ) {}
};

class LFQUEUE {
	NODE * volatile head;
	NODE * volatile tail;
public:
	LFQUEUE( )
	{
		head = tail = new NODE( 0 );
	}
	~LFQUEUE( ) {}

	void Init( )
	{
		NODE *ptr;
		while ( head->next != nullptr ) {
			ptr = head->next;
			head->next = head->next->next;
			delete ptr;
		}
		tail = head;
	}
	bool CAS( NODE * volatile * addr, NODE *old_node, NODE *new_node )
	{
		return atomic_compare_exchange_strong( reinterpret_cast<volatile atomic_int *>( addr ),
			reinterpret_cast<int *>( &old_node ),
			reinterpret_cast<int>( new_node ) );
	}
	void Enq( int key )
	{
		NODE *e = new NODE( key );
		while ( true ) {
			NODE *last = tail;
			NODE *next = last->next;
			if ( last != tail ) continue;
			if ( next != nullptr ) {
				CAS( &tail, last, next );
				continue;
			}
			if ( false == CAS( &last->next, nullptr, e ) ) continue;
			CAS( &tail, last, e );
			return;
		}
	}
	int Deq( )
	{
		while ( true ) {
			NODE *first = head;
			NODE *next = first->next;
			NODE *last = tail;
			NODE *lastnext = last->next;
			if ( first != head ) continue;
			if ( last == first ) {
				if ( lastnext == nullptr ) {
					// cout << "EMPTY!!!\n";
					this_thread::sleep_for( 1ms );
					return -1;
				}
				else
				{
					CAS( &tail, last, lastnext );
					continue;
				}
			}
			if ( nullptr == next ) continue;
			int result = next->key;
			if ( false == CAS( &head, first, next ) ) continue;
			first->next = nullptr;
			// delete first;
			return result;
		}
	}

	void display20( )
	{
		int c = 20;
		NODE *p = head->next;
		while ( p != nullptr )
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if ( c == 0 ) break;
		}
		cout << endl;
	}
};

// stamped pointer 
class SPTR {
public:
	NODE* volatile ptr;
	volatile int stamp;

	SPTR( ) {
		ptr = nullptr;
		stamp = 0;
	}

	SPTR( NODE* p, int v ) {
		ptr = p;
		stamp = v;
	}
};

class SLFQUEUE {
	SPTR head;
	SPTR tail;
public:
	SLFQUEUE( ) {
		head.ptr = tail.ptr = new NODE( 0 );
	}

	~SLFQUEUE( ) {}

	void Init( ) {
		NODE* ptr;
		while ( head.ptr->next != nullptr ) {
			ptr = head.ptr->next;
			head.ptr->next = head.ptr->next->next;
			delete ptr;
		}

		tail = head;
	}

	bool CAS( NODE* volatile* addr, NODE* old_node, NODE* new_node ) {
		return atomic_compare_exchange_strong( reinterpret_cast<volatile atomic_int*>( addr ),
			reinterpret_cast<int*>( &old_node ),
			reinterpret_cast<int>( new_node ) );
	}

	bool STAMPCAS( SPTR* addr, NODE* old_node, int old_stamp, NODE* new_node ) {
		SPTR old_ptr{ old_node, old_stamp };
		SPTR new_ptr{ new_node, old_stamp + 1 };
		return atomic_compare_exchange_strong( reinterpret_cast<atomic_llong*>( addr ),
			reinterpret_cast<long long*>( &old_ptr ),
			*( reinterpret_cast<long long*>( &new_ptr ) ) );
	}

	void Enq( int key ) {
		NODE* e = new NODE( key );
		while ( true ) {
			SPTR last = tail;
			NODE* next = last.ptr->next;
			if ( last.ptr != tail.ptr ) continue;
			if ( nullptr == next ) {
				if ( CAS( &( last.ptr->next ), nullptr, e ) ) {
					STAMPCAS( &tail, last.ptr, last.stamp, e );
					return;
				}
			}
			else STAMPCAS( &tail, last.ptr, last.stamp, next );
		}
	}

	int Deq( ) {
		while ( true ) {
			SPTR first = head;
			NODE* next = first.ptr->next;
			SPTR last = tail;
			NODE* lastnext = last.ptr->next;

			if ( first.ptr != head.ptr ) continue;
			if ( last.ptr == first.ptr ) {
				if ( lastnext == nullptr ) {
					cout << "EMPTY!!! ";
					this_thread::sleep_for( 1ms );
					return -1;
				}
				else {
					STAMPCAS( &tail, last.ptr, last.stamp, lastnext );
					continue;
				}
			}

			if ( nullptr == next ) continue;
			int result = next->key;
			if ( false == STAMPCAS( &head, first.ptr, first.stamp, next ) ) continue;
			//first.ptr->next = nullptr;
			delete first.ptr;
			return result;
		}
	}

	void display20( ) {
		int c = 20;
		NODE* p = head.ptr->next;
		while ( p != nullptr ) {
			cout << p->key;
			p = p->next;
			c--;
			if ( c == 0 ) break;
			cout << ", ";
		}
		cout << endl;
	}
};

int main()
{
	hMutex = CreateMutex( nullptr, false, nullptr );

	constexpr std::size_t ThreadCount = 8;
	constexpr std::size_t LoopCount = 64'000'000;

	//{
	//	LockQueue<int> lQueue;

	//	auto job1 = [&lQueue, LoopCount, ThreadCount]( )
	//	{
	//		for ( int i = 1; i <= LoopCount / ThreadCount; ++i )
	//		{
	//			if ( rand( ) % 2 == 0 || i < ( 10000 / ThreadCount ) )
	//			{
	//				lQueue.Push( i );
	//			}
	//			else
	//			{
	//				lQueue.Pop( );
	//			}
	//		}
	//	};

	//	std::thread threads[ThreadCount];

	//	auto start = std::chrono::system_clock::now( );
	//	for ( auto& thread : threads )
	//	{
	//		thread = std::thread( job1 );
	//	}

	//	for ( auto& thread : threads )
	//	{
	//		thread.join( );
	//	}
	//	auto end = std::chrono::system_clock::now( );

	//	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count( ) << std::endl;
	//}

	{
		LockFreeQueue<int> lfQueue;

		auto job = [&lfQueue, LoopCount, ThreadCount]( )
		{
			for ( int i = 1; i <= LoopCount / ThreadCount; ++i )
			{
				if ( rand( ) % 2 == 0 || i < ( 10000 / ThreadCount ) )
				{
					lfQueue.Push( (int*)i );
				}
				else
				{
					lfQueue.Pop( );
				}
			}
		};

		std::thread threads[ThreadCount];

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
	}

	/*{
		LFQUEUE lfQueue;

		auto job1 = [&lfQueue, LoopCount, ThreadCount]( )
		{
			for ( int i = 1; i <= LoopCount / ThreadCount; ++i )
			{
				if ( rand( ) % 2 == 0 || i < ( 10000 / ThreadCount ) )
				{
					lfQueue.Enq( i );
				}
				else
				{
					lfQueue.Deq( );
				}
			}
		};

		std::thread threads[ThreadCount];

		auto start = std::chrono::system_clock::now( );
		for ( auto& thread : threads )
		{
			thread = std::thread( job1 );
		}

		for ( auto& thread : threads )
		{
			thread.join( );
		}
		auto end = std::chrono::system_clock::now( );

		std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count( ) << std::endl;
	}
	
	{
		SLFQUEUE slfQueue;

		auto job1 = [&slfQueue, LoopCount, ThreadCount]( )
		{
			for ( int i = 1; i <= LoopCount / ThreadCount; ++i )
			{
				if ( rand( ) % 2 == 0 || i < ( 10000 / ThreadCount ) )
				{
					slfQueue.Enq( i );
				}
				else
				{
					slfQueue.Deq( );
				}
			}
		};

		std::thread threads[ThreadCount];

		auto start = std::chrono::system_clock::now( );
		for ( auto& thread : threads )
		{
			thread = std::thread( job1 );
		}

		for ( auto& thread : threads )
		{
			thread.join( );
		}
		auto end = std::chrono::system_clock::now( );

		std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count( ) << std::endl;
	}*/

	return 0;
}

/*
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
*/