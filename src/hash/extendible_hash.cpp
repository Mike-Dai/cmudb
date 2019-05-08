#include <list>

#include "hash/extendible_hash.h"
#include "page/page.h"
#include <vector>
#include <memory>

namespace cmudb {

/*
 * constructor
 * array_size: fixed array size for each bucket
 */
template <typename K, typename V>
ExtendibleHash<K, V>::ExtendibleHash(size_t size): 
bucket_size(size), depth(0), bucket_number(0) {
	buckets.emplace_back(Bucket(0, 0));
	bucket_number = 1;
}

/*
 * helper function to calculate the hashing address of input key
 */
template <typename K, typename V>
size_t ExtendibleHash<K, V>::HashKey(const K &key) {
  return std::hash<K>()(key);
}

/*
 * helper function to return global depth of hash table
 * NOTE: you must implement this function in order to pass test
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetGlobalDepth() const {
  return depth;
}

/*
 * helper function to return local depth of one specific bucket
 * NOTE: you must implement this function in order to pass test
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetLocalDepth(int bucket_id) const {
  if (buckets[bucket_id]) {
  	return buckets[bucket_id]->depth;
  }
  return -1;
}

/*
 * helper function to return current number of bucket in hash table
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetNumBuckets() const {
  return bucket_number;
}

/*
 * lookup function to find value associate with input key
 */
template <typename K, typename V>
bool ExtendibleHash<K, V>::Find(const K &key, V &value) {
	int id = HashKey(key) & ((1 << depth) - 1);
	if (buckets[id]->items.find(key) != buckets[id]->items.end()) {
		value = buckets[id]->items[key];
		return true;
	}
  return false;
}

/*
 * delete <key,value> entry in hash table
 * Shrink & Combination is not required for this project
 */
template <typename K, typename V>
bool ExtendibleHash<K, V>::Remove(const K &key) {
	int id = HashKey(key) & ((1 << depth) - 1);
	if (buckets[id]->items.find(key) != buckets[id]->items.end()) {
		buckets[id]->items.erase(key);
		return true;
	}
	else {
		return false;
	}
}

template <typename K, typename V>
std::unique_ptr<typename ExtendibleHash<K, V>::Bucket> 
ExtendibleHash<K, V>::split(std::shared_ptr<Bucket> &b) {
	auto res = std::make_unique<Bucket>(0, b->depth);
	while (res->items.empty()) {
		++b->depth;
		++res->depth;
		for (auto it = b->items.begin(); it != b->items.end(); ) {
			if (HashKey(it->first) & (1 << (b->depth - 1))) {  //????????????
				res->items.insert(*it);
				res->id = HashKey(it->first) & ((1 << b->depth) - 1);
				it = b->items.erase(it);
			}
			else {
				++it;
			}
		}
		if (b->items.empty()) { //????????????
			b->items.swap(res->items);
			b->id = res->id;
		}
	}
	++bucket_number;
	return res;
}

/*
 * insert <key,value> entry in hash table
 * Split & Redistribute bucket when there is overflow and if necessary increase
 * global depth
 */
template <typename K, typename V>
void ExtendibleHash<K, V>::Insert(const K &key, const V &value) {
	size_t id = HashKey(key) & ((1 << depth) - 1);
	if (buckets[id] == nullptr) {
		buckets[id] = std::make_shared<Bucket>(id, depth);
		++bucket_number;
	}
	auto bucket = buckets[id];
	if (bucket->items.find(key) != bucket->items.end()) {
		bucket->items[key] = value;
		return;
	}
	else {   //not found
		bucket->items.insert({key, value});
		if (bucket->items.size() < bucket_size) {
			auto old_id = bucket->id;
			auto old_depth = bucket->depth;
			std::shared_ptr<Bucket> new_bucket = split(bucket);
			if (new_bucket == nullptr) {
				bucket->depth = old_depth;
				return;
			}
			if (bucket->depth > depth) {
				auto size = buckets.size();
				auto factor = (1 << (bucket->depth - depth));
				depth = bucket->depth;
				buckets.resize(buckets.size() * factor);
				
				buckets[bucket->id] = bucket;
				buckets[new_bucket->id] =  new_bucket;
				
				for (size_t i = 0; i < size; ++i) {
					if (buckets[i]) {
						if (i < buckets[i]->id || ((i & ((1 << buckets[i]->depth) - 1)) != buckets[i]->id)) {
							buckets[i].reset();
						}
						else {
							auto step = 1 << buckets[i]->depth;
							for (size_t j = i + step; j < buckets.size(); j += step) {
								buckets[j] = buckets[i];
							}
						}
					}
				}
			}
			else {
				for (size_t i = old_id; i < buckets.size(); i += (1 << old_depth)) {
					buckets[i].reset();
				}

				buckets[bucket->id] = bucket;
				buckets[new_bucket->id] = new_bucket;

				auto step = 1 << bucket->depth;
				for (size_t i = bucket->id + step; i < buckets.size(); i += step) {
					buckets[i] = bucket;
				}
				for (size_t i = new_bucket->id + step; i < buckets.size(); i += step) {
					buckets[i] = new_bucket;
				}
			}
		}
	}
}

template class ExtendibleHash<page_id_t, Page *>;
template class ExtendibleHash<Page *, std::list<Page *>::iterator>;
// test purpose
template class ExtendibleHash<int, std::string>;
template class ExtendibleHash<int, std::list<int>::iterator>;
template class ExtendibleHash<int, int>;
} // namespace cmudb
