#include <gtest/gtest.h>
#include <my_malloc.h>

using namespace std;

TEST(MallocTest, InitAndCheckAvailableMemory) {
  reset_heap();
  size_t size = available_memory();
  ASSERT_EQ(size, HEAP_SIZE - sizeof(node_t));
}

TEST(MallocTest, CheckNumberOfFreeNodes) {
  reset_heap();
  int count = number_of_free_nodes();
  ASSERT_EQ(count, 1);
}

TEST(MallocTest, SimpleMallocCall) {
  reset_heap();
  void *p = my_malloc(100);
  ASSERT_TRUE(p != NULL);
}


int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
