/**
 * LRU implementation
 */
#include "buffer/lru_replacer.h"
#include "page/page.h"

#include <algorithm>

namespace cmudb {

template <typename T> LRUReplacer<T>::LRUReplacer() {
	head = new Node();
	tail = head;
}

template <typename T> LRUReplacer<T>::~LRUReplacer() {}

/*
 * Insert value into LRU
 */
template <typename T> void LRUReplacer<T>::Insert(const T &value) {
	auto it = items.find(value);
	if (it == items.end()) {
		Node node = Node(value);
		tail->next = node;
		tail = tail->next;
		items.emplace(value, tail);
	}
	else {
		if (it.second != tail) {
			Node *pre = it->second->pre; //????????
			Node *cur = pre->next;
			pre->next = std::move(cur->next);
			pre->next->pre = pre;

			cur->pre = tail;
			tail->next = std::move(cur);
			tail = tail->next;
		}
	}
}

/* If LRU is non-empty, pop the head member from LRU to argument "value", and
 * return true. If LRU is empty, return false
 */
template <typename T> bool LRUReplacer<T>::Victim(T &value) {
  if (!items.empty()) {
  	value = head->value;
  	items.erase(value);
  	return true;
  }
  else {
  	return false;
  }
}

/*
 * Remove value from LRU. If removal is successful, return true, otherwise
 * return false
 */
template <typename T> bool LRUReplacer<T>::Erase(const T &value) {
  size_t old_size = items.size();
  items.erase(value);
  return items.size() != old_size;
}

template <typename T> size_t LRUReplacer<T>::Size() { return items.size() - 1; }

template class LRUReplacer<Page *>;
// test only
template class LRUReplacer<int>;

} // namespace cmudb
