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
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator
(BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>* leaf, BufferPoolManager* buff_pool_manager):
leaf_(leaf), buff_pool_manager_(buff_pool_manager) {}


INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
bool INDEXITERATOR_TYPE::isEnd() {
	return (leaf_->GetNextPageId() == INVALID_PAGE_ID)
	&& (leaf_ != nullptr && index_ == leaf_->GetSize());
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;
template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;
template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

} // namespace cmudb
