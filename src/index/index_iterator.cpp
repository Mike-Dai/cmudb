/**
 * index_iterator.cpp
 */
#include <cassert>

#include "index/index_iterator.h"

namespace cmudb {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
template <typename KeyType, typename ValueType, typename KeyComparator>
INDEXITERATOR_TYPE::IndexIterator
(BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *leaf, int index, BufferPoolManager *buff_pool_manager):
leaf_(leaf), index_(index), buff_pool_manager_(buff_pool_manager) {}


template <typename KeyType, typename ValueType, typename KeyComparator>
INDEXITERATOR_TYPE::~IndexIterator() = default;

template <typename KeyType, typename ValueType, typename KeyComparator>
bool INDEXITERATOR_TYPE::isEnd() {
	return leaf_ == nullptr || leaf_->GetNextPageId() == INVALID_PAGE_ID
	 && index_ == leaf_->GetSize();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
const MappingType & INDEXITERATOR_TYPE::operator*() {
	if (isEnd()) {
		throw std::out_of_range("index out of range");
	}
	return leaf_->GetItem(index_);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator & INDEXITERATOR_TYPE::operator++() {
	index_++;
	if (index_ == leaf_->GetSize() && leaf_->GetNextPageId() != INVALID_PAGE_ID) {
		auto *page = buff_pool_manager_->FetchPage(leaf_->GetNextPageId());
    	if (page == nullptr) {
      		throw Exception(EXCEPTION_TYPE_INDEX,
                      "all page are pinned while IndexIterator(operator++)");
    	}
    	auto next_leaf =
        	reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType,
                                           KeyComparator> *>(page->GetData());
		index_ = 0;  //???????
		leaf_ = next_leaf;
	}
	
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool INDEXITERATOR_TYPE::operator==(IndexIterator* it) {
	while (!isEnd() && !it->isEnd()) {
		if (*this != *it) {
			return false;
		}
		++this;
		++it;
	}
	if (!isEnd() || !it->isEnd()) {
		return false;
	}
	else {
		return true;
	}
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool INDEXITERATOR_TYPE::operator!=(IndexIterator* it) {
	return !(this == it);
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;
template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;
template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

} // namespace cmudb
