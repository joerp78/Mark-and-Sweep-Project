#include <gtest/gtest.h>
#include <gc.h>
#include <heap.h>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Compute the initial free space after subtracting metadata for the first free block
static size_t initial_free_space() {
    return HEAP_SIZE - sizeof(Heap::node_t);
}

// Test fixture to reset heap before each test
class GCHeapTest : public ::testing::Test {
protected:
    Heap heap;
    GarbageCollector gc;

    void SetUp() override {
        heap.reset();  // reset the heap to a clean state
    }
};

// Verifies heap starts empty (aside from metadata)
TEST_F(GCHeapTest, Initial_Free_Space) {
    size_t free_mem = heap.available_memory();
    ASSERT_EQ(free_mem, initial_free_space());
}

// Allocates two blocks, connects them in a cycle, and verifies expected space is used
TEST_F(GCHeapTest, Available_Memory_After_Two_Allocations) {
    void* ptr1 = gc.malloc(100, &heap);
    void* ptr2 = gc.malloc(100, &heap);

    // Establish mutual references to simulate a cycle
    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);

    // Calculate total expected usage including allocation metadata
    size_t alloc_overhead = sizeof(GarbageCollector::allocation);
    size_t expected = initial_free_space() - 2 * (100 + alloc_overhead);

    size_t free_mem = heap.available_memory();
    ASSERT_EQ(free_mem, expected);
}

// Allocates until the heap is full and frees all; RC should reclaim all memory
TEST_F(GCHeapTest, RC_Collect) {
    std::vector<void*> ptrs;
    const int blockSize = 100;

    // Allocate as much as possible
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }

    ASSERT_FALSE(ptrs.empty()) << "Should allocate at least one block";

    // Delete all external references
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    // Perform reference counting garbage collection
    gc.rc_collect(&heap);

    // All memory should be reclaimed
    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

// Allocates until the heap is full and frees all; MS should reclaim all memory
TEST_F(GCHeapTest, MS_Collect) {
    std::vector<void*> ptrs;
    const int blockSize = 100;

    // Allocate as many blocks as possible
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }

    ASSERT_FALSE(ptrs.empty()) << "Should allocate at least one block";

    // Remove all external references
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    // Run mark-and-sweep collection
    gc.ms_collect(&heap);

    // Check that all memory has been reclaimed
    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

// Creates a cycle and tests that RC does not collect it
TEST_F(GCHeapTest, Reference_Counting_Leaves_Cycle) {
    void* ptr1 = gc.malloc(100, &heap);
    void* ptr2 = gc.malloc(100, &heap);

    // Introduce a circular reference
    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);

    // Delete all external references
    gc.delete_reference(ptr1);
    gc.delete_reference(ptr2);

    // Run reference counting GC
    gc.rc_collect(&heap);

    // The cycle is unreachable but still present due to RC limitation
    size_t alloc_overhead = sizeof(GarbageCollector::allocation);
    size_t expected = initial_free_space() - 2 * (100 + alloc_overhead);
    ASSERT_EQ(heap.available_memory(), expected);
}

// Uses MS to successfully reclaim cyclic memory unreachable by RC
TEST_F(GCHeapTest, Marksweep_Reclaims_Cycle) {
    void* ptr1 = gc.malloc(100, &heap);
    void* ptr2 = gc.malloc(100, &heap);

    // Circular reference setup
    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);

    // Remove external references
    gc.delete_reference(ptr1);
    gc.delete_reference(ptr2);

    // RC fails to collect the cycle
    gc.rc_collect(&heap);

    // MS should successfully reclaim the cycle
    gc.ms_collect(&heap);
    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

// Ensures freed adjacent blocks coalesce into one larger free chunk
TEST_F(GCHeapTest, Two_Adjacent_Frees_Coalesce) {
    void* ptr1 = heap.my_malloc(128);
    void* ptr2 = heap.my_malloc(128);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // Free the two adjacent blocks
    heap.my_free(ptr1);
    heap.my_free(ptr2);

    // Expect entire heap (except metadata) to be free again
    size_t expected_space = initial_free_space();
    EXPECT_EQ(heap.available_memory(), expected_space);

    // Dump free list and check it has coalesced into one chunk
    ::testing::internal::CaptureStdout();
    heap.print_free_list();
    std::string dump = ::testing::internal::GetCapturedStdout();

    std::ostringstream oss;
    oss << "Free(" << expected_space << ")->\n";
    ASSERT_EQ(dump, oss.str());
}

// Fills heap with small blocks and times MS reclaiming them
TEST_F(GCHeapTest, Stress_Test_Full_Heap) {
    const size_t blockSize = 32;
    std::vector<void*> ptrs;

    // Fill the heap with small allocations
    while (true) {
        void* p = gc.malloc(blockSize, &heap);
        if (!p) break;
        ptrs.push_back(p);
    }

    ASSERT_FALSE(ptrs.empty());
    ASSERT_LT(heap.available_memory(), blockSize);

    // Link blocks in a chain (non-cyclic)
    for (size_t i = 1; i < ptrs.size(); ++i) {
        gc.add_nested_reference(ptrs[i-1], ptrs[i]);
    }

    // Remove all roots
    for (void* p : ptrs) {
        gc.delete_reference(p);
    }

    // Measure MS collection time
    auto t0 = high_resolution_clock::now();
    gc.ms_collect(&heap);
    auto t1 = high_resolution_clock::now();
    auto ms_time = duration_cast<microseconds>(t1 - t0).count();

    ASSERT_EQ(heap.available_memory(), initial_free_space());

    // Test that a large block can now be allocated
    size_t max = initial_free_space() - sizeof(GarbageCollector::allocation);
    void* big = gc.malloc(max, &heap);
    ASSERT_NE(big, nullptr);

    // Reclaim it and re-check
    gc.delete_reference(big);
    auto t2 = high_resolution_clock::now();
    gc.ms_collect(&heap);
    auto t3 = high_resolution_clock::now();
    auto ms_time_big = duration_cast<microseconds>(t3 - t2).count();

    cout << "32-byte block MS collect: " << ms_time << "µs\n";
    cout << "Big block MS collect: " << ms_time_big << "µs\n";

    ASSERT_EQ(heap.available_memory(), initial_free_space());
}

// Stress test comparing efficiency of RC vs MS for many tiny, non-cyclic blocks
TEST_F(GCHeapTest, Efficiency_Stress_Test) {
    const size_t blockSize = 1;
    double rc_time, ms_time;

    // --- Reference Counting Phase ---
    {
        std::vector<void*> ptrs;
        double num_objects = 0;

        // Fill heap with small allocations
        while (true) {
            void* p = gc.malloc(blockSize, &heap);
            if (!p) break;
            ptrs.push_back(p);
            num_objects++;
        }

        for (void* p : ptrs) {
            gc.delete_reference(p);
        }

        auto t0 = high_resolution_clock::now();
        gc.rc_collect(&heap);
        auto t1 = high_resolution_clock::now();
        rc_time = duration_cast<microseconds>(t1 - t0).count();

        cout << "RC Efficiency: " 
             << rc_time / num_objects + (70.0 / heap.available_memory()) << endl;
    }

    heap.reset();

    // --- Mark and Sweep Phase ---
    {
        std::vector<void*> ptrs;
        double num_objects = 0;

        while (true) {
            void* p = gc.malloc(blockSize, &heap);
            if (!p) break;
            ptrs.push_back(p);
            num_objects++;
        }

        for (void* p : ptrs) {
            gc.delete_reference(p);
        }

        auto t0 = high_resolution_clock::now();
        gc.ms_collect(&heap);
        auto t1 = high_resolution_clock::now();
        ms_time = duration_cast<microseconds>(t1 - t0).count();

        cout << "MS Efficiency: " 
             << ms_time / num_objects + (70.0 / heap.available_memory()) << endl;
    }
}

// Same stress test as above, but introduces cycles to expose RC limitations
TEST_F(GCHeapTest, Efficiency_Stress_Test_Cyclic) {
    const size_t blockSize = 100;
    double rc_time, ms_time;

    // --- Reference Counting Phase ---
    {
        std::vector<void*> ptrs;
        double num_objects = 0;

        while (true) {
            void* p = gc.malloc(blockSize, &heap);
            if (!p) break;
            ptrs.push_back(p);
            num_objects++;
        }

        // Create cycles between every pair
        for (size_t i = 1; i < ptrs.size(); ++i) {
            gc.add_nested_reference(ptrs[i-1], ptrs[i]);
            gc.add_nested_reference(ptrs[i], ptrs[i-1]);
        }

        for (void* p : ptrs) {
            gc.delete_reference(p);
        }

        auto t0 = high_resolution_clock::now();
        gc.rc_collect(&heap);
        auto t1 = high_resolution_clock::now();
        rc_time = duration_cast<microseconds>(t1 - t0).count();

        cout << "RC (cyclic) Efficiency: " 
             << rc_time / num_objects + (70.0 / heap.available_memory()) << endl;
    }

    heap.reset();

    // --- Mark and Sweep Phase ---
    {
        std::vector<void*> ptrs;
        double num_objects = 0;

        while (true) {
            void* p = gc.malloc(blockSize, &heap);
            if (!p) break;
            ptrs.push_back(p);
            num_objects++;
        }

        for (size_t i = 1; i < ptrs.size(); ++i) {
            gc.add_nested_reference(ptrs[i-1], ptrs[i]);
            gc.add_nested_reference(ptrs[i], ptrs[i-1]);
        }

        for (void* p : ptrs) {
            gc.delete_reference(p);
        }

        auto t0 = high_resolution_clock::now();
        gc.ms_collect(&heap);
        auto t1 = high_resolution_clock::now();
        ms_time = duration_cast<microseconds>(t1 - t0).count();

        cout << "MS (cyclic) Efficiency: " 
             << ms_time / num_objects + (70.0 / heap.available_memory()) << endl;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}