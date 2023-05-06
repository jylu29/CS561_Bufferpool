#include <iostream>
#include <cmath>
#include <sys/time.h>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <queue>
#include <functional>

#include "parameter.h"
#include "executor.h"

using namespace std;
using namespace bufmanager;

const int LRU_ALGORITHM = 0;
const int CFLRU_ALGORITHM_MIN_HEAP = 1;
const int CFLRU_ALGORITHM_ARRAY = 2;
const int LRUWSR_ALGORITHM = 3;

Buffer *Buffer::buffer_instance;
long Buffer::max_buffer_size = 0;
int Buffer::buffer_hit = 0;
int Buffer::buffer_miss = 0;
int Buffer::read_io = 0;
int Buffer::write_io = 0;
int Buffer::global_clock = 0;

Buffer::Buffer(Simulation_Environment *_env)
{
  max_buffer_size = _env->buffer_size_in_pages;
  buffer_instance = this;
  global_clock = 0;
}

// The below 'search' function searches for a particular pageId from the bufferpool
int WorkloadExecutor::search(Buffer* buffer_instance, int pageId)
{
  for (int i = 0; i < buffer_instance->buffer_pool.size(); ++i) {
    if (buffer_instance->buffer_pool[i].pageId == pageId) {
      return i;
    }
  }
  return -1;
}

// The below 'getContentByPageId' function gets the content of the passed in 'pageId' from disk (rawdata_database.dat)
string getContentByPageId(int pageId)
{
  long offset = static_cast<long>(pageId) * PAGE_SIZE; // Calculating offset

  ifstream file("rawdata_database.dat", ios::binary);

  if (file.is_open())
  {
    file.seekg(offset, ios::beg); // Seek to the corresponding position in the file

    char buffer[PAGE_SIZE];
    file.read(buffer, PAGE_SIZE); // Read the page content of size 'PAGE_SIZE' into the buffer

    file.close();

    return string(buffer, PAGE_SIZE); // Convert the buffer into a string and return
  }
  else
  {
    cerr << "Unable to open the rawdata_database.dat file" << endl;
  }

  // If the pageId is not found or the file cannot be opened, return an empty string
  return "";
}

// The below function writes back to disk the contents of the evicted dirty page
void write_back_to_disk(int pageId, string & content) {
    long offset = static_cast<long>(pageId) * PAGE_SIZE;

    fstream file("rawdata_database.dat", fstream ::binary | fstream::out | fstream::in);

    if (file.is_open())
    {
        file.seekp(offset); // Seek to the corresponding position in the file

        const char *buffer = content.c_str();
        file.write(buffer, PAGE_SIZE); // Write the page content to the disk

        file.close();
    }
    else
    {
        cerr << "Unable to open the rawdata_database.dat file" << endl;
    }
}

// The below function performs the read operation
int WorkloadExecutor::read(Buffer* buffer_instance, int pageId, int offset, int algorithm, bool simulation_on_disk, bool is_write_operation=false)
{
  int index = search(buffer_instance, pageId);

  if (index != -1) { // If index != - 1, it's a buffer hit meaning the page is in the buffer
    ++Buffer::buffer_hit;
    buffer_instance->buffer_pool[index].timestamp = Buffer::global_clock; // Update the timestamp of the page found in the buffer
    buffer_instance->buffer_pool[index].cold = false; // Update cold attribute of the page
  } else {
    ++Buffer::buffer_miss;
    if (!is_write_operation) {
      ++Buffer::read_io;
    }
    string content;
    if (simulation_on_disk) {
        content = getContentByPageId(pageId);
    }
    if (buffer_instance->buffer_pool.size() < Buffer::max_buffer_size) {
      buffer_instance->buffer_pool.push_back({pageId, Buffer::global_clock, false, false, content}); // Assuming the newly added page is not dirty
      index = buffer_instance->buffer_pool.size() - 1;
    } else { // We call any of the page eviction algorithms.
      if (algorithm == LRU_ALGORITHM) {
        index = buffer_instance->LRU();
        }
      else if (algorithm == CFLRU_ALGORITHM_MIN_HEAP) {
        index = buffer_instance->CFLRU_using_min_heap();
      }
      else if (algorithm == CFLRU_ALGORITHM_ARRAY) {
        index = buffer_instance->CFLRU_using_array();
      }
      else if (algorithm == LRUWSR_ALGORITHM) {
        index = buffer_instance->LRUWSR();
      }
      // Need to write back to disk if it is a dirty page and simulation on disk
      if (buffer_instance->buffer_pool[index].dirty && simulation_on_disk) {
          write_back_to_disk(buffer_instance->buffer_pool[index].pageId, buffer_instance->buffer_pool[index].content);
      }
      if (buffer_instance->buffer_pool[index].dirty) {
          ++Buffer::write_io;
      }

      buffer_instance->buffer_pool[index] = {pageId, Buffer::global_clock, false, false, content};
    }
  }

  if ((simulation_on_disk) && (!is_write_operation)) {
      // Performing the read operation
      Simulation_Environment *_env = Simulation_Environment::getInstance();

      int entry_position = offset * _env->entry_size;

      // Access the content member of the Page struct directly
      std::string &page_content = buffer_instance->buffer_pool[index].content;
      std::string content_read(page_content.begin() + entry_position,
                               page_content.begin() + entry_position + _env->entry_size);
  }
  ++Buffer::global_clock; // Increment the global clock after each read operation, whether it's a hit or a miss
  return index;
}

// The below function performs a write operation
int WorkloadExecutor::write(Buffer* buffer_instance, int pageId, int offset, string & new_entry, int algorithm, bool simulation_on_disk)
{
  int index = read(buffer_instance, pageId, offset, algorithm, simulation_on_disk, true);

  // Performing write operation
  if (simulation_on_disk) {
      Simulation_Environment *_env = Simulation_Environment::getInstance();
      int entry_position = offset * _env->entry_size;
      string &page_buffer = buffer_instance->buffer_pool[index].content;

      // Make sure the new_entry size does not exceed the entry size allowed
      if (new_entry.size() > _env->entry_size) {
          std::cerr << "The new entry size exceeds the allowed entry size." << std::endl;
          return -1;
      }

      // Update the buffer content with the new entry
      page_buffer.replace(entry_position, _env->entry_size, new_entry);
  }
  buffer_instance->buffer_pool[index].dirty = true; // Mark the page as dirty after the write operation
//  ++Buffer::write_io;
  return index;
}

// The below function implements the 'Least Recently Used' page eviction algorithm
int Buffer::LRU()
{
  int index = 0;
  int min_timestamp = buffer_pool[0].timestamp;

// We return the index of the minimum time stamp
  for (int i = 1; i < buffer_pool.size(); ++i) {
    if (buffer_pool[i].timestamp < min_timestamp) {
      min_timestamp = buffer_pool[i].timestamp;
      index = i;
    }
  }

  return index;
}

// The below function implements CFLRU (Clean-First LRU) using a min heap
int Buffer::CFLRU_using_min_heap() {
  int index = -1;
  int clean_first_region_size = max_buffer_size / 3; // Calculating clean_first_region_size
  vector<int> clean_first_indices;
  vector<int> working_region_indices;

  // Form a min heap of the time stamps and the indexes of the pages in buffer pool
  priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> min_heap;
  for (int i = 0; i < buffer_pool.size(); ++i) {
    min_heap.push({buffer_pool[i].timestamp, i});
  }

  // Populate the clean_first_indices vector
  for (int i = 0; i < clean_first_region_size; ++i) {
    clean_first_indices.push_back(min_heap.top().second);
    min_heap.pop();
  }

  // Populate the working_region_indices_vector
  while (!min_heap.empty()) {
    working_region_indices.push_back(min_heap.top().second);
    min_heap.pop();
  }

  // Find the first clean page in the clean-first region that is least recently accessed
  for (int i : clean_first_indices) {
    if (buffer_pool[i].dirty == false) {
      index = i;
      break;
    }
  }

  // If there are no clean pages in the clean-first region, find the least recently used page in the overall buffer pool
  if (index == -1) {
    index = clean_first_indices[0];
  }

  return index;
}

// The below function implements CFLRU (Clean-First LRU) using an array
int Buffer::CFLRU_using_array() {
  int index = -1;
  int window_size = 3;

  // Creating lru_list_array
  vector<int> lru_list(buffer_pool.size());
  for (int i = 0; i < buffer_pool.size(); i++){
    lru_list[i] = i;
  }

  // Sorting the lru_list_array according to the timestamps of the bufferpool pages
  std::sort(lru_list.begin(), lru_list.end(),[&](int A, int B) -> bool {return buffer_pool[A].timestamp < buffer_pool[B].timestamp;});

  // Find the first clean page in the clean-first region
  for (int i = 0; i < max_buffer_size / window_size; ++i) {
    if (!buffer_pool[lru_list[i]].dirty) {
      return i;
    }
  }
  // Return the classical LRU index for bufferpool
  index = lru_list[0];
  return index;
}

// The below function implements LRU-WSR(LRU Write Sequence Reordering)
int Buffer::LRUWSR()
{
  int index;

  bool found = false;

  while (!found)
  {
      index = 0;
      int min_timestamp = buffer_pool[0].timestamp;
    // First the least recently used page is found
    for (int i = 1; i < buffer_pool.size(); ++i)
    {
      if (buffer_pool[i].timestamp < min_timestamp)
      {
        min_timestamp = buffer_pool[i].timestamp;
        index = i;
      }
    }

    // We check if the LRU page is dirty. If it is clean, we evict it
    if (!buffer_pool[index].dirty)
    {
      found = true;
    }
    else
    {
      // We then check if the dirty page is cold. If it is cold, we evict it
      if (buffer_pool[index].cold)
      {
        found = true;
      }
      // If the dirty page is hot, we set the cold flag and update the timestamp. We give a second chance to the dirty hot page
      else
      {
        buffer_pool[index].cold = true;                                           
        buffer_pool[index].timestamp = Buffer::global_clock;
        ++Buffer::global_clock;
      }
    }
  }
  return index;
}

// The below function prints the stats of the buffer
int Buffer::printStats()
{
  Simulation_Environment* _env = Simulation_Environment::getInstance();
  cout << "******************************************************" << endl;
  cout << "Printing Stats..." << endl;
  cout << "Number of operations: " << _env->num_operations << endl;
  cout << "Buffer Hit: " << buffer_hit << endl;
  cout << "Buffer Miss: " << buffer_miss << endl;
  cout << "Read IO: " << read_io << endl;
  cout << "Write IO: " << write_io << endl;
  cout << "Global Clock: " << global_clock << endl;
  cout << "******************************************************" << endl;
  return 0;
}

Buffer *Buffer::getBufferInstance(Simulation_Environment *_env) {
    if (buffer_instance== nullptr) {
        buffer_instance = new Buffer(_env);
    }
    return buffer_instance;
}