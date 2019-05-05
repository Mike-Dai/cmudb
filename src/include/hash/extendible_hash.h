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

const int BUCKET_SIZE = 3;

namespace cmudb {

template <typename K, typename V>
class ExtendibleHash : public HashTable<K, V> {
  class Bucket {
  private:
    int id;
    int depth;
    bool overflow;
    V* value;
  public:
    Bucket(int id): id(id), depth(1), size(3), overflow(false) {
      value = (V*)malloc(size);
    }
    int GetDepth() {
      return depth;
    }
    int GetID() {
      return id;
    }
    V* GetValue() {
      return value;
    }
    void IncrSize() {
      size++;
    }
    void UpdateValue(const V &value) {
      int i = 0;
      if (GetSize() == 0) {
        first = Node(value);
      }
      else {
        int depth = GetDepth();
        Node node = first;
        while (i < depth) {
          if (!node.isFull()) {

          }
        }
      }
      IncrSize();
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
  int size;
  int globalDepth;
  int number;
};
} // namespace cmudb
