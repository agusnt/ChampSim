#ifndef PROFILER_H
#define PROFILER_H

#include <string.h>
#include <fstream>
#include "json.h"
#include "champsim.h"
#include "champsim_constants.h"

using namespace std;
using json = nlohmann::json;

class Profiler {
public:
  Profiler() {
  }

  void set_stats_file_name(string filename, string exec_name) {
    full_file_name = filename;
    cout << "Generating Output file in: " << full_file_name << endl;
    exe_name = exec_name;
  }

  void open_stat_file(string filename) {
    stats.open(filename);
  }

  void print_json_stats_file() {
    this->open_stat_file(full_file_name);
    this->print_system_config();
    this->print_stats();
    stats << setw(4) << js << endl;
    stats.close();
  }

  void print_system_config() {
    js["core_config"]["exec"] = exe_name;
    js["core_config"]["cpu"] = cpu;
    js["core_config"]["freq_scale"] = freq_scale;
    js["core_config"]["ifetch_buffer_size"] = ifetch_buffer_size;
    js["core_config"]["decode_buffer_size"] = decode_buffer_size;
    js["core_config"]["dispatch_buffer_size"] = dispatch_buffer_size;
    js["core_config"]["rob_size"] = rob_size;
    js["core_config"]["lq_size"] = lq_size;
    js["core_config"]["sq_size"] = sq_size;
    js["core_config"]["fetch_width"] = fetch_width;
    js["core_config"]["decode_width"] = decode_width;
    js["core_config"]["dispatch_width"] = dispatch_width;
    js["core_config"]["schedule_width"] = schedule_width;
    js["core_config"]["execute_width"] = execute_width;
    js["core_config"]["lq_width"] = lq_width;
    js["core_config"]["sq_width"] = sq_width;
    js["core_config"]["retire_width"] = retire_width;
    js["core_config"]["mispredict_penalty"] = mispredict_penalty;
    js["core_config"]["decode_latency"] = decode_latency;
    js["core_config"]["dispatch_latency"] = dispatch_latency;
    js["core_config"]["schedule_latency"] = schedule_latency;
    js["core_config"]["execute_latency"] = execute_latency;

    js["L1D"]["sets"] = l1cd_sets;
    js["L1D"]["ways"] = l1cd_ways;

    js["L1I"]["sets"] = l1ci_sets;
    js["L1I"]["ways"] = l1ci_ways;
  }

  void print_stats() {
    js["cpu_stats"]["total_retired_instructions"] = m_total_retired;
    js["cpu_stats"]["ipc"] = ipc;
    js["cpu_stats"]["loads"] = loads;
    js["cpu_stats"]["stores"] = stores;

    double hit, miss = 0.0;

    hit = (double(uop_cache_hit) / double(uop_cache_read) * 100.00);
    miss = 100.00 - hit;

    js["uop_cache"]["uop_cache_read"] = uop_cache_read;
    js["uop_cache"]["uop_cache_hit"] = uop_cache_hit;
    js["uop_cache"]["from_fetch"] = from_fetch;
    js["uop_cache"]["hit_per"] = hit;
    js["uop_cache"]["miss_per"] = miss;

    hit = (double(l1i_hit) / double(l1i_access) * 100.00);
    miss = 100.00 - hit;

    js["l1i_cache"]["l1i_access"] = l1i_access;
    js["l1i_cache"]["l1i_hit"] = l1i_hit;
    js["l1i_cache"]["l1i_miss"] = l1i_miss;
    js["l1i_cache"]["hit_per"] = hit;
    js["l1i_cache"]["miss_per"] = miss;

  }

  void clear_stats_after_warmup() {
    cout << "Clearning stats after warmup!!" << endl;

    m_total_retired = 0;
    ipc = 0;
    loads = 0;
    stores = 0;

    uop_cache_read = 0;
    uop_cache_hit = 0;
    from_fetch = 0;

    l1i_access = 0;
    l1i_hit = 0;
    l1i_miss = 0;
  }

  //Add stats here
  uint64_t m_total_retired = 0;
  uint64_t m_total_access_l1d[NUM_TYPES] = { 0 };
  double ipc = 0;
  uint64_t loads = 0;
  uint64_t stores = 0;
  uint64_t uop_cache_read = 0;
  uint64_t uop_cache_hit = 0;
  uint64_t from_fetch = 0;

  uint64_t l1i_access = 0;
  uint64_t l1i_hit = 0;
  uint64_t l1i_miss = 0;


  //System parameters
  uint32_t cpu;
  double freq_scale;
  std::size_t ifetch_buffer_size;
  std::size_t decode_buffer_size;
  std::size_t dispatch_buffer_size;
  std::size_t rob_size;
  std::size_t lq_size;
  std::size_t sq_size;
  unsigned fetch_width;
  unsigned decode_width;
  unsigned dispatch_width;
  unsigned schedule_width;
  unsigned execute_width;
  unsigned lq_width;
  unsigned sq_width;
  unsigned retire_width;
  unsigned mispredict_penalty;
  unsigned decode_latency;
  unsigned dispatch_latency;
  unsigned schedule_latency;
  unsigned execute_latency;

  unsigned l1cd_sets = 0;
  unsigned l1cd_ways = 0;

  unsigned l1ci_sets = 0;
  unsigned l1ci_ways = 0;

  uint64_t test = 0;
  ofstream stats;
  json js;
  string exe_name;
  string full_file_name;
};

inline Profiler* get_profiler_ptr = new Profiler;
#endif
