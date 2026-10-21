#include "lockManager.h"

#include <algorithm>

void LockManager::registerTxn(const string &txnId, long long timestamp)
{
    if (!this->txnTimestamp.count(txnId))
        this->txnTimestamp[txnId] = timestamp;
}

bool LockManager::isOlderThanAll(const string &requester, const unordered_set<string> &owners) const
{
    auto it = this->txnTimestamp.find(requester);
    if (it == this->txnTimestamp.end())
        return false;
    long long requesterTs = it->second;

    for (const auto &owner : owners)
    {
        auto ownerIt = this->txnTimestamp.find(owner);
        if (ownerIt == this->txnTimestamp.end())
            return false;
        if (!(requesterTs < ownerIt->second))
            return false;
    }
    return true;
}

void LockManager::recordHeldLock(const string &txnId, const string &pageKey)
{
    TxnLocks &locks = this->txnLocks[txnId];
    if (locks.held.insert(pageKey).second)
        locks.heldOrder.push_back(pageKey);
}

LockRequestResult LockManager::requestShared(const string &txnId, const string &pageKey)
{
    LockState &state = this->lockTable[pageKey];

    if (state.lockType == LockType::NONE)
    {
        state.lockType = LockType::SHARED;
        state.owners.clear();
        state.owners.insert(txnId);
        this->recordHeldLock(txnId, pageKey);
        return {};
    }

    if (state.lockType == LockType::SHARED)
    {
        state.owners.insert(txnId);
        this->recordHeldLock(txnId, pageKey);
        return {};
    }

    // EXCLUSIVE
    if (state.owners.count(txnId))
    {
        this->recordHeldLock(txnId, pageKey);
        return {};
    }

    LockRequestResult res;
    res.conflictingOwners.assign(state.owners.begin(), state.owners.end());
    res.outcome = this->isOlderThanAll(txnId, state.owners) ? LockRequestOutcome::WAIT : LockRequestOutcome::DIE;
    sort(res.conflictingOwners.begin(), res.conflictingOwners.end());
    return res;
}

LockRequestResult LockManager::requestExclusive(const string &txnId, const string &pageKey)
{
    LockState &state = this->lockTable[pageKey];

    if (state.lockType == LockType::NONE)
    {
        state.lockType = LockType::EXCLUSIVE;
        state.owners.clear();
        state.owners.insert(txnId);
        this->recordHeldLock(txnId, pageKey);
        return {};
    }

    if (state.lockType == LockType::EXCLUSIVE)
    {
        if (state.owners.size() == 1 && state.owners.count(txnId))
        {
            this->recordHeldLock(txnId, pageKey);
            return {};
        }

        LockRequestResult res;
        res.conflictingOwners.assign(state.owners.begin(), state.owners.end());
        res.outcome = this->isOlderThanAll(txnId, state.owners) ? LockRequestOutcome::WAIT : LockRequestOutcome::DIE;
        sort(res.conflictingOwners.begin(), res.conflictingOwners.end());
        return res;
    }

    // SHARED
    unordered_set<string> conflictingOwners;
    conflictingOwners = state.owners;
    conflictingOwners.erase(txnId);

    if (conflictingOwners.empty() && state.owners.count(txnId))
    {
        // upgrade S->X
        state.lockType = LockType::EXCLUSIVE;
        state.owners.clear();
        state.owners.insert(txnId);
        this->recordHeldLock(txnId, pageKey);
        return {};
    }

    LockRequestResult res;
    res.conflictingOwners.assign(conflictingOwners.begin(), conflictingOwners.end());
    res.outcome = this->isOlderThanAll(txnId, conflictingOwners) ? LockRequestOutcome::WAIT : LockRequestOutcome::DIE;
    sort(res.conflictingOwners.begin(), res.conflictingOwners.end());
    return res;
}

vector<string> LockManager::releaseAll(const string &txnId)
{
    vector<string> unlocked;
    auto it = this->txnLocks.find(txnId);
    if (it == this->txnLocks.end())
        return unlocked;

    for (const auto &pageKey : it->second.heldOrder)
    {
        auto lockIt = this->lockTable.find(pageKey);
        if (lockIt == this->lockTable.end())
            continue;

        lockIt->second.owners.erase(txnId);
        if (lockIt->second.owners.empty())
            lockIt->second.lockType = LockType::NONE;

        unlocked.push_back(pageKey);
    }

    it->second.held.clear();
    it->second.heldOrder.clear();
    return unlocked;
}

LockSnapshot LockManager::snapshot(const string &pageKey) const
{
    LockSnapshot snap;
    auto it = this->lockTable.find(pageKey);
    if (it == this->lockTable.end())
        return snap;
    snap.lockType = it->second.lockType;
    snap.owners.assign(it->second.owners.begin(), it->second.owners.end());
    sort(snap.owners.begin(), snap.owners.end());
    return snap;
}

