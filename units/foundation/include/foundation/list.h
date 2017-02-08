#pragma once

template<class T>
struct list_node_t
{
	T* _prev;
	T* _next;
};

template<class T>
struct linked_list_t
{
	T* _head;
	T* _tail;

	linked_list_t() : _head(nullptr), _tail(nullptr) { }

	bool empty() const
	{
		return _head == nullptr;
	}

	bool any() const
	{
		return _head != nullptr;
	}

	T* front() const
	{
		return _head;
	}

	T* back() const
	{
		return _tail;
	}

	void push_front(T* node)
	{
		ASSERT(node->_next == nullptr);
		ASSERT(node->_prev == nullptr);

		node->_prev = nullptr;
		node->_next = _head;

		if (_head != nullptr)
		{
			_head->_prev = node;
		}

		_head = node;

		if (_tail == nullptr)
			_tail = _head;
	}

	void push_back(T* node)
	{
		if (_tail == nullptr)
			push_front(node);
		else
		{
			ASSERT(node->_next == nullptr);
			ASSERT(node->_prev == nullptr);
			_tail->_next = node;
			node->_prev = _tail;
			node->_next = nullptr;
			_tail = node;
		}
	}

	void insert_after(T* node, T* after)
	{
		if (node->_next)
		{
			T* next = after->_next;
			next->_prev = node;
		}

		node->_next = after->_next;
		node->_prev = after;
		after->_next = node;

		if (after == _tail)
			_tail = node;
	}

	void insert_before(T* node, T* before)
	{
		if (before->_prev)
		{
			T* prev = before->_prev;
			prev->_next = node;
		}

		node->_next = before;
		node->_prev = before->_prev;
		before->_prev = node;

		if (before == _head)
			_head = node;
	}

	T* pop_front()
	{
		T* ret = front();
		if (ret)
			remove(ret);
		return ret;
	}

	T* pop_back()
	{
		T* ret = back();
		if (ret)
			remove(ret);
		return ret;
	}

	void remove(T* node)
	{
		// TODO: assert this node is actually in this list
		T* next = node->_next;
		T* prev = node->_prev;

		if (node == _head) _head = next;
		if (node == _tail) _tail = prev;
		if (prev != nullptr) prev->_next = next;
		if (next != nullptr) next->_prev = prev;

		node->_next = nullptr;
		node->_prev = nullptr;
	}

	T* next(T* node)
	{
		return node->_next;
	}

	const T* next(const T* node)
	{
		return node->_next;
	}

	T* prev(T* node)
	{
		return node->_prev;
	}

	const T* prev(const T* node)
	{
		return node->_prev;
	}

	void clear()
	{
		while (!empty())
			remove(front());
	}
};