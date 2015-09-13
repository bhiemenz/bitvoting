#ifndef TEST_BLOCKCHAIN_H
#define TEST_BLOCKCHAIN_H

#include "transaction.h"

void test_blockchain();

// used by database_store tests
void random_transaction_election(Transaction** out);

#endif // TEST_BLOCKCHAIN_H
