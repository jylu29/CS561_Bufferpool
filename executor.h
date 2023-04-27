#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <iostream>
#include <vector>
#include <string>
#include "parameter.h"

using namespace std;

namespace bufmanager
{

class Buffer
{
public:
    static Buffer *getBufferInstance(Simulation_Environment *_env);
    struct Page {
        int pageId;
        int timestamp;
        bool dirty;
        bool cold;
        string content;
    };
    static int global_clock;

    vector<Page> buffer_pool;
    int search(int pageId);
    int read(int pageId, int offset, int algorithm, bool is_write_operation);
    int write(int pageId, int offset, const string new_entry, int algorithm);
    int unpin(int pageId);

    int LRU();
    int CFLRU_using_min_heap();
    int CFLRU_using_array();
    int LRUWSR();
    int printBuffer();
    int printStats();

    static long max_buffer_size;
    static int buffer_hit;
    static int buffer_miss;
    static int read_io;
    static int write_io;

private:
    Buffer(Simulation_Environment *_env);

    static Buffer *buffer_instance;
};

class WorkloadExecutor
{
public:
    int search(Buffer* buffer_instance, int pageId);
    int read(Buffer* buffer_instance, int pageId, int offset, int algorithm, bool simulation_on_disk, bool is_write_operation);
    int write(Buffer* buffer_instance, int pageId, int offset, string & new_entry, int algorithm, bool simulation_on_disk);
    int unpin(Buffer* buffer_instance, int pageId);

};

} // namespace bufmanager

#endif // EXECUTOR_H