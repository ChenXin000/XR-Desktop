#ifndef DOUBLE_LINKED_LIST_H
#define DOUBLE_LINKED_LIST_H

template <class T>
struct ListNode
{
	ListNode<T>* prev ;
	ListNode<T>* next ;
	T data;
};

template <class T>
class DLinkList
{
public:
	DLinkList();
	~DLinkList();

private:
	ListNode<T>* begin;
	ListNode<T>* end;

public:
	const ListNode<T>* getBegin() const;
	const ListNode<T>* getEnd() const;

	void pushBack(T& data);
	ListNode<T>* remove(const ListNode<T> *node);
	void remove(T& data);
	const ListNode<T>* find(T& data);
	bool isEmpty();

	void destroy();
};

template <class T>
inline DLinkList<T>::DLinkList()
{
	begin = new ListNode<T>;
	end = new ListNode<T>;
	
	begin->next = end;
	end->prev = begin;
}

template<class T>
inline DLinkList<T>::~DLinkList()
{
	destroy();

	delete begin;
	delete end;
}
// ��ȡ��ʼ�ڵ�
template<class T>
inline const ListNode<T>* DLinkList<T>::getBegin() const
{
	return begin->next;
}
// ��ȡ�����ڵ�
template<class T>
inline const ListNode<T>* DLinkList<T>::getEnd() const
{
	return end;
}
// ������β���������
template<class T>
inline void DLinkList<T>::pushBack(T& data)
{
	ListNode<T>* newEnd = new ListNode<T>;
	newEnd->prev = end;
	end->next = newEnd;
	end->data = data;
	end = newEnd;
}
// �Ƴ���ǰ�ڵ㣬��������һ���ڵ�
template<class T>
inline ListNode<T>* DLinkList<T>::remove(const ListNode<T>* node)
{
	ListNode<T>* prevNode = node->prev;
	ListNode<T>* nextNode = node->next;

	prevNode->next = nextNode;
	nextNode->prev = prevNode;

	delete node;
	return nextNode;
}
// �Ƴ�һ���ڵ�
template<class T>
inline void DLinkList<T>::remove(T& data)
{
	ListNode<T>* item = begin->next;
	while (item != end)
	{
		if (item->data == data)
		{
			remove(item);
			break;
		}
		item = item->next;
	}
}
// Ѱ�ҽڵ�
template<class T>
inline const ListNode<T>* DLinkList<T>::find(T& data)
{
	ListNode<T>* item = begin->next;
	while (item != end)
	{
		if (item->data == data)
			return item;
		item = item->next;
	}
	return nullptr;
}
// �Ƿ�Ϊ��
template<class T>
inline bool DLinkList<T>::isEmpty()
{
	return (begin->next == end);
}
// �������нڵ�
template<class T>
inline void DLinkList<T>::destroy()
{
	const ListNode<T>* item = begin->next;

	while (item != end)
	{
		const ListNode<T>* temp = item;
		item = item->next;
		remove(temp);
	}
}

#endif // !DOUBLE_LINKED_LIST_H

