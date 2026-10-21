#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

enum class LockType
{
    NONE,
    SHARED,
    EXCLUSIVE
};

enum class LockRequestOutcome
{
    GRANTED,
    WAIT,
    DIE
};

struct LockRequestResult
{
    LockRequestOutcome outcome = LockRequestOutcome::GRANTED;
    vector<string> conflictingOwners;
};

struct LockSnapshot
{
    LockType lockType = LockType::NONE;
    vector<string> owners;
};

class LockManager
{
    struct LockState
    {
        LockType lockType = LockType::NONE;
        unordered_set<string> owners;
    };

    struct TxnLocks
    {
        unordered_set<string> held;
        vector<string> heldOrder;
    };

    unordered_map<string, long long> txnTimestamp;
    unordered_map<string, TxnLocks> txnLocks;
    unordered_map<string, LockState> lockTable;

    bool isOlderThanAll(const string &requester, const unordered_set<string> &owners) const;
    void recordHeldLock(const string &txnId, const string &pageKey);

public:
    void registerTxn(const string &txnId, long long timestamp);

    LockRequestResult requestShared(const string &txnId, const string &pageKey);
    LockRequestResult requestExclusive(const string &txnId, const string &pageKey);

    vector<string> releaseAll(const string &txnId);

    LockSnapshot snapshot(const string &pageKey) const;
};

