#include <gtest/gtest.h>
#include <gc.h>
#include <heap.h>
#include <chrono>


using namespace std;
using namespace std::chrono;

//Compute the size of user header and initial free space
static size_t initial_free_space() {
    return HEAP_SIZE - sizeof(Heap::node_t);
}

//Fixture to ensure heap is reset before each test
//Basically a setup call that runs when every test is executed 
class GCHeapTest : public ::testing::Test {
protected:
    Heap heap;
    GarbageCollector gc;

    void SetUp() override {
        heap.reset();
    }
};

//Ensures the heap is generated properly, checking if is has 1 free chunk equal to available mem
TEST_F(GCHeapTest, InitialFreeSpace) {
    size_t free_mem = heap.available_memory();
    ASSERT_EQ(free_mem, initial_free_space());
}

//Verifies malloc works correctly 
//Checks if avail mem == initial mem - (2 allocations + their headers)
TEST_F(GCHeapTest, AvailableMemoryAfterTwoAllocations) {
    void* ptr1 = gc.malloc(100, &heap);
    void* ptr2 = gc.malloc(100, &heap);
    
    //Create a cyclic reference
    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);

    size_t alloc_overhead = sizeof(GarbageCollector::allocation);
    size_t expected = initial_free_space() - 2 * (100 + alloc_overhead);

    size_t free_mem = heap.available_memory();
    ASSERT_EQ(free_mem, expected);
}

//Verifies RC collection works correctly
TEST_F(GCHeapTest, RCCollect) {
    std::vector<void*> ptrs;
    const int blockSize = 100;
    //Fully allocate heap 
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }
    //Verify heap and pointers are properly allocated
    ASSERT_FALSE(ptrs.empty()) << "Should allocate at least one block";

    //Remove references to allocated pointers for RC to collect
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    //Run garbage collection and verify memory state
    gc.rc_collect(&heap);

    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

//Verifies that reference counting doesn't work on cyclic references 
//Memory allocated and then roots deleted, count is still >0 so RC GC doesn't delete 
TEST_F(GCHeapTest, ReferenceCountingLeavesCycle) {
    //Allocate and create cycle
    void* ptr1 = gc.malloc(100, &heap);
    void* ptr2 = gc.malloc(100, &heap);
    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);
    //Remove root references
    gc.delete_reference(ptr1);
    gc.delete_reference(ptr2);

    gc.rc_collect(&heap);

    //RC should not reclaim cyclic garbage so free stays the same as after alloc
    size_t alloc_overhead = sizeof(GarbageCollector::allocation);
    size_t expected = initial_free_space() - 2 * (100 + alloc_overhead);
    ASSERT_EQ(heap.available_memory(), expected);
}

//Verifies that mark and sweep collects cyclic references
//Same setup as previous test but mark and sweep cleans up what RC couldn't collect
TEST_F(GCHeapTest, MarksweepReclaimsCycle) {
    //Allocate and create cycle
    void* ptr1 = gc.malloc(100, &heap);
    void* ptr2 = gc.malloc(100, &heap);
    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);
    //Remove root references
    gc.delete_reference(ptr1);
    gc.delete_reference(ptr2);

    gc.rc_collect(&heap);
    gc.ms_collect(&heap);

    //MS should reclaim all unreachable objects, resetting heap
    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

//This test verifies that coalescing works correctly 
//2 blocks are allocated then freed with no GC just to check coalescing is working properly
//Test leverages print_free_list() to verify correct components are freed 
TEST_F(GCHeapTest, TwoAdjacentFreesCoalesceIntoOne) {
    void* ptr1 = heap.my_malloc(128);
    void* ptr2 = heap.my_malloc(128);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    heap.my_free(ptr1);
    heap.my_free(ptr2);


    size_t expected_space = initial_free_space();
    EXPECT_EQ(heap.available_memory(), expected_space);

    ::testing::internal::CaptureStdout();
    heap.print_free_list();
    std::string dump = ::testing::internal::GetCapturedStdout();

    //Verify output format is "Free(<size>)\n" when there's just one node
    std::ostringstream oss;
    oss << "Free(" << expected_space << ")->\n";
    ASSERT_EQ(dump, oss.str());
}

//This is a stress test for the allocator and MS garbage collector  
//Max blocks of size 32 are allocated, then 1 max size block is allocated
TEST_F(GCHeapTest, StressTestFullHeap) {
    const size_t blockSize = 32;
    std::vector<void*> ptrs;

    //Allocate until no space left
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }
    ASSERT_FALSE(ptrs.empty())
        << "Should have allocated at least one block";
    //Shouldn't be able to fit another blockSize chunk
    ASSERT_LT(heap.available_memory(), blockSize);

    //Link each allocation to the next: ptrs[i-1] -> ptrs[i]
    for (size_t i = 1; i < ptrs.size(); ++i) {
        gc.add_nested_reference(ptrs[i-1], ptrs[i]);
    }

    //Drop all root refs and collect
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    //Take timestamp before/after MS runs and calculate
    auto t0 = high_resolution_clock::now();
    gc.ms_collect(&heap);
    auto t1 = high_resolution_clock::now();
    auto ms_time = duration_cast<microseconds>(t1 - t0).count();

    // After mark-sweep, there should be a completely free heap
    size_t actual = heap.available_memory();
    size_t expect = initial_free_space();
    std::cout << "After GC: actual=" << actual
              << " expected=" << expect << std::endl;
    ASSERT_EQ(actual, expect);

    //Grab one big block equal to the entire heap once everythings collected
    size_t max = initial_free_space() - sizeof(GarbageCollector::allocation);
    void* big = gc.malloc(max, &heap);
    ASSERT_NE(big, nullptr)
        << "Should be able to malloc the entire heap once it's free again";
    
    //Take timestamp before/after MS runs and calculate
    gc.delete_reference(big);
    auto t2 = high_resolution_clock::now();
    gc.ms_collect(&heap);
    auto t3 = high_resolution_clock::now();
    auto ms_time_big = duration_cast<microseconds>(t1 - t0).count();


    std::cout<< "32 bit collect took " << ms_time << "µs" << endl;
    std::cout<< "Big block collect took " << ms_time_big << "µs" << endl;


    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

//This test compares the execution time of RC and MS garbage collectors
//MS is given a list of cyclic references, RC is given standard allocations
TEST_F(GCHeapTest, Efficiency_StressTest_RCvsMS) {
    const size_t blockSize = 32;
    size_t rc_time, ms_time;
    
    {
    std::vector<void*> ptrs;

    //Fill the heap
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }
    ASSERT_FALSE(ptrs.empty()) << "Should allocate at least one block";

    //Drop all roots
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    //Time the RC pass on the unchained blocks
    auto t0 = std::chrono::high_resolution_clock::now();
    gc.rc_collect(&heap);
    auto t1 = std::chrono::high_resolution_clock::now();
    rc_time = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    ASSERT_EQ(heap.available_memory(), initial_free_space());
    }
    heap.reset();
    {
    std::vector<void*> ptrs;

    //Fill the heap
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }
    ASSERT_FALSE(ptrs.empty()) << "Should allocate at least one block before exhaustion";
    ASSERT_LT(heap.available_memory(), blockSize)
        << "Remaining free should be less than one block";

    //Chain the blocks together 
    for (size_t i = 1; i < ptrs.size(); ++i) {
        gc.add_nested_reference(ptrs[i-1], ptrs[i]);
    }
    //Drop all roots
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    //Time the MS pass on the chained blocks
    auto t2 = std::chrono::high_resolution_clock::now();
    gc.ms_collect(&heap);
    auto t3 = std::chrono::high_resolution_clock::now();
    ms_time = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

    ASSERT_EQ(heap.available_memory(), initial_free_space());
    }

    //Check efficiency, RC should be faster than MS
    EXPECT_LT(rc_time, ms_time)
        << "rc_collect took " << rc_time << "µs; ms_collect took " << ms_time << "µs";

    std::cout << "rc_collect: " << rc_time << "µs,    ms_collect: " << ms_time << "µs\n";
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
