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

#include "hash/hash_table.h"

namespace cmudb {

template <typename K, typename V>
class ExtendibleHash : public HashTable<K, V> {
  class Bucket {
  public:
    int id;
    int depth;
    int size;
    bool overflow;
    std::map<K, V> items;
    Bucket(int id, int depth): id(id), depth(depth), overflow(false) {
    }
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
  std::unique_ptr<Bucket> split(std::shared_ptr<Bucket> &);
  std::vector<std::shared_ptr<Bucket>> buckets;
  int bucket_size;
  int global_depth;
  int bucket_number; //used bucket
};
} // namespace cmudb
