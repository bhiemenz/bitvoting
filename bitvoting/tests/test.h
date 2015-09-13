#ifndef TEST_H
#define TEST_H

#include "tests/test_comparison.h"
#include "tests/test_serialization.h"
#include "tests/test_blockchain.h"
#include "tests/test_paillier.h"
#include "tests/test_database_store.h"

void test_start()
{
    test_comparison();
    test_serialization();
    test_blockchain();
    test_pailler();
    test_database_store();

    // call others too...
}

#endif // TEST_H
