/**
 * lock_manager.cpp
 */

#include <cassert>
#include "concurrency/lock_manager.h"

namespace cmudb {

bool LockManager::LockShared(Transaction *txn, const RID &rid) {
	std::unique_lock<std::mutex> latch(mutex_);
	if (txn->GetState() == TransactionState::ABORTED) {
		return false;
	}
	assert(txn->GetState() == TransactionState::GROWING);
	Request req{txn->GetTransactionId(), LockMode::SHARED, false};
	if (lock_table_.count(rid) == 0) {
		lock_table_[rid].exclusive_count = 0;
		lock_table_[rid].oldest = txn->GetTransactionId();
		lock_table_[rid].list.push_back(req);
	}
	else {
		if (lock_table_[rid].exclusive_count != 0 &&
			txn->GetTransactionId() > lock_table_[rid].oldest) {
			txn->SetState(TransactionState::ABORTED);
			return false;
		}
		if (txn->GetTransactionId() < lock_table_[rid].oldest) {
			lock_table_[rid].oldest = txn->GetTransactionId();
		}
		lock_table_[rid].list.push_back(req);
	}

	//maybe blocked
	Request *cur = nullptr;
	cond.wait(latch, [&]() -> bool {
		bool all_shared = true, all_granted = true;
		for (auto &r : lock_table_[rid].list) {
			if (r.txn_id != txn->GetTransactionId()) {
				if (r.mode != LockMode::SHARED || !r.granted) {
					return false;
				}
			}
			else {
				cur = &r;
				return all_shared && all_granted;
			}
		}
		return false;
	});

	//granted shared lock
	assert(cur != nullptr && cur->txn_id == txn->GetTransactionId());
	cur->granted = true;
	txn->GetSharedLockSet()->insert(rid);

	//notify other threads
	cond.notify_all();
  return true;
}

bool LockManager::LockExclusive(Transaction *txn, const RID &rid) {
  return false;
}

bool LockManager::LockUpgrade(Transaction *txn, const RID &rid) {
  return false;
}

bool LockManager::Unlock(Transaction *txn, const RID &rid) {
  return false;
}

} // namespace cmudb
