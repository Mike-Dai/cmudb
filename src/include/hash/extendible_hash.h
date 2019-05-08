/*
 * extendible_hash.h : implementation of in-memory hash table using extendible
 * hashing
 *
 * Functionality: The buffer pool manager must maintain a page table to be able
 * to quickly map a PageId to its corresponding memory location; or alternately
 * report that the PageId does not match any currently-buffered page.
 */

#pragma once

#include <cstdlib>
#include <vector>
#include <string>
#include <map>
#include <memory>

#include "hash/hash_table.h"

namespace cmudb {

template <typename K, typename V>
class ExtendibleHash : public HashTable<K, V> {
  struct Bucket {
    Bucket() = default;
    explicit Bucket(int id, int depth): id(id), depth(depth) {}
    size_t id = 0;
    int depth = 0;
    bool overflow = false;
    std::map<K, V> items;
  };
public:
  // constructor
  ExtendibleHash(size_t size);
  // helper function to generate hash addressing
  size_t HashKey(const K &key);
  // helper function to get global & local depth
  int GetGlobalDepth() const;
  int GetLocalDepth(int bucket_id) const;
  int GetNumBuckets() const;
  // lookup and modifier
  bool Find(const K &key, V &value) override;
  bool Remove(const K &key) override;
  void Insert(const K &key, const V &value) override;

private:
  // add your own member variables here
  mutable std::mutex mutex_;
  std::unique_ptr<Bucket> split(std::shared_ptr<Bucket> &);
  std::vector<std::shared_ptr<Bucket>> buckets;
  size_t bucket_size;
  int depth;
  size_t pair_count;  //??????????
  int bucket_number; //used bucket
};
} // namespace cmudb
