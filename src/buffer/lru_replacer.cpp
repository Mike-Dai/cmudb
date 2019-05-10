/**
 * LRU implementation
 */
#include "buffer/lru_replacer.h"
#include "page/page.h"

#include <algorithm>

namespace cmudb {

template <typename T> LRUReplacer<T>::LRUReplacer() {
	head_ = new Node();
	tail_ = head_;
}

template <typename T> LRUReplacer<T>::~LRUReplacer() {}

/*
 * Insert value into LRU
 */
template <typename T> void LRUReplacer<T>::Insert(const T &value) {
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = items.find(value);
	if (it == items.end()) {
		tail_->next = new Node(value, tail_);
		tail_ = tail_->next;
		items.emplace(value, tail_);
	}
	else {
		if (it->second != tail_) {
			Node *pre = it->second->pre; //????????
			Node *cur = pre->next;
			pre->next = std::move(cur->next);
			pre->next->pre = pre;

			cur->pre = tail_;
			tail_->next = std::move(cur);
			tail_ = tail_->next;
		}
	}
}

/* If LRU is non-empty, pop the head_ member from LRU to argument "value", and
 * return true. If LRU is empty, return false
 */
template <typename T> bool LRUReplacer<T>::Victim(T &value) {
  	std::lock_guard<std::mutex> lock(mutex_);

  	if (items.empty()) {
  		return false;
  	}
  
	value = head_->next->value;
	head_->next = head_->next->next;
	if (head_->next != nullptr) {
		head_->next->pre = head_;
	}

	items.erase(value);
	if (items.size() == 0) {
		tail_ = head_;
	}
	return true;

}

/*
 * Remove value from LRU. If removal is successful, return true, otherwise
 * return false
 */
template <typename T> bool LRUReplacer<T>::Erase(const T &value) {
  	std::lock_guard<std::mutex> lock(mutex_);

  	auto it = items.find(value);
  	if (it != items.end()) {
  		if (it->second != tail_) {
  			Node *pre = it->second->pre;
  			Node *cur = pre->next;
  			pre->next = std::move(cur->next);
  			pre->next->pre = pre;
  		}
  		else {
  			tail_ = tail_->pre;
  			delete tail_->next;
  		}

  		items.erase(value);
  		if (items.size() == 0) {
  			tail_ = head_;
  		}
  		return true;
  	}
  	return false;
}

template <typename T> size_t LRUReplacer<T>::Size() { return items.size() - 1; }

template class LRUReplacer<Page *>;
// test only
template class LRUReplacer<int>;

} // namespace cmudb
