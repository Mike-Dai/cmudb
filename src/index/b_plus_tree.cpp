/**
 * b_plus_tree.cpp
 */
#include <iostream>
#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "index/b_plus_tree.h"
#include "page/header_page.h"

namespace cmudb {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(const std::string &name,
                                BufferPoolManager *buffer_pool_manager,
                                const KeyComparator &comparator,
                                page_id_t root_page_id)
    : index_name_(name), root_page_id_(root_page_id),
      buffer_pool_manager_(buffer_pool_manager), comparator_(comparator) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::GetValue(const KeyType &key,
                              std::vector<ValueType> &result,
                              Transaction *transaction) {
  auto *leaf = FindLeafPage(key, false);
  if (leaf != nullptr) {
    ValueType value;
    if(leaf->Lookup(key, value, comparator_)) {
      result.push_back(value);
      buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
      return true;
    }
  }
  return false;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value,
                            Transaction *transaction) {
  if (IsEmpty()) {
    StartNewTree(key, value);
    return true;
  }
  else {
    return InsertIntoLeaf(key, value, transaction);
  }
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  auto *page = buffer_pool_manager_->NewPage(root_page_id_);
  if (page == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,
                    "all page are pinned while StartNewTree");
  }

  auto root = 
    reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>
    (page->GetData());
  UpdateRootPageId(true);
  root->Init(root_page_id_, INVALID_PAGE_ID);
  root->Insert(key, value, comparator_);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immediately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                    Transaction *transaction) {
  auto *leaf = FindLeafPage(key, false);
  if (leaf == nullptr) {
    return false;
  }
  if (leaf->GetSize() < leaf->GetMaxSize()) {
    ValueType v;
    if (leaf->Lookup(key, v, comparator_)) {
      return false;
    }
    leaf->Insert(key, value, comparator_);
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
  }
  else {
    auto *leaf2 = Split<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>>(leaf);

    if (comparator_(key, leaf2->KeyAt(0)) < 0) {
      leaf->Insert(key, value, comparator_);
    }
    else {
      leaf2->Insert(key, value, comparator_);
    }

    if (comparator_(leaf->KeyAt(0), leaf2->KeyAt(0)) < 0) {
      leaf2->SetNextPageId(leaf->GetNextPageId());
      leaf->SetNextPageId(leaf2->GetPageId());
    } else {
      leaf2->SetNextPageId(leaf->GetPageId());
    }

    InsertIntoParent(leaf, leaf2->KeyAt(0), leaf2, transaction);
  
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(leaf2->GetPageId(), true);
  }
  return true;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N> N *BPLUSTREE_TYPE::Split(N *node) { 
    page_id_t page_id;
    auto *page = buffer_pool_manager_->NewPage(page_id);
    if (page == nullptr) {
      throw Exception(EXCEPTION_TYPE_INDEX,
                      "all pages are pinned while split");
    }
    auto new_node = reinterpret_cast<N *>(page->GetData());
    new_node->Init(page_id);

    node->MoveHalfTo(new_page, buffer_pool_manager_);
    return new_page;
 }

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node,
                                      const KeyType &key,
                                      BPlusTreePage *new_node,
                                      Transaction *transaction) {
    if (old_node->IsRootPage()) {
      auto *page = buffer_pool_manager_->NewPage(root_page_id_);
      if (page == nullptr) {
        throw Exception(EXCEPTION_TYPE_INDEX,
                        "all page are pinned while InsertIntoParent");
    }
    auto root =
    reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t,
                                           KeyComparator> *>(page->GetData());

    root->Init(root_page_id_);
    root->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());

    old_node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);

    UpdateRootPageId(false);

    buffer_pool_manager_->UnpinPage(old_node->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);

    buffer_pool_manager_->UnpinPage(root->GetPageId(), true);
  }
  else {
    auto *page = buffer_pool_manager_->FetchPage(old_node->GetParentPageId());
    if (page == nullptr) {
      throw Exception(EXCEPTION_TYPE_INDEX,
                      "all page are pinned while InsertIntoParent");
    }
    auto internal =
        reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t,
                                               KeyComparator> *>(page->GetData());

    if (internal->GetSize() < internal->GetMaxSize()) {
      internal->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());

      new_node->SetParentPageId(internal->GetPageId());

      buffer_pool_manager_->UnpinPage(old_node->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(internal->GetPageId(), true);
    }
    else {
      page_id_t page_id;
      auto *page = buffer_pool_manager_->NewPage(page_id);
      if (page == nullptr) {
        throw Exception(EXCEPTION_TYPE_INDEX,
                        "all page are pinned while InsertIntoParent");
      }
      auto *copy = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t,
                                                          KeyComparator> *>(page->GetData());
      copy->Init(page_id);
      copy->SetSize(internal->GetSize());
      for (int i = 1, j = 0; i <= internal->GetSize(); ++i, ++j) {
        if (internal->ValueAt(i - 1) == old_node->GetPageId()) {
          copy->SetKeyAt(j, key);
          copy->SetValueAt(j, new_node->GetPageId());
          ++j;
        }
        // not the end
        if (i < internal->GetSize()) {
          copy->SetKeyAt(j, internal->KeyAt(i));
          copy->SetValueAt(j, internal->ValueAt(i));
        }
      }

      assert(copy->GetSize() == copy->GetMaxSize());
      auto internal2 =
          Split<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>>(copy);

      internal->SetSize(copy->GetSize() + 1);
      for (int i = 0; i < copy->GetSize(); ++i) {
        internal->SetKeyAt(i + 1, copy->KeyAt(i));
        internal->SetValueAt(i + 1, copy->ValueAt(i));
      }

      if (comparator_(key, internal2->KeyAt(0)) < 0) {
        new_node->SetParentPageId(internal->GetPageId());
      }
      else {
        new_node->SetParentPageId(internal2->GetPageId());
        old_node->SetParentPageId(internal2->GetPageId());
      }

      buffer_pool_manager_->UnpinPage(old_node->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);

      buffer_pool_manager_->UnpinPage(copy->GetPageId(), false);
      buffer_pool_manager_->DeletePage(copy->GetPageId());

      InsertIntoParent(internal, internal2->KeyAt(0), internal2);
    }
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if (IsEmpty()) {
    return;
  }
  
  auto *leaf = FindLeafPage(key, false);
  if (leaf != nullptr) {
    leaf->RemoveAndDeleteRecord(key, comparator_);
    if (CoalesceOrRedistribute(leaf, transaction)) {
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
    buffer_pool_manager_->DeletePage(leaf->GetPageId()); 
    }
    else {
      buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
    }
  }  
}

/*
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) {
  if (node->IsRootPage()) {
    return AdjustRoot(node);
  }

  if (node->GetSize() >= node->GetMinSize()) {
    return false;
  }

  auto *page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  if (page == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,
                    "all page are pinned while CoalesceOrRedistribute");
  }
  auto parent = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>
                  (page->GetData());

  int value_index = parent->ValueIndex(node->GetPageId());
  int sibling_page_id;
  if (value_index == 0) {
    sibling_page_id = parent->ValueAt(value_index + 1);
  }
  else {
    sibling_page_id = parent->ValueAt(value_index - 1);
  }

  page = buffer_pool_manager_->FetchPage(sibling_page_id);
  if (page == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,
                    "all page are pinned while CoalesceOrRedistribute");
  }
  auto sibling = reinterpret_cast<N *>(page->GetData());

  if (node->GetSize() + sibling->GetSize() > node->GetMaxSize()) {
    if (value_index == 0) {
      Redistribute<N>(sibling, node, 0);
    }
    else {
      Redistribute<N>(sibling, node, 1);
    }
    buffer_pool_manager_->UnpinPage(sibling->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);
    return false;
  }
  
  if (Coalesce<N>(node, sibling, parent, value_index, transaction)) {
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), false);
    buffer_pool_manager_->DeletePage(parent->GetPageId());
  }
  else {
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);
  }

  if (value_index == 0) {           //????????
    buffer_pool_manager_->UnpinPage(sibling->GetPageId(), false);
    buffer_pool_manager_->DeletePage(sibling->GetPageId());
    return false;
  }
  else {
    buffer_pool_manager_->UnpinPage(sibling->GetPageId(), true);
    return true;
  }
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::Coalesce(
    N *&neighbor_node, N *&node,
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *&parent,
    int index, Transaction *transaction) {
  node->MoveAllTo(neighbor_node, index, buffer_pool_manager_);
  parent->Remove(index);

  if (CoalesceOrRedistribute(parent, transaction)) {
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), false);  
    buffer_pool_manager_->DeletePage(parent->GetPageId());
    return true;
  }
  else {
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);
    return false;
  }
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node, int index) {
  if (index == 0) {
    neighbor_node->MoveFirstToEndOf(node, buffer_pool_manager_);
  }
  else {
    auto *page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
    if (page == nullptr) {
      throw Exception(EXCEPTION_TYPE_INDEX,
                      "all page are pinned while Redistribute");
    }

        auto parent =
        reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t,
                                               KeyComparator> *>(page->GetData());

    int index = parent->ValueIndex(node->GetPageId());
    neighbor_node->MoveLastToFrontOf(node, index, buffer_pool_manager_);
  }
}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  if (old_root_node->IsLeafPage()) { //????????????
    if (old_root_node->GetSize() == 0) {
      root_page_id_ = INVALID_PAGE_ID;
      UpdateRootPageId(false);
      return true;
    }
    return false;
  }

  if (old_root_node->GetSize() == 1) {
    auto root = 
        reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(old_root_node);
    root_page_id_ = root->ValueAt(0);
    UpdateRootPageId(false);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leaftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin() { 
    KeyType key{};
    return IndexIterator<KeyType, ValueType, KeyComparator>
            (FindLeafPage(key, true), 0, buffer_pool_manager_);
 }

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) {
  auto *leaf = FindLeafPage(key, false);
  int index = 0;
  if (leaf != nullptr) {
    index = leaf->KeyIndex(key, comparator_);
  }
  return IndexIterator<KeyType, ValueType, KeyComparator>
          (leaf, index, buffer_pool_manager_);  //why true???????
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 */
INDEX_TEMPLATE_ARGUMENTS
B_PLUS_TREE_LEAF_PAGE_TYPE *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key,
                                                         bool leftMost) {
  if (IsEmpty()) {
    return nullptr;
  }

  auto *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,
                    "all page are pinned while FindLeafPage");
  }
  auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());

  while (node->IsLeafPage()) {
    auto internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(node);
    page_id_t next;

    if (leftMost) {
      next = internal->ValueAt(0);
    }
    else {
      next = internal->Lookup(key, comparator_);
    }

    page = buffer_pool_manager_->FetchPage(next);
    if (page == nullptr) {
      throw Exception(EXCEPTION_TYPE_INDEX,
                      "all page are pinned while FindLeafPage");
    }
    node = reinterpret_cast<BPlusTreePage *>(page->GetData());

    buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
  }

  auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(node);
  return leaf;
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  auto *page = buffer_pool_manager_->FetchPage(HEADER_PAGE_ID);
  if (page == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,
                    "all page are pinned while UpdateRootPageId");
  }
  auto *header_page = reinterpret_cast<HeaderPage *>(page->GetData());

  if (insert_record)
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  else
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for debug only
 * print out whole b+tree sturcture, rank by rank
 */
INDEX_TEMPLATE_ARGUMENTS
std::string BPLUSTREE_TYPE::ToString(bool verbose) { return "Empty tree"; }

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name,
                                    Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key{};
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name,
                                    Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key{};
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

} // namespace cmudb
