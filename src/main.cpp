#include <iostream>
#include <unordered_map>
#include <string>
#include <gc.h>
#include <heap.h>
#include <limits>
#include <sstream>

using namespace std;

void print_help() {
    cout << "\nAvailable commands:\n"
         << "  alloc <name> <size>        - Allocate object\n"
         << "  ref <from> [to]            - Add external (or nested if 'to' is given) reference\n"
         << "  delref <name>              - Delete external reference\n"
         << "  rc                         - Run reference counting GC\n"
         << "  ms                         - Run mark-and-sweep GC\n"
         << "  mem                        - Show available memory\n"
         << "  list                       - List current objects\n"
         << "  help                       - Show this help menu\n"
         << "  exit                       - Quit the program\n";
}

int main() {
    cout << "==== Interactive Garbage Collection Simulator ====" << endl;
    print_help();

    Heap heap;
    GarbageCollector gc;

    unordered_map<string, void*> objects;

    string command;
    while (true) {

        cout << "\n> ";
        cin >> command;

        if (command == "alloc") {
            string name;
            size_t size;
            cin >> name >> size;
        
            if (cin.fail()) {
                cout << "Invalid input. Usage: alloc <name> <size>" << endl;
                cin.clear();               // clear the fail state
                cin.ignore(10000, '\n');   // discard the rest of the line
                continue;
            }
        
            if (objects.find(name) != objects.end()) {
                cout << "Objects must have unique names." << endl;
            } else {
                void* ptr = gc.malloc(size, &heap);
                if (ptr) {
                    objects[name] = ptr;
                    cout << "Allocated '" << name << "' with " << size << " bytes." << endl;
                } else {
                    cout << "Allocation failed." << endl;
                }
            }

        } else if (command == "ref") {
            string line;
            getline(cin >> ws, line); // read the rest of the line, trimming leading whitespace
            istringstream iss(line);
            string from, to;
            iss >> from >> to;

            if (!from.empty() && to.empty()) {
                if (objects.count(from)) {
                    gc.add_reference(objects[from]);
                    cout << "Added external reference to '" << from << "'." << endl;
                } else {
                    cout << "Unknown object: " << from << endl;
                }
            } else if (!from.empty() && !to.empty()) {
                if (objects.count(from) && objects.count(to)) {
                    gc.add_nested_reference(objects[from], objects[to]);
                    cout << "Added nested reference: " << from << " → " << to << endl;
                } else {
                    cout << "Unknown object names." << endl;
                }
            } else {
                cout << "Usage: ref <from> [to]" << endl;
            }

        } else if (command == "delref") {
            string name;
            cin >> name;
            if (objects.count(name)) {
                gc.delete_reference(objects[name]);
                cout << "Deleted external reference to '" << name << "'" << endl;
            } else {
                cout << "Unknown object: " << name << endl;
            }

        } else if (command == "rc") {
            list<void*> deleted_ptrs = gc.rc_collect(&heap);
            for (auto ptr : deleted_ptrs) {
                for (auto it = objects.begin(); it != objects.end(); ) {
                    if (it->second == ptr) {
                        it = objects.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            cout << "Reference counting GC completed." << endl;

        } else if (command == "ms") {
            list<void*> deleted_ptrs = gc.ms_collect(&heap);
            for (auto ptr : deleted_ptrs) {
                for (auto it = objects.begin(); it != objects.end(); ) {
                    if (it->second == ptr) {
                        it = objects.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            cout << "Mark and sweep GC completed." << endl;

        } else if (command == "mem") {
            cout << "Available memory: " << heap.available_memory() << " bytes." << endl;

        } else if (command == "list") {
            cout << "Tracked objects:" << endl;
            for (const auto& [name, ptr] : objects) {
                cout << "  " << name << ": " << ptr << endl;
            }

        } else if (command == "exit") {
            cout << "Exiting garbage collection simulator." << endl;
            break;
        
        } else if (command == "help") {
            print_help();

        } else {
            cout << "Unknown command. Try again." << endl;
            // Flush the rest of the line to prevent input errors
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    return 0;
}
