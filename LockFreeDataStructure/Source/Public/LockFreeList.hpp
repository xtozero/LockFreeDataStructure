#pragma once

#include <mutex>

// coarse-grained synchronization

template <typename T>
class CList
{
public:
	bool Add( T data )
	{
		Node* prev = &m_head;
		m_lock.lock( );
		Node* curr = prev->m_next;

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				prev = curr;
				curr = curr->m_next;
			}
			else
			{
				break;
			}
		}

		if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
		{
			m_lock.unlock( );
			return false;
		}
		else
		{
			auto newNode = new Node( data, curr );
			prev->m_next = newNode;
			m_lock.unlock( );
			return true;
		}
	}

	bool Remove( T data )
	{
		Node* prev = &m_head;
		m_lock.lock( );
		Node* curr = prev->m_next;

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				prev = curr;
				curr = curr->m_next;
			}
			else
			{
				break;
			}
		}

		if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
		{
			prev->m_next = curr->m_next;
			delete curr;
			m_lock.unlock( );
			return true;
		}
		else
		{
			m_lock.unlock( );
			return false;
		}
	}

	bool Contains( T data )
	{
		Node* prev = &m_head;
		m_lock.lock( );
		Node* curr = prev->m_next;

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				prev = curr;
				curr = curr->m_next;
			}
			else
			{
				break;
			}
		}

		if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
		{
			m_lock.unlock( );
			return true;
		}
		else
		{
			m_lock.unlock( );
			return false;
		}
	}

	void Clear( )
	{
		std::lock_guard lock_guard( m_lock );

		while ( m_head.m_next != &m_tail )
		{
			Node* ptr = m_head.m_next;
			m_head.m_next = m_head.m_next->m_next;
			delete ptr;
		}
	}
private:
	class Node
	{
	public:
		explicit Node( T data ) noexcept :
			m_data( data )
		{ }

		Node( T data, Node* next ) noexcept :
			m_data( data ), m_next( next )
		{ }

		T m_data;
		Node* m_next = nullptr;
	};

	Node m_head = Node( T( ), &m_tail );
	Node m_tail = Node( T( ) );
	std::mutex m_lock;
};

// fine-grained synchronization
template <typename T>
class FList
{
public:
	bool Add( T data )
	{
		m_head.Lock( );
		Node* prev = &m_head;
		Node* curr = prev->m_next;
		curr->Lock( );

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				prev->Unlock( );
				prev = curr;
				curr = curr->m_next;
				curr->Lock( );
			}
			else
			{
				break;
			}
		}

		if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
		{
			curr->Unlock( );
			prev->Unlock( );
			return false;
		}
		else
		{
			auto newNode = new Node( data, curr );
			prev->m_next = newNode;
			curr->Unlock( );
			prev->Unlock( );
			return true;
		}
	}

	bool Remove( T data )
	{
		m_head.Lock( );
		Node* prev = &m_head;
		Node* curr = prev->m_next;
		curr->Lock( );

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				prev->Unlock( );
				prev = curr;
				curr = curr->m_next;
				curr->Lock( );
			}
			else
			{
				break;
			}
		}

		if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
		{
			prev->m_next = curr->m_next;
			curr->Unlock( );
			prev->Unlock( );
			delete curr;
			return true;
		}
		else
		{
			curr->Unlock( );
			prev->Unlock( );
			return false;
		}
	}

	bool Contains( T data )
	{
		m_head.Lock( );
		Node* prev = &m_head;
		Node* curr = prev->m_next;
		curr->Lock( );

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				prev->Unlock( );
				prev = curr;
				curr = curr->m_next;
				curr->Lock( );
			}
			else
			{
				break;
			}
		}

		if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
		{
			curr->Unlock( );
			prev->Unlock( );
			return true;
		}
		else
		{
			curr->Unlock( );
			prev->Unlock( );
			return false;
		}
	}

	void Clear( )
	{
		m_head.Lock( );

		while ( m_head.m_next != &m_tail )
		{
			Node* ptr = m_head.m_next;
			ptr->Lock( );
			m_head.m_next = m_head.m_next->m_next;
			ptr->Unlock( );
			delete ptr;
		}

		m_head.Unlock( );
	}
private:
	class Node
	{
	public:
		explicit Node( T data ) noexcept :
			m_data( data )
		{ }

		Node( T data, Node* next ) noexcept :
			m_data( data ), m_next( next )
		{ }

		void Lock( )
		{
			m_lock.lock( );
		}

		void Unlock( )
		{
			m_lock.unlock( );
		}

		T m_data;
		Node* m_next = nullptr;
		std::mutex m_lock;
	};

	Node m_head = Node( T( ), &m_tail );
	Node m_tail = Node( T( ) );
};

// Optimistic synchronization
template <typename T>
class OList
{
public:
	bool Add( T data )
	{
		while ( true )
		{
			Node* prev = &m_head;
			Node* curr = prev->m_next;

			while ( curr != &m_tail )
			{
				if ( curr->m_data < data )
				{
					prev = curr;
					curr = curr->m_next;
				}
				else
				{
					break;
				}
			}

			prev->Lock( );
			curr->Lock( );

			if ( Validate( prev, curr ) )
			{
				if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
				{
					prev->Unlock( );
					curr->Unlock( );
					return false;
				}
				else
				{
					auto newNode = new Node( data, curr );
					prev->m_next = newNode;
					prev->Unlock( );
					curr->Unlock( );
					return true;
				}
			}
			else
			{
				prev->Unlock( );
				curr->Unlock( );
			}
		}
	}

	bool Remove( T data )
	{
		while ( true )
		{
			Node* prev = &m_head;
			Node* curr = prev->m_next;

			while ( curr != &m_tail )
			{
				if ( curr->m_data < data )
				{
					prev = curr;
					curr = curr->m_next;
				}
				else
				{
					break;
				}
			}

			prev->Lock( );
			curr->Lock( );

			if ( Validate( prev, curr ) )
			{
				if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
				{
					prev->m_next = curr->m_next;
					prev->Unlock( );
					curr->Unlock( );

					m_freeListHead.Lock( );
					Node* flCurr = m_freeListHead.m_next;
					flCurr->Lock( );

					m_freeListHead.m_next = curr;
					curr->m_next = flCurr;

					m_freeListHead.Unlock( );
					flCurr->Unlock( );

					return true;
				}
				else
				{
					prev->Unlock( );
					curr->Unlock( );
					return false;
				}
			}
			else
			{
				prev->Unlock( );
				curr->Unlock( );
			}
		}
	}

	bool Contains( T data )
	{
		while ( true )
		{
			Node* prev = &m_head;
			Node* curr = prev->m_next;

			while ( curr != &m_tail )
			{
				if ( curr->m_data < data )
				{
					prev = curr;
					curr = curr->m_next;
				}
				else
				{
					break;
				}
			}

			prev->Lock( );
			curr->Lock( );

			if ( Validate( prev, curr ) )
			{
				if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
				{
					prev->Unlock( );
					curr->Unlock( );
					return true;
				}
				else
				{
					prev->Unlock( );
					curr->Unlock( );
					return false;
				}
			}
			else
			{
				prev->Unlock( );
				curr->Unlock( );
			}
		}
	}

private:
	class Node
	{
	public:
		explicit Node( T data ) noexcept :
			m_data( data )
		{ }

		Node( T data, Node* next ) noexcept :
			m_data( data ), m_next( next )
		{ }

		void Lock( )
		{
			m_lock.lock( );
		}

		void Unlock( )
		{
			m_lock.unlock( );
		}

		T m_data;
		Node* m_next = nullptr;
		std::mutex m_lock;
	};

	bool Validate( Node* prev, Node* curr )
	{
		Node* node = &m_head;
		
		while ( node != &m_tail )
		{
			if ( node->m_data <= prev->m_data )
			{
				if ( node == prev )
				{
					return prev->m_next == curr;
				}
				node = node->m_next;
			}
			else
			{
				break;
			}
		}

		return false;
	}

	Node m_head = Node( T( ), &m_tail );
	Node m_tail = Node( T( ) );

	Node m_freeListHead = Node( T( ), &m_freeListTail );
	Node m_freeListTail = Node( T( ) );
};

// Lazy synchronization
template <typename T>
class LList
{
public:
	bool Add( T data )
	{
		while ( true )
		{
			Node* prev = &m_head;
			Node* curr = prev->m_next;

			while ( curr != &m_tail )
			{
				if ( curr->m_data < data )
				{
					prev = curr;
					curr = curr->m_next;
				}
				else
				{
					break;
				}
			}

			prev->Lock( );
			curr->Lock( );

			if ( Validate( prev, curr ) )
			{
				if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
				{
					prev->Unlock( );
					curr->Unlock( );
					return false;
				}
				else
				{
					auto newNode = new Node( data, curr );
					prev->m_next = newNode;
					prev->Unlock( );
					curr->Unlock( );
					return true;
				}
			}
			else
			{
				prev->Unlock( );
				curr->Unlock( );
			}
		}
	}

	bool Remove( T data )
	{
		while ( true )
		{
			Node* prev = &m_head;
			Node* curr = prev->m_next;

			while ( curr != &m_tail )
			{
				if ( curr->m_data < data )
				{
					prev = curr;
					curr = curr->m_next;
				}
				else
				{
					break;
				}
			}

			prev->Lock( );
			curr->Lock( );

			if ( Validate( prev, curr ) )
			{
				if ( ( curr != &m_tail ) && ( data == curr->m_data ) )
				{
					curr->m_marked = true;
					prev->m_next = curr->m_next;
					prev->Unlock( );
					curr->Unlock( );

					m_freeListHead.Lock( );
					Node* flCurr = m_freeListHead.m_next;
					flCurr->Lock( );

					m_freeListHead.m_next = curr;
					curr->m_next = flCurr;

					m_freeListHead.Unlock( );
					flCurr->Unlock( );

					return true;
				}
				else
				{
					prev->Unlock( );
					curr->Unlock( );
					return false;
				}
			}
			else
			{
				prev->Unlock( );
				curr->Unlock( );
			}
		}
	}

	bool Contains( T data )
	{
		Node* curr = &m_head;

		while ( curr != &m_tail )
		{
			if ( curr->m_data < data )
			{
				curr = curr->m_next;
			}
			else
			{
				break;
			}
		}

		return ( curr->m_data == data ) && ( curr->m_marked == false );
	}

private:
	class Node
	{
	public:
		explicit Node( T data ) noexcept :
			m_data( data )
		{ }

		Node( T data, Node* next ) noexcept :
			m_data( data ), m_next( next )
		{ }

		void Lock( )
		{
			m_lock.lock( );
		}

		void Unlock( )
		{
			m_lock.unlock( );
		}

		T m_data;
		Node* m_next = nullptr;
		std::mutex m_lock;
		bool m_marked = false;
	};

	bool Validate( Node* prev, Node* curr )
	{
		return ( prev->m_marked == false ) 
			&& ( curr->m_marked == false ) 
			&& ( prev->m_next == curr );
	}

	Node m_head = Node( T( ), &m_tail );
	Node m_tail = Node( T( ) );

	Node m_freeListHead = Node( T( ), &m_freeListTail );
	Node m_freeListTail = Node( T( ) );
};