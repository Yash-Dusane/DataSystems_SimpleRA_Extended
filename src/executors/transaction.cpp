#include "global.h"
#include "lockManager.h"

#include <algorithm>
#include <deque>
#include <sstream>

enum class TxnOpType
{
    BEGIN,
    READ,
    WRITE,
    COMMIT
};

struct TxnOp
{
    TxnOpType type = TxnOpType::BEGIN;
    string txnId = "";
    string tableName = "";
    int pageIndex = -1;
};

enum class TxnState
{
    ACTIVE,
    WAITING,
    ABORTED,
    COMMITTED
};

struct TxnContext
{
    bool hasTimestamp = false;
    long long timestamp = 0;
    TxnState state = TxnState::ACTIVE;

    bool hasPending = false;
    TxnOp pendingOp;
    deque<TxnOp> delayedOps;
};

static string pageKey(const string &tableName, int pageIndex)
{
    return tableName + "|" + to_string(pageIndex);
}

static bool splitPageKey(const string &key, string &tableName, int &pageIndex)
{
    auto pos = key.find('|');
    if (pos == string::npos)
        return false;
    tableName = key.substr(0, pos);
    string pageStr = key.substr(pos + 1);
    try
    {
        pageIndex = stoi(pageStr);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

static bool fileExists(const string &filePath)
{
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

static string outputFileNameForInput(const string &inputFileName)
{
    const string suffix = ".txt";
    if (inputFileName.size() >= suffix.size() &&
        inputFileName.compare(inputFileName.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        return inputFileName.substr(0, inputFileName.size() - suffix.size()) + "_output.txt";
    }
    return inputFileName + "_output.txt";
}

static void logLine(ofstream &fout, const string &line)
{
    fout << line << "\n";
}

static string lockTypeToString(LockType type)
{
    switch (type)
    {
    case LockType::SHARED:
        return "shared";
    case LockType::EXCLUSIVE:
        return "exclusive";
    default:
        return "none";
    }
}

static string joinTxnIds(const vector<string> &txnIds)
{
    string result;
    for (int i = 0; i < txnIds.size(); i++)
    {
        if (i)
            result += ", ";
        result += txnIds[i];
    }
    return result;
}

static string conflictingTxnText(const vector<string> &txnIds)
{
    if (txnIds.empty())
        return "older transaction";
    if (txnIds.size() == 1)
        return txnIds.front();
    return "transactions " + joinTxnIds(txnIds);
}

static string waitDieBlockerText(const string &txnId, const vector<string> &conflictingOwners, const unordered_map<string, TxnContext> &txns)
{
    auto txnIt = txns.find(txnId);
    if (txnIt == txns.end())
        return conflictingTxnText(conflictingOwners);

    long long txnTimestamp = txnIt->second.timestamp;
    string blocker = "";
    long long blockerTimestamp = txnTimestamp;

    for (const auto &owner : conflictingOwners)
    {
        auto ownerIt = txns.find(owner);
        if (ownerIt == txns.end())
            continue;

        if (ownerIt->second.timestamp < txnTimestamp &&
            (blocker.empty() || ownerIt->second.timestamp < blockerTimestamp))
        {
            blocker = owner;
            blockerTimestamp = ownerIt->second.timestamp;
        }
    }

    if (!blocker.empty())
        return blocker;
    return conflictingTxnText(conflictingOwners);
}

static bool parseScheduleFile(const string &scheduleFilePath, vector<TxnOp> &ops, unordered_map<string, vector<TxnOp>> &opsPerTxn)
{
    ifstream fin(scheduleFilePath, ios::in);
    if (!fin.good())
        return false;

    ops.clear();
    opsPerTxn.clear();

    string line;
    while (getline(fin, line))
    {
        // trim whitespace-only lines
        bool nonSpace = false;
        for (char c : line)
        {
            if (!isspace(static_cast<unsigned char>(c)))
            {
                nonSpace = true;
                break;
            }
        }
        if (!nonSpace)
            continue;

        istringstream iss(line);
        vector<string> tokens;
        string tok;
        while (iss >> tok)
            tokens.push_back(tok);

        if (tokens.size() == 2 && tokens[0] == "BEGIN")
        {
            TxnOp op;
            op.type = TxnOpType::BEGIN;
            op.txnId = tokens[1];
            ops.push_back(op);
            continue;
        }

        if (tokens.size() == 2 && tokens[0] == "COMMIT")
        {
            TxnOp op;
            op.type = TxnOpType::COMMIT;
            op.txnId = tokens[1];
            ops.push_back(op);
            opsPerTxn[op.txnId].push_back(op);
            continue;
        }

        if (tokens.size() == 4 && (tokens[0] == "READ" || tokens[0] == "WRITE"))
        {
            TxnOp op;
            op.type = (tokens[0] == "READ") ? TxnOpType::READ : TxnOpType::WRITE;
            op.txnId = tokens[1];
            op.tableName = tokens[2];
            try
            {
                op.pageIndex = stoi(tokens[3]);
            }
            catch (...)
            {
                return false;
            }
            ops.push_back(op);
            opsPerTxn[op.txnId].push_back(op);
            continue;
        }

        return false;
    }

    return true;
}

static bool executeReadOrWrite(const TxnOp &op, ofstream &fout)
{
    if (op.type == TxnOpType::READ)
    {
        Cursor cursor(op.tableName, op.pageIndex);
        (void)cursor.getNext();
        logLine(fout, "READ " + op.txnId + " " + op.tableName + " " + to_string(op.pageIndex));
        return true;
    }

    if (op.type == TxnOpType::WRITE)
    {
        Cursor cursor(op.tableName, op.pageIndex);
        (void)cursor.page.getRow(0);
        logLine(fout, "WRITE " + op.txnId + " " + op.tableName + " " + to_string(op.pageIndex));
        return true;
    }

    return false;
}

static void logUnlocks(const vector<string> &unlocked, const string &txnId, ofstream &fout)
{
    for (const auto &u : unlocked)
    {
        string t;
        int p;
        if (splitPageKey(u, t, p))
            logLine(fout, "unlock(" + t + " " + to_string(p) + ") by " + txnId);
    }
}

static void queueDelayedOpsAtFront(TxnContext &ctx, deque<TxnOp> &workQueue)
{
    while (!ctx.delayedOps.empty())
    {
        workQueue.push_front(ctx.delayedOps.back());
        ctx.delayedOps.pop_back();
    }
}

static void resumeWaitingTransactions(unordered_map<string, TxnContext> &txns, LockManager &lockManager, ofstream &fout, unordered_set<string> *abortedTxns = nullptr, deque<TxnOp> *workQueue = nullptr)
{
    bool progressed = true;
    while (progressed)
    {
        progressed = false;
        vector<string> waiting;
        waiting.reserve(txns.size());
        for (auto &kv : txns)
        {
            if (kv.second.state == TxnState::WAITING && kv.second.hasPending)
                waiting.push_back(kv.first);
        }

        sort(waiting.begin(), waiting.end(), [&](const string &a, const string &b) {
            return txns[a].timestamp < txns[b].timestamp;
        });

        for (const auto &txnId : waiting)
        {
            TxnContext &ctx = txns[txnId];
            if (ctx.state != TxnState::WAITING || !ctx.hasPending)
                continue;

            const TxnOp &pending = ctx.pendingOp;
            string key = pageKey(pending.tableName, pending.pageIndex);
            LockRequestResult res = (pending.type == TxnOpType::READ) ? lockManager.requestShared(txnId, key) : lockManager.requestExclusive(txnId, key);

            if (res.outcome == LockRequestOutcome::GRANTED)
            {
                string lockKind = (pending.type == TxnOpType::READ) ? "shared" : "exclusive";
                logLine(fout, txnId + " " + lockKind + " lock(" + pending.tableName + " " + to_string(pending.pageIndex) + ") granted");
                ctx.state = TxnState::ACTIVE;
                ctx.hasPending = false;
                executeReadOrWrite(pending, fout);
                if (workQueue != nullptr)
                    queueDelayedOpsAtFront(ctx, *workQueue);
                progressed = true;
                continue;
            }

            if (res.outcome == LockRequestOutcome::WAIT)
            {
                LockSnapshot snap = lockManager.snapshot(key);
                if (!res.conflictingOwners.empty())
                {
                    string heldType = lockTypeToString(snap.lockType);
                    if (heldType == "none")
                        heldType = "a";
                    logLine(fout, txnId + " still waits because " + res.conflictingOwners.front() + " holds " + heldType + " lock");
                }
                continue;
            }

            // Defensive: should not happen for a waiting transaction.
            logLine(fout, txnId + " dies (unexpected wait-die state)");
            logLine(fout, "ABORT " + txnId);
            logLine(fout, "ROLLBACK " + txnId);
            vector<string> unlocked = lockManager.releaseAll(txnId);
            logUnlocks(unlocked, txnId, fout);
            ctx.state = TxnState::ABORTED;
            ctx.hasPending = false;
            ctx.delayedOps.clear();
            if (abortedTxns != nullptr)
                abortedTxns->insert(txnId);
            progressed = true;
        }
    }
}

/**
 * @brief
 * SYNTAX: TRANSACTION <input_file>
 */
bool syntacticParseTRANSACTION()
{
    logger.log("syntacticParseTRANSACTION");
    if (tokenizedQuery.size() != 2)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    parsedQuery.queryType = TRANSACTION;
    parsedQuery.transactionFileName = tokenizedQuery[1];
    return true;
}

bool semanticParseTRANSACTION()
{
    logger.log("semanticParseTRANSACTION");
    string scheduleFilePath = "../data/" + parsedQuery.transactionFileName;
    if (!fileExists(scheduleFilePath))
    {
        cout << "SEMANTIC ERROR: File doesn't exist" << endl;
        return false;
    }

    vector<TxnOp> ops;
    unordered_map<string, vector<TxnOp>> opsPerTxn;
    if (!parseScheduleFile(scheduleFilePath, ops, opsPerTxn))
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    for (const auto &op : ops)
    {
        if (op.type != TxnOpType::READ && op.type != TxnOpType::WRITE)
            continue;

        if (!tableCatalogue.isTable(op.tableName))
        {
            cout << "SEMANTIC ERROR: Relation doesn't exist" << endl;
            return false;
        }
        Table *table = tableCatalogue.getTable(op.tableName);
        if (table == nullptr || op.pageIndex < 0 || (uint)op.pageIndex >= table->blockCount)
        {
            cout << "SEMANTIC ERROR: Page doesn't exist" << endl;
            return false;
        }
    }

    return true;
}

void executeTRANSACTION()
{
    logger.log("executeTRANSACTION");

    string scheduleFilePath = "../data/" + parsedQuery.transactionFileName;
    vector<TxnOp> scheduleOps;
    unordered_map<string, vector<TxnOp>> opsPerTxn;
    if (!parseScheduleFile(scheduleFilePath, scheduleOps, opsPerTxn))
        return;

    string outputFileName = outputFileNameForInput(parsedQuery.transactionFileName);
    string outputFilePath = "../data/" + outputFileName;
    ofstream fout(outputFilePath, ios::trunc);
    if (!fout.good())
        return;

    deque<TxnOp> workQueue(scheduleOps.begin(), scheduleOps.end());
    unordered_map<string, TxnContext> txns;
    unordered_set<string> abortedTxns;
    LockManager lockManager;
    long long nextTimestamp = 1;

    while (!workQueue.empty())
    {
        TxnOp op = workQueue.front();
        workQueue.pop_front();

        TxnContext &ctx = txns[op.txnId];

        if (op.type == TxnOpType::BEGIN)
        {
            logLine(fout, "BEGIN " + op.txnId);
            if (!ctx.hasTimestamp)
            {
                ctx.timestamp = nextTimestamp++;
                ctx.hasTimestamp = true;
                lockManager.registerTxn(op.txnId, ctx.timestamp);
            }
            ctx.state = TxnState::ACTIVE;
            ctx.hasPending = false;
            ctx.delayedOps.clear();
            continue;
        }

        if (!ctx.hasTimestamp)
        {
            // If BEGIN is missing for some reason, still assign a timestamp for ordering.
            ctx.timestamp = nextTimestamp++;
            ctx.hasTimestamp = true;
            lockManager.registerTxn(op.txnId, ctx.timestamp);
        }

        if (ctx.state == TxnState::COMMITTED)
            continue;

        if (ctx.state == TxnState::ABORTED)
        {
            continue;
        }

        if (ctx.state == TxnState::WAITING)
        {
            if (op.type == TxnOpType::COMMIT)
            {
                logLine(fout, op.txnId + " commit delayed because transaction is waiting");
            }
            ctx.delayedOps.push_back(op);
            continue;
        }

        if (op.type == TxnOpType::READ || op.type == TxnOpType::WRITE)
        {
            string lockKind = (op.type == TxnOpType::READ) ? "shared" : "exclusive";
            logLine(fout, op.txnId + " requests " + lockKind + " lock(" + op.tableName + " " + to_string(op.pageIndex) + ")");

            string key = pageKey(op.tableName, op.pageIndex);
            LockRequestResult res = (op.type == TxnOpType::READ) ? lockManager.requestShared(op.txnId, key) : lockManager.requestExclusive(op.txnId, key);

            if (res.outcome == LockRequestOutcome::GRANTED)
            {
                logLine(fout, "Lock granted");
                executeReadOrWrite(op, fout);
                continue;
            }

            if (res.outcome == LockRequestOutcome::WAIT)
            {
                logLine(fout, op.txnId + " waits (" + conflictingTxnText(res.conflictingOwners) + " holds conflicting lock, " + op.txnId + " is older so allowed to wait)");
                ctx.state = TxnState::WAITING;
                ctx.hasPending = true;
                ctx.pendingOp = op;
                continue;
            }

            logLine(fout, op.txnId + " dies (younger than " + waitDieBlockerText(op.txnId, res.conflictingOwners, txns) + ", cannot wait)");
            logLine(fout, "ABORT " + op.txnId);
            logLine(fout, "ROLLBACK " + op.txnId);
            vector<string> unlocked = lockManager.releaseAll(op.txnId);
            logUnlocks(unlocked, op.txnId, fout);
            ctx.state = TxnState::ABORTED;
            ctx.hasPending = false;
            ctx.delayedOps.clear();
            abortedTxns.insert(op.txnId);
            resumeWaitingTransactions(txns, lockManager, fout, &abortedTxns, &workQueue);
            continue;
        }

        if (op.type == TxnOpType::COMMIT)
        {
            logLine(fout, "COMMIT " + op.txnId);
            vector<string> unlocked = lockManager.releaseAll(op.txnId);
            logUnlocks(unlocked, op.txnId, fout);
            ctx.state = TxnState::COMMITTED;
            ctx.hasPending = false;
            ctx.delayedOps.clear();
            resumeWaitingTransactions(txns, lockManager, fout, &abortedTxns, &workQueue);
            continue;
        }
    }

    vector<string> restartOrder(abortedTxns.begin(), abortedTxns.end());
    sort(restartOrder.begin(), restartOrder.end(), [&](const string &a, const string &b) {
        return txns[a].timestamp < txns[b].timestamp;
    });

    for (const auto &txnId : restartOrder)
    {
        TxnContext &ctx = txns[txnId];
        logLine(fout, "Restart " + txnId);
        ctx.state = TxnState::ACTIVE;
        ctx.hasPending = false;
        ctx.delayedOps.clear();

        const auto &restartOps = opsPerTxn[txnId];
        for (const auto &op : restartOps)
        {
            if (op.type == TxnOpType::READ || op.type == TxnOpType::WRITE)
            {
                string lockKind = (op.type == TxnOpType::READ) ? "shared" : "exclusive";
                logLine(fout, op.txnId + " requests " + lockKind + " lock(" + op.tableName + " " + to_string(op.pageIndex) + ")");

                string key = pageKey(op.tableName, op.pageIndex);
                LockRequestResult res = (op.type == TxnOpType::READ) ? lockManager.requestShared(op.txnId, key) : lockManager.requestExclusive(op.txnId, key);

                if (res.outcome == LockRequestOutcome::GRANTED)
                {
                    logLine(fout, "Lock granted");
                    executeReadOrWrite(op, fout);
                    continue;
                }

                logLine(fout, op.txnId + " could not restart immediately");
                ctx.state = TxnState::WAITING;
                ctx.hasPending = true;
                ctx.pendingOp = op;
                break;
            }

            if (op.type == TxnOpType::COMMIT)
            {
                logLine(fout, "COMMIT " + op.txnId);
                vector<string> unlocked = lockManager.releaseAll(op.txnId);
                logUnlocks(unlocked, op.txnId, fout);
                ctx.state = TxnState::COMMITTED;
                ctx.hasPending = false;
                break;
            }
        }
    }

    fout.close();
    return;
}
