#include <gtest/gtest.h>

#include "test_loops.h"

int call_linked()
{
    Node n1;
    Node n2;
    Node n3;

    n1.next = &n2;
    n1.value = 1;

    n2.next = &n3;
    n2.value = 2;

    n3.next = nullptr;
    n3.value = 3;

    Node *n = &n1;
    return linked(n);
}

TEST(HelixResultTest, Linked)
{
    EXPECT_EQ(6, call_linked());
}

TEST(HelixResultTest, Multiloop)
{
    EXPECT_EQ(210, multiloop_1(10, 100, 100));
    EXPECT_EQ(310, multiloop_2(10, 100, 100));
}

TEST(HelixResultTest, Simple)
{
    EXPECT_EQ(5105, simple_1(100, 10));
    EXPECT_EQ(477, simple_2(100, 10));
}