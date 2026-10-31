#include <gtest/gtest.h>
#include "rainman/formats/CRgdHashTable.h"
#include <vector>
#include <cstring>

extern "C" unsigned long hash(unsigned char *k, unsigned long length, unsigned long initval);

class RgdHashTableTest : public ::testing::Test {
protected:
    CRgdHashTable table;
};

TEST_F(RgdHashTableTest, ConstructorPrePopulatesNumericKeys) {
    // Constructor pre-populates keys "1" through "9999"
    const char* val = table.HashToValue(table.ValueToHash("42"));
    ASSERT_NE(val, nullptr);
    EXPECT_STREQ(val, "42");
}

TEST_F(RgdHashTableTest, ValueToHashAndHashToValueRoundTrip) {
    unsigned long h = table.ValueToHash("weapon_damage");
    const char* resolved = table.HashToValue(h);
    ASSERT_NE(resolved, nullptr);
    EXPECT_STREQ(resolved, "weapon_damage");
}

TEST_F(RgdHashTableTest, ValueToHashRegistersUnknownString) {
    // A non-numeric string not already in the table gets auto-registered
    unsigned long h = table.ValueToHash("my_custom_key");
    const char* resolved = table.HashToValue(h);
    ASSERT_NE(resolved, nullptr);
    EXPECT_STREQ(resolved, "my_custom_key");
}

TEST_F(RgdHashTableTest, ValueToHashStaticConsistentWithValueToHash) {
    unsigned long h1 = table.ValueToHash("test_value");
    unsigned long h2 = CRgdHashTable::ValueToHashStatic("test_value");
    EXPECT_EQ(h1, h2);
}

TEST_F(RgdHashTableTest, ValueToHashStaticWithLengthConsistent) {
    const char* val = "test_value";
    unsigned long h1 = CRgdHashTable::ValueToHashStatic(val);
    unsigned long h2 = CRgdHashTable::ValueToHashStatic(val, strlen(val));
    EXPECT_EQ(h1, h2);
}

TEST_F(RgdHashTableTest, HexStringPassthrough) {
    // A 10-char hex string "0xDEADBEEF" should be parsed as a literal hash
    unsigned long h = table.ValueToHash("0xDEADBEEF");
    EXPECT_EQ(h, 0xDEADBEEFUL);
}

TEST_F(RgdHashTableTest, HexStringPassthroughLowercase) {
    unsigned long h = table.ValueToHash("0xdeadbeef");
    EXPECT_EQ(h, 0xDEADBEEFUL);
}

TEST_F(RgdHashTableTest, HexStringStaticPassthrough) {
    unsigned long h = CRgdHashTable::ValueToHashStatic("0xABCD1234");
    EXPECT_EQ(h, 0xABCD1234UL);
}

TEST_F(RgdHashTableTest, FillUnknownListTracksUnresolvedHashes) {
    // Look up a hash that doesn't exist — it should be tracked as unknown
    unsigned long fakeHash = 0x12345678UL;
    const char* result = table.HashToValue(fakeHash);
    EXPECT_EQ(result, nullptr);

    std::vector<unsigned long> unknowns;
    table.FillUnknownList(unknowns);
    bool found = false;
    for (unsigned long h : unknowns) {
        if (h == fakeHash) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(RgdHashTableTest, ValueToHashNumericNotCustom) {
    // Numeric-only strings (like "42") are pre-populated and not flagged custom
    unsigned long h = table.ValueToHash("42");
    const char* val = table.HashToValue(h);
    ASSERT_NE(val, nullptr);
    EXPECT_STREQ(val, "42");
}

TEST_F(RgdHashTableTest, DifferentStringsDifferentHashes) {
    unsigned long h1 = table.ValueToHash("alpha");
    unsigned long h2 = table.ValueToHash("beta");
    EXPECT_NE(h1, h2);
}

TEST_F(RgdHashTableTest, SameStringProducesSameHash) {
    unsigned long h1 = table.ValueToHash("consistent_key");
    unsigned long h2 = table.ValueToHash("consistent_key");
    EXPECT_EQ(h1, h2);
}

TEST_F(RgdHashTableTest, UnknownHashReturnsNullptr) {
    // A hash never inserted should return nullptr
    const char* result = table.HashToValue(0xFFFFFFFFUL);
    EXPECT_EQ(result, nullptr);
}

TEST_F(RgdHashTableTest, NewClearsAllEntries) {
    table.ValueToHash("some_key_before_new");
    table.New();
    // After New(), previously registered keys should be gone
    const char* result = table.HashToValue(table.ValueToHashStatic("some_key_before_new"));
    EXPECT_EQ(result, nullptr);
}

TEST_F(RgdHashTableTest, HexStringTooShortHashes) {
    // "0xABCD" is only 6 chars, not 10 — should be hashed, not parsed as hex literal
    unsigned long h = table.ValueToHash("0xABCD");
    // It should NOT equal the literal value 0x0000ABCD
    EXPECT_NE(h, 0x0000ABCDUL);
}

TEST_F(RgdHashTableTest, HexStringWithInvalidCharHashes) {
    // "0xGGGGGGGG" has invalid hex chars — should fall through to hash
    unsigned long h = table.ValueToHash("0xGGGGGGGG");
    EXPECT_NE(h, 0UL); // should produce some hash, not zero
}

TEST_F(RgdHashTableTest, FillUnknownListDoesNotDuplicate) {
    table.HashToValue(0xAAAAAAAAUL);
    table.HashToValue(0xBBBBBBBBUL);
    std::vector<unsigned long> unknowns;
    table.FillUnknownList(unknowns);
    int countA = 0, countB = 0;
    for (unsigned long h : unknowns) {
        if (h == 0xAAAAAAAAUL) ++countA;
        if (h == 0xBBBBBBBBUL) ++countB;
    }
    EXPECT_EQ(countA, 1);
    EXPECT_EQ(countB, 1);
}

namespace {
unsigned long HashBytes(const void *data, unsigned long length, unsigned long initval = 0) {
    return hash(const_cast<unsigned char *>(static_cast<const unsigned char *>(data)), length, initval);
}
} // namespace

// Expected values were generated by the original Windows implementation (32-bit unsigned long)
// so that all platforms stay compatible with hashes stored in game files.
TEST(RgdHashFunctionTest, KnownFieldNames) {
    EXPECT_EQ(HashBytes("", 0), 0xBD49D10DUL);
    EXPECT_EQ(HashBytes("GameData", 8), 0xF693C657UL);
    EXPECT_EQ(HashBytes("weapon_damage", 13), 0x65C3A374UL);
    EXPECT_EQ(HashBytes("health_ext", 10), 0x90255BA9UL);
}

TEST(RgdHashFunctionTest, BlockBoundaryLengths) {
    EXPECT_EQ(HashBytes("abcdefghijk", 11), 0xE52B8E4CUL);
    EXPECT_EQ(HashBytes("abcdefghijkl", 12), 0x0B1B3EA5UL);
    EXPECT_EQ(HashBytes("abcdefghijklm", 13), 0x3122B031UL);
}

TEST(RgdHashFunctionTest, BytesAbove127) {
    const unsigned char data[] = {0x80, 0xFF, 0xC3, 0xA9, 0x7F, 0x00, 0xFE, 0x81, 0x90, 0xAB, 0xCD, 0xEF, 0xF0};
    EXPECT_EQ(HashBytes(data, 13), 0x0D2B0D58UL);
    EXPECT_EQ(HashBytes(data, 12), 0x160E8ABBUL);
    EXPECT_EQ(HashBytes(data, 11), 0x285210BEUL);
    EXPECT_EQ(HashBytes(data, 13, 0xFFFFFFFFUL), 0x63D69144UL);
}

TEST(RgdHashFunctionTest, NonZeroInitialValue) {
    EXPECT_EQ(HashBytes("GameData", 8, 0x12345678UL), 0x1511FE6AUL);
    EXPECT_EQ(HashBytes("weapon_damage", 13, 0xDEADBEEFUL), 0x41740279UL);
    EXPECT_EQ(HashBytes("health_ext", 10, 1), 0x8A713325UL);
}