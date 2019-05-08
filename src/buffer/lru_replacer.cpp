/**
 * LRU implementation
 */
#include "buffer/lru_replacer.h"
#include "page/page.h"

#include <algorithm>

namespace cmudb {

template <typename T> LRUReplacer<T>::LRUReplacer() {}

template <typename T> LRUReplacer<T>::~LRUReplacer() {}

/*
 * Insert value into LRU
 */
template <typename T> void LRUReplacer<T>::Insert(const T &value) {
	if (std::find(items.begin(), items.end(), value) == items.end()) {
		items.push_back(value);
	}
	else {
		items.erase(std::remove(items.begin(), items.end(), value), items.end());
		items.push_back(value);
	}
}

/* If LRU is non-empty, pop the head member from LRU to argument "value", and
 * return true. If LRU is empty, return false
 */
template <typename T> bool LRUReplacer<T>::Victim(T &value) {
  if (!items.empty()) {
  	value = items.front();
  	items.pop_front();
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
  items.erase(std::remove(items.begin(), items.end(), value), items.end());
  return items.size() != old_size;
}

template <typename T> size_t LRUReplacer<T>::Size() { return items.size(); }

template class LRUReplacer<Page *>;
// test only
template class LRUReplacer<int>;

} // namespace cmudb
