#ifndef TEST_SERIALIZATION_H
#define TEST_SERIALIZATION_H

void test_serialization();

// ============================================================================
// Methods used for serialization:

#include "helper.h"

#include <string>

const std::string TMP_FILE = "/tmp/serialization_tmp";

template<typename T>
void serialize(T& data)
{
    Helper::SaveToFile(data, TMP_FILE);
}

template<typename T>
void serialize(T* data)
{
    Helper::SaveToFile(data, TMP_FILE);
}

template<typename T>
void deserialize(T& data)
{
    Helper::LoadFromFile(TMP_FILE, data);
}

template<typename T>
void deserialize(T** data)
{
    Helper::LoadFromFile(TMP_FILE, data);
}

#endif // TEST_SERIALIZATION_H
