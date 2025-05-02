#include <stdio.h>
#include <gc.h>
#include <heap.h>

int main() {
    Heap heap;
    GarbageCollector gc;

    void *ptr1 = gc.malloc(100, &heap);
    void *ptr2 = gc.malloc(100, &heap);

    gc.add_nested_reference(ptr1, ptr2);
    gc.add_nested_reference(ptr2, ptr1);

    cout << "Free space: " << heap.available_memory() << endl;

    gc.delete_reference(ptr1);
    gc.delete_reference(ptr2);

    gc.rc_collect(&heap);
    cout << "Free space: " << heap.available_memory() << endl;

    gc.ms_collect(&heap);
    cout << "Free space: " << heap.available_memory() << endl;
    
    return 0;
}