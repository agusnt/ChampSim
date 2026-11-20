#include "berti.h"

#include <algorithm>
#include <bit>
#include <map>
#include <cassert>
#include <iostream>

#include "cache.h"

/*
 * Berti: an Accurate Local-Delta Data Prefetcher
 *
 * 55th ACM/IEEE International Conference on Microarchitecture (MICRO 2022),
 * October 1-5, 2022, Chicago, Illinois, USA.
 *
 * @Authors: Agustín Navarro-Torres, Biswabandan Panda, J. Alastruey-Benedé,
 *           Pablo Ibáñez, Víctor Viñals-Yúfera, and Alberto Ros
 * @Manteiners: Agustín Navarro -Torres
 * @Email: agusnt@unizar.es
 * @Date: 22/11/2022
 *
 * This code is an update from the original code to work with the new version
 * of ChampSim: https://github.com/agusnt/Berti-Artifact
 *
 * Maybe fine-tuning is required to get the optimal performance/accuracy.
 *
 * Please note that this version of ChampSim has noticeable differences with
 * the used for the paper, so results can varies.
 *
 * Cite this:
 *
 * A. Navarro-Torres, B. Panda, J. Alastruey-Benedé, P. Ibáñez,
 * V. Viñals-Yúfera and A. Ros,
 * "Berti: an Accurate Local-Delta Data Prefetcher,"
 * 2022 55th IEEE/ACM International Symposium on Microarchitecture (MICRO),
 * 2022, pp. 975-991, doi: 10.1109/MICRO56248.2022.00072.
 *
 * @INPROCEEDINGS{9923806,  author={Navarro-Torres, Agustín and Panda,
 * Biswabandan and Alastruey-Benedé, Jesús and Ibáñez, Pablo and Viñals-Yúfera,
 * Víctor and Ros, Alberto},  booktitle={2022 55th IEEE/ACM International
 * Symposium on Microarchitecture (MICRO)},   title={Berti: an Accurate
 * Local-Delta Data Prefetcher},   year={2022},  volume={},  number={},
 * pages={975-991},  doi={10.1109/MICRO56248.2022.00072}}
 */

int contarBits(unsigned int n)
{
  int count = 0;
  while (n) {
    count += n & 1;
    n >>= 1;
  }
  return count;
}

std::unordered_map<uint64_t, uint64_t> ip_issue_pf;
/******************************************************************************/
/*                      Latency table functions                               */
/******************************************************************************/
uint8_t berti::LatencyTable::add(uint64_t addr, uint64_t tag, bool pf, uint64_t cycle)
{
  /*
   * Save if possible the new miss into the pqmshr (latency) table
   *
   * Parameters:
   *  - addr: address without cache offset
   *  - access: is theh entry accessed by a demand request
   *  - cycle: time to use in the latency table
   *
   * Return: pf
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_LATENCY_TABLE] " << __func__;
    std::cout << " addr: " << std::hex << addr << " tag: " << tag;
    std::cout << " prefetch: " << std::dec << +pf << " cycle: " << cycle;
  }

  latency_table* free;
  free = nullptr;

  for (int i = 0; i < size; i++) {
    // Search if the addr already exists. If it exist we does not have
    // to do nothing more
    if (latencyt[i].addr == addr) {
      if constexpr (champsim::debug_print) {
        std::cout << " line already found; find_tag: " << latencyt[i].tag;
        std::cout << " find_pf: " << +latencyt[i].pf << std::endl;
      }
      // latencyt[i].time = cycle;
      latencyt[i].pf = pf;
      latencyt[i].tag = tag;
      return latencyt[i].pf;
    }

    // We discover a free space into the latency table, save it for later
    if (latencyt[i].tag == 0)
      free = &latencyt[i];
  }

  // The table is full (e.g. entries of dropped prefetches): do not track this request
  if (free == nullptr)
    return pf;

  // We save the new entry into the latency table
  free->addr = addr;
  free->time = cycle;
  free->tag = tag;
  free->pf = pf;

  if constexpr (champsim::debug_print)
    std::cout << " new entry" << std::endl;
  return free->pf;
}

uint64_t berti::LatencyTable::del(uint64_t addr)
{
  /*
   * Remove the address from the latency table
   *
   * Parameters:
   *  - addr: address without cache offset
   *
   *  Return: the latency of the address
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_LATENCY_TABLE] " << __func__;
    std::cout << " addr: " << std::hex << addr;
  }

  for (int i = 0; i < size; i++) {
    // Line already in the table
    if (latencyt[i].addr == addr) {
      // Calculate latency
      uint64_t time = latencyt[i].time;

      if constexpr (champsim::debug_print) {
        std::cout << " tag: " << latencyt[i].tag;
        std::cout << " prefetch: " << std::dec << +latencyt[i].pf;
        std::cout << " cycle: " << latencyt[i].time << std::endl;
      }

      latencyt[i].addr = 0; // Free the entry
      latencyt[i].tag = 0;  // Free the entry
      latencyt[i].time = 0; // Free the entry
      latencyt[i].pf = 0;   // Free the entry

      // Return the latency
      return time;
    }
  }

  // We should always track the misses
  if constexpr (champsim::debug_print)
    std::cout << " TRANSLATION" << std::endl;
  return 0;
}

uint64_t berti::LatencyTable::get(uint64_t addr)
{
  /*
   * Return time or 0 if the addr is or is not in the pqmshr (latency) table
   *
   * Parameters:
   *  - addr: address without cache offset
   *
   * Return: time if the line is in the latency table, otherwise 0
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_LATENCY_TABLE] " << __func__;
    std::cout << " addr: " << std::hex << addr << std::dec;
  }

  for (int i = 0; i < size; i++) {
    // Search if the addr already exists
    if (latencyt[i].addr == addr) {
      if constexpr (champsim::debug_print) {
        std::cout << " time: " << latencyt[i].time << std::endl;
      }
      return latencyt[i].time;
    }
  }

  if constexpr (champsim::debug_print)
    std::cout << " NOT FOUND" << std::endl;
  return 0;
}

uint64_t berti::LatencyTable::get_tag(uint64_t addr)
{
  /*
   * Return IP-Tag or 0 if the addr is or is not in the pqmshr (latency) table
   *
   * Parameters:
   *  - addr: address without cache offset
   *
   * Return: ip-tag if the line is in the latency table, otherwise 0
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_LATENCY_TABLE] " << __func__;
    std::cout << " addr: " << std::hex << addr;
  }

  for (int i = 0; i < size; i++) {
    if (latencyt[i].addr == addr && latencyt[i].tag) // This is the address
    {
      if constexpr (champsim::debug_print) {
        std::cout << " tag: " << latencyt[i].tag << std::endl;
      }
      return latencyt[i].tag;
    }
  }

  if constexpr (champsim::debug_print)
    std::cout << " NOT_FOUND" << std::endl;
  return 0;
}

/******************************************************************************/
/*                       Shadow Cache functions                               */
/******************************************************************************/
bool berti::ShadowCache::add(uint32_t set, uint32_t way, uint64_t addr, bool pf, uint64_t lat)
{
  /*
   * Add block to shadow cache
   *
   * Parameters:
   *      - cpu: cpu
   *      - set: cache set
   *      - way: cache way
   *      - addr: cache block v_addr
   *      - access: the cache is access by a demand
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_SHADOW_CACHE] " << __func__;
    std::cout << " set: " << set << " way: " << way;
    std::cout << " addr: " << std::hex << addr << std::dec;
    std::cout << " pf: " << +pf;
    std::cout << " latency: " << lat << std::endl;
  }

  // The cache may report way == NUM_WAY when the replacement policy bypasses the fill
  if (set >= (uint32_t)sets || way >= (uint32_t)ways)
    return false;

  scache[set][way].addr = addr;
  scache[set][way].pf = pf;
  scache[set][way].lat = lat;
  return scache[set][way].pf;
}

bool berti::ShadowCache::get(uint64_t addr)
{
  /*
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: true if the addr is in the l1d cache, false otherwise
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_SHADOW_CACHE] " << __func__;
    std::cout << " addr: " << std::hex << addr << std::endl;
  }

  for (int i = 0; i < sets; i++) {
    for (int ii = 0; ii < ways; ii++) {
      if (scache[i][ii].addr == addr) {
        if constexpr (champsim::debug_print) {
          std::cout << " set: " << i << " way: " << i << std::endl;
        }
        return true;
      }
    }
  }

  return false;
}

void berti::ShadowCache::set_pf(uint64_t addr, bool pf)
{
  /*
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: change value of pf field
   */
  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_SHADOW_CACHE] " << __func__;
    std::cout << " addr: " << std::hex << addr << std::dec;
  }

  for (int i = 0; i < sets; i++) {
    for (int ii = 0; ii < ways; ii++) {
      if (scache[i][ii].addr == addr) {
        if constexpr (champsim::debug_print) {
          std::cout << " set: " << i << " way: " << ii;
          std::cout << " old_pf_value: " << +scache[i][ii].pf;
          std::cout << " new_pf_value: " << +pf << std::endl;
        }
        scache[i][ii].pf = pf;
        return;
      }
    }
  }

  // The address should always be in the cache
  assert((0) && "Address is must be in shadow cache");
}

bool berti::ShadowCache::is_pf(uint64_t addr)
{
  /*
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: True if the saved one is a prefetch
   */

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_SHADOW_CACHE] " << __func__;
    std::cout << " addr: " << std::hex << addr << std::dec;
  }

  for (int i = 0; i < sets; i++) {
    for (int ii = 0; ii < ways; ii++) {
      if (scache[i][ii].addr == addr) {
        if constexpr (champsim::debug_print) {
          std::cout << " set: " << i << " way: " << ii;
          std::cout << " pf: " << +scache[i][ii].pf << std::endl;
        }

        return scache[i][ii].pf;
      }
    }
  }

  assert((0) && "Address is must be in shadow cache");
  return 0;
}

uint64_t berti::ShadowCache::get_latency(uint64_t addr)
{
  /*
   * Init shadow cache
   *
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: the saved latency
   */
  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_SHADOW_CACHE] " << __func__;
    std::cout << " addr: " << std::hex << addr << std::dec;
  }

  for (int i = 0; i < sets; i++) {
    for (int ii = 0; ii < ways; ii++) {
      if (scache[i][ii].addr == addr) {
        if constexpr (champsim::debug_print) {
          std::cout << " set: " << i << " way: " << ii;
          std::cout << " latency: " << scache[i][ii].lat << std::endl;
        }

        return scache[i][ii].lat;
      }
    }
  }

  assert((0) && "Address is must be in shadow cache");
  return 0;
}
/******************************************************************************/
/*                       History Table functions                               */
/******************************************************************************/
void berti::HistoryTable::add(uint64_t tag, uint64_t addr, uint64_t cycle)
{
  /*
   * Save the new information into the history table
   *
   * Parameters:
   *  - tag: PC tag
   *  - addr: addr access
   */
  // uint16_t set = tag & TABLE_SET_MASK;
  uint64_t set = tag % sets;
  // If the latest entry is the same, we do not add it
  history_table* latest = (history_pointers[set] == &historyt[set][0]) ? &historyt[set][ways - 1] : history_pointers[set] - 1;
  if (latest->addr == (addr & ADDR_MASK))
    return;

  // Save new element into the history table
  history_pointers[set]->tag = tag;
  history_pointers[set]->time = cycle & TIME_MASK;
  history_pointers[set]->addr = addr & ADDR_MASK;

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_HISTORY_TABLE] " << __func__;
    std::cout << " tag: " << std::hex << tag << " line_addr: " << addr << std::dec;
    std::cout << " cycle: " << cycle << " set: " << set << std::endl;
  }

  if (history_pointers[set] == &historyt[set][ways - 1]) {
    history_pointers[set] = &historyt[set][0]; // End the cycle
  } else
    history_pointers[set]++; // Pointer to the next (oldest) entry
}

uint16_t berti::HistoryTable::get_aux(uint32_t latency, uint64_t tag, uint64_t act_addr, uint64_t* tags, uint64_t* addr, uint64_t cycle)
{
  /*
   * Return an array (by parameter) with all the possible PC that can launch
   * an on-time and late prefetch
   *
   * Parameters:
   *  - tag: PC tag
   *  - latency: latency of the processor
   */

  uint16_t num_on_time = 0;
  // uint16_t set = tag & TABLE_SET_MASK;
  uint64_t set = tag % sets;

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_HISTORY_TABLE] " << __func__;
    std::cout << " tag: " << std::hex << tag << " line_addr: " << addr << std::dec;
    std::cout << " cycle: " << cycle << " set: " << set << std::endl;
  }

  // This is the begin of the simulation
  if (cycle < latency)
    return num_on_time;

  // The IPs that is launch in this cycle will be able to launch this prefetch
  cycle -= latency;

  // Pointer to guide
  history_table* pointer = history_pointers[set];

  do {
    if (num_on_time >= BERTI_TABLE_DELTA_SIZE)
      return num_on_time;
    // Look for the IPs that can launch this prefetch
    if (pointer->tag == tag && pointer->time <= cycle) {
      // Test that addr is not duplicated
      if (pointer->addr == act_addr)
        return num_on_time;
      // This IP can launch the prefetch
      tags[num_on_time] = pointer->tag;
      addr[num_on_time] = pointer->addr;
      num_on_time++;
    }

    if (pointer == historyt[set]) {
      // We get at the end of the history, we start again
      pointer = &historyt[set][ways - 1];
    } else
      pointer--;
  } while (pointer != history_pointers[set]);

  return num_on_time;
}

uint16_t berti::HistoryTable::get(uint32_t latency, uint64_t tag, uint64_t act_addr, uint64_t* tags, uint64_t* addr, uint64_t cycle)
{
  /*
   * Return an array (by parameter) with all the possible PC that can launch
   * an on-time and late prefetch
   *
   * Parameters:
   *  - tag: PC tag
   *  - latency: latency of the processor
   *  - on_time_ip (out): ips that can launch an on-time prefetch
   *  - on_time_addr (out): addr that can launch an on-time prefetch
   *  - num_on_time (out): number of ips that can launch an on-time prefetch
   */

  act_addr &= ADDR_MASK;

  uint16_t num_on_time = get_aux(latency, tag, act_addr, tags, addr, cycle & TIME_MASK);

  // We found on-time prefetchs
  return num_on_time;
}

/******************************************************************************/
/*                        Berti table functions                               */
/******************************************************************************/
void berti::InnerBerti::update_last_addr(uint64_t ip, uint64_t line_addr)
{
  if (bertit.count(ip)) {
    if constexpr (champsim::debug_print) {
      std::cout << "[BERTI_LATENCY_TABLE] " << __func__;
      std::cout << std::hex << " B@: " << line_addr << " IP: " << ip;
      std::cout << std::dec << std::endl;
    }
    bertit[ip]->last_addr = line_addr;
  }
}

uint64_t berti::InnerBerti::get_last_addr(uint64_t ip)
{
  if (bertit.count(ip)) {
    return bertit[ip]->last_addr;
  }
  return 0;
}

void berti::InnerBerti::increase_conf_tag(uint64_t tag)
{
  /*
   * Increase the global confidence of the deltas associated to the tag
   *
   * Parameters:
   *  tag : tag to find
   */
  if constexpr (champsim::debug_print)
    std::cout << "[BERTI_BERTI] " << __func__ << " tag: " << std::hex << tag << std::dec;

  if (bertit.find(tag) == bertit.end()) {
    // Tag not found
    if constexpr (champsim::debug_print)
      std::cout << " TAG NOT FOUND" << std::endl;

    return;
  }

  // Get the entries and the deltas

  bertit[tag]->conf += CONFIDENCE_INC;

  if constexpr (champsim::debug_print)
    std::cout << " global_conf: " << bertit[tag]->conf;

  if (bertit[tag]->conf == CONFIDENCE_MAX) {

    // Max confidence achieve
    for (auto& i : bertit[tag]->deltas) {
      // Set bits to prefetch level
      if (i.conf > CONFIDENCE_L1)
        i.rpl = BERTI_L1;
      else if (i.conf > CONFIDENCE_L2)
        i.rpl = BERTI_L2;
      else if (i.conf > CONFIDENCE_L2R)
        i.rpl = BERTI_L2R;
      else
        i.rpl = BERTI_R;

      if constexpr (champsim::debug_print) {
        std::cout << "Delta: " << i.delta;
        std::cout << " Conf: " << i.conf << " Level: " << +i.rpl;
        std::cout << "|";
      }

      i.conf = 0; // Reset confidence
    }

    bertit[tag]->conf = 0; // Reset global confidence
  }

  if constexpr (champsim::debug_print)
    std::cout << std::endl;
}

void berti::InnerBerti::add(uint64_t tag, int64_t delta)
{
  /*
   * Save the new information into the history table
   *
   * Parameters:
   *  - tag: PC tag
   *  - cpu: actual cpu
   *  - stride: actual cpu
   */
  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_BERTI] " << __func__;
    std::cout << " tag: " << std::hex << tag << std::dec;
    std::cout << " delta: " << delta;
  }

  auto add_delta = [](auto delta, auto entry) {
    // Lambda function to add a new element
    delta_t new_delta;
    new_delta.delta = delta;
    new_delta.conf = CONFIDENCE_INIT;
    new_delta.rpl = BERTI_R;
    auto it = std::find_if(std::begin(entry->deltas), std::end(entry->deltas), [](const auto i) { return (i.delta == 0); });
    assert(it != std::end(entry->deltas));
    *it = new_delta;
  };

  if (bertit.find(tag) == bertit.end()) {
    if constexpr (champsim::debug_print)
      std::cout << " allocating a new entry;";

    // We are not tracking this tag
    if (bertit_queue.size() > BERTI_TABLE_SIZE) {
      // FIFO replacent algorithm
      uint64_t key = bertit_queue.front();
      berti_entry* entry = bertit[key];

      if constexpr (champsim::debug_print)
        std::cout << " removing tag: " << std::hex << key << std::dec << ";";

      delete entry; // Free previous entry

      bertit.erase(bertit_queue.front());
      bertit_queue.pop();
    }

    bertit_queue.push(tag); // Add new tag
    assert((bertit.size() <= BERTI_TABLE_SIZE) && "Tracking too much tags");

    // Confidence IP
    berti_entry* entry = new berti_entry;
    entry->conf = CONFIDENCE_INC;
    entry->valid = true;

    // Saving the new stride
    add_delta(delta, entry);

    if constexpr (champsim::debug_print)
      std::cout << " confidence: " << CONFIDENCE_INIT << std::endl;

    // Save the new tag
    bertit.insert(std::make_pair(tag, entry));
    return;
  }

  // Get the delta
  berti_entry* entry = bertit[tag];

  for (auto& i : entry->deltas) {
    if (i.delta == delta) {
      // We already track the delta
      i.conf += CONFIDENCE_INC;

      if (i.conf > CONFIDENCE_MAX)
        i.conf = CONFIDENCE_MAX;

      if constexpr (champsim::debug_print)
        std::cout << " confidence: " << i.conf << std::endl;

      return;
    }
  }

  // We have space to add a new entry
  auto ssize = std::count_if(std::begin(entry->deltas), std::end(entry->deltas), [](const auto i) { return i.delta != 0; });

  if (ssize < size) {
    add_delta(delta, entry);
    assert((std::size(entry->deltas) <= size) && "I remember too much deltas");
    return;
  }

  // We find the delta with less confidence
  std::sort(std::begin(entry->deltas), std::end(entry->deltas), compare_rpl);
  if (entry->deltas.front().rpl == BERTI_R || entry->deltas.front().rpl == BERTI_L2R) {
    if constexpr (champsim::debug_print)
      std::cout << " replaced_delta: " << entry->deltas.front().delta << std::endl;
    entry->deltas.front().delta = delta;
    entry->deltas.front().conf = CONFIDENCE_INIT;
    entry->deltas.front().rpl = BERTI_R;
  }
}

uint8_t berti::InnerBerti::get(uint64_t tag, std::vector<delta_t>& res)
{
  /*
   * Save the new information into the history table
   *
   * Parameters:
   *  - tag: PC tag
   *
   * Return: the stride to prefetch
   */
  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI_BERTI] " << __func__ << " tag: " << std::hex << tag;
    std::cout << std::dec;
  }

  if (!bertit.count(tag)) {
    if constexpr (champsim::debug_print)
      std::cout << " TAG NOT FOUND" << std::endl;
    // no_found_berti++;
    return 0;
  }

  // found_berti++;

  if constexpr (champsim::debug_print)
    std::cout << std::endl;

  // We found the tag
  berti_entry* entry = bertit[tag];

  for (auto& i : entry->deltas) {
    if (i.delta != 0 && i.rpl != BERTI_R)
      res.push_back(i);
  }

  if (res.empty() && entry->conf >= LAUNCH_MIDDLE_CONF) {
    // We do not find any delta, so we will try to launch with small confidence
    for (auto& i : entry->deltas) {
      if (i.delta != 0) {
        delta_t new_delta;
        new_delta.delta = i.delta;
        if (i.conf > CONFIDENCE_MIDDLE_L1)
          new_delta.rpl = BERTI_L1;
        else if (i.conf > CONFIDENCE_MIDDLE_L2)
          new_delta.rpl = BERTI_L2;
        else
          continue;
        res.push_back(new_delta);
      }
    }
  }

  // Sort the entries
  std::sort(std::begin(res), std::end(res), compare_greater_delta);
  return 1;
}

void berti::InnerBerti::find_and_update(uint64_t latency, uint64_t tag, uint64_t cycle, uint64_t line_addr)
{
  // We were tracking this miss
  uint64_t tags[HISTORY_TABLE_WAYS];
  uint64_t addr[HISTORY_TABLE_WAYS];
  uint16_t num_on_time = 0;

  // Get the IPs that can launch a prefetch
  num_on_time = historyt->get(latency, tag, line_addr, tags, addr, cycle);
  bool first = true;

  for (uint32_t i = 0; i < num_on_time; i++) {
    // Add information into berti table
    int64_t stride;
    line_addr &= ADDR_MASK;

    // Usually applications go from lower to higher memory position.
    // The operation order is important (mainly because we allow
    // negative strides)
    stride = (int64_t)(line_addr - addr[i]);

    if (stride == 0)
      continue;

    if ((std::abs(stride) < (1 << DELTA_MASK))) {
      add(tags[i], stride);
      // Increase conf tag
      if (first) {
        increase_conf_tag(tag);
        first = false;
      }
    }
  }
}

bool berti::InnerBerti::compare_rpl(delta_t a, delta_t b)
{
  if (a.rpl == BERTI_R && b.rpl != BERTI_R)
    return 1;
  else if (b.rpl == BERTI_R && a.rpl != BERTI_R)
    return 0;
  else if (a.rpl == BERTI_L2R && b.rpl != BERTI_L2R)
    return 1;
  else if (b.rpl == BERTI_L2R && a.rpl != BERTI_L2R)
    return 0;
  else {
    if (a.conf < b.conf)
      return 1;
    else
      return 0;
  }
}

bool berti::InnerBerti::compare_greater_delta(delta_t a, delta_t b)
{
  // Sorted stride when the confidence is full
  if (a.rpl == BERTI_L1 && b.rpl != BERTI_L1)
    return 1;
  else if (a.rpl != BERTI_L1 && b.rpl == BERTI_L1)
    return 0;
  else {
    if (a.rpl == BERTI_L2 && b.rpl != BERTI_L2)
      return 1;
    else if (a.rpl != BERTI_L2 && b.rpl == BERTI_L2)
      return 0;
    else {
      if (a.rpl == BERTI_L2R && b.rpl != BERTI_L2R)
        return 1;
      if (a.rpl != BERTI_L2R && b.rpl == BERTI_L2R)
        return 0;
      else {
        if (std::abs(a.delta) < std::abs(b.delta))
          return 1;
        return 0;
      }
    }
  }
}

uint64_t berti::InnerBerti::ip_hash(uint64_t ip)
{
  /*
   * IP hash function
   */
#ifdef HASH_ORIGINAL
  ip = ((ip >> 1) ^ (ip >> 4)); // Original one
#endif
  // IP hash from here: http://burtleburtle.net/bob/hash/integer.html
#ifdef THOMAS_WANG_HASH_1
  ip = (ip ^ 61) ^ (ip >> 16);
  ip = ip + (ip << 3);
  ip = ip ^ (ip >> 4);
  ip = ip * 0x27d4eb2d;
  ip = ip ^ (ip >> 15);
#endif
#ifdef THOMAS_WANG_HASH_2
  ip = (ip + 0x7ed55d16) + (ip << 12);
  ip = (ip ^ 0xc761c23c) ^ (ip >> 19);
  ip = (ip + 0x165667b1) + (ip << 5);
  ip = (ip + 0xd3a2646c) ^ (ip << 9);
  ip = (ip + 0xfd7046c5) + (ip << 3);
  ip = (ip ^ 0xb55a4f09) ^ (ip >> 16);
#endif
#ifdef THOMAS_WANG_HASH_3
  ip -= (ip << 6);
  ip ^= (ip >> 17);
  ip -= (ip << 9);
  ip ^= (ip << 4);
  ip -= (ip << 3);
  ip ^= (ip << 10);
  ip ^= (ip >> 15);
#endif
#ifdef THOMAS_WANG_HASH_4
  ip += ~(ip << 15);
  ip ^= (ip >> 10);
  ip += (ip << 3);
  ip ^= (ip >> 6);
  ip += ~(ip << 11);
  ip ^= (ip >> 16);
#endif
#ifdef THOMAS_WANG_HASH_5
  ip = (ip + 0x479ab41d) + (ip << 8);
  ip = (ip ^ 0xe4aa10ce) ^ (ip >> 5);
  ip = (ip + 0x9942f0a6) - (ip << 14);
  ip = (ip ^ 0x5aedd67d) ^ (ip >> 3);
  ip = (ip + 0x17bea992) + (ip << 7);
#endif
#ifdef THOMAS_WANG_HASH_6
  ip = (ip ^ 0xdeadbeef) + (ip << 4);
  ip = ip ^ (ip >> 10);
  ip = ip + (ip << 7);
  ip = ip ^ (ip >> 13);
#endif
#ifdef THOMAS_WANG_HASH_7
  ip = ip ^ (ip >> 4);
  ip = (ip ^ 0xdeadbeef) + (ip << 5);
  ip = ip ^ (ip >> 11);
#endif
#ifdef THOMAS_WANG_NEW_HASH
  ip ^= (ip >> 20) ^ (ip >> 12);
  ip = ip ^ (ip >> 7) ^ (ip >> 4);
#endif
#ifdef THOMAS_WANG_HASH_HALF_AVALANCHE
  ip = (ip + 0x479ab41d) + (ip << 8);
  ip = (ip ^ 0xe4aa10ce) ^ (ip >> 5);
  ip = (ip + 0x9942f0a6) - (ip << 14);
  ip = (ip ^ 0x5aedd67d) ^ (ip >> 3);
  ip = (ip + 0x17bea992) + (ip << 7);
#endif
#ifdef THOMAS_WANG_HASH_FULL_AVALANCHE
  ip = (ip + 0x7ed55d16) + (ip << 12);
  ip = (ip ^ 0xc761c23c) ^ (ip >> 19);
  ip = (ip + 0x165667b1) + (ip << 5);
  ip = (ip + 0xd3a2646c) ^ (ip << 9);
  ip = (ip + 0xfd7046c5) + (ip << 3);
  ip = (ip ^ 0xb55a4f09) ^ (ip >> 16);
#endif
#ifdef THOMAS_WANG_HASH_INT_1
  ip -= (ip << 6);
  ip ^= (ip >> 17);
  ip -= (ip << 9);
  ip ^= (ip << 4);
  ip -= (ip << 3);
  ip ^= (ip << 10);
  ip ^= (ip >> 15);
#endif
#ifdef THOMAS_WANG_HASH_INT_2
  ip += ~(ip << 15);
  ip ^= (ip >> 10);
  ip += (ip << 3);
  ip ^= (ip >> 6);
  ip += ~(ip << 11);
  ip ^= (ip >> 16);
#endif
#ifdef XOR_SHIFT_HASH
  ip = ip ^ (ip >> 2) ^ (ip >> 5);
#endif
#ifdef FOLD_HASH
  uint64_t hash = 0;
  while (ip) {
    hash ^= (ip & IP_MASK);
    ip >>= SIZE_IP_MASK;
  }
  ip = hash;
#endif
  return ip; // No IP hash
}

berti::welford_t berti::average_latency;


/******************************************************************************/
/*                        Cache Functions                                     */
/******************************************************************************/
uint64_t berti::current_cycle() const { return static_cast<uint64_t>(intern_->current_time.time_since_epoch() / intern_->clock_period); }

void berti::prefetcher_initialize()
{
  pf_to_l1 = 0;
  pf_to_l2 = 0;
  pf_to_l2_bc_mshr = 0;
  cant_track_latency = 0;
  cross_page = 0;
  no_cross_page = 0;
  no_found_berti = 0;
  found_berti = 0;
  average_issued = 0;
  average_num = 0;
  pq_full = 0;
  auto pq_size = 0;
  uint64_t mshr_size = intern_->get_mshr_size();
  // Calculate latency table size
  uint64_t latency_table_size = intern_->get_mshr_size();
  for (auto const& i : intern_->get_rq_size())
    latency_table_size += i;
  for (auto const& i : intern_->get_wq_size())
    latency_table_size += i;
  for (auto const& i : intern_->get_pq_size()) {
    latency_table_size += i;
    pq_size = i;
  }

  std::cout << "PQ: " << pq_size << " MSHR: " << mshr_size << std::endl;
  // New structures
  inner = new InnerBerti(BERTI_TABLE_DELTA_SIZE, latency_table_size, intern_->NUM_SET, intern_->NUM_WAY);
  std::cout << "Berti Prefetcher" << std::endl;

#ifdef NO_CROSS_PAGE
  std::cout << "No Crossing Page" << std::endl;
#endif
#ifdef HASH_ORIGINAL
  std::cout << "BERTI HASH ORIGINAL" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_1
  std::cout << "BERTI HASH 1" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_2
  std::cout << "BERTI HASH 2" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_3
  std::cout << "BERTI HASH 3" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_4
  std::cout << "BERTI HASH 4" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_5
  std::cout << "BERTI HASH 5" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_6
  std::cout << "BERTI HASH 6" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_7
  std::cout << "BERTI HASH 7" << std::endl;
#endif
#ifdef THOMAS_WANG_NEW_HASH
  std::cout << "BERTI HASH NEW" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_HALF_AVALANCHE
  std::cout << "BERTI HASH HALF AVALANCHE" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_FULL_AVALANCHE
  std::cout << "BERTI HASH FULL AVALANCHE" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_INT_1
  std::cout << "BERTI HASH INT 1" << std::endl;
#endif
#ifdef THOMAS_WANG_HASH_INT_2
  std::cout << "BERTI HASH INT 2" << std::endl;
#endif
#ifdef XOR_SHIFT_HASH
  std::cout << "BERTI HASH XOR SHIFT" << std::endl;
#endif
#ifdef FOLD_HASH
  std::cout << "BERTI HASH FOLD" << std::endl;
#endif
  std::cout << "Berti IP MASK " << std::hex << IP_MASK << std::dec << std::endl;
  std::cout << "Berti L1D Conf: " << std::dec << CONFIDENCE_L1 << " L2C: " << CONFIDENCE_L2 << " L2R: " << CONFIDENCE_L2R << std::endl;
  std::cout << "Berti History set: " << std::dec << HISTORY_TABLE_SETS << " ways: " << HISTORY_TABLE_WAYS << std::endl;
  std::cout << "Berti Delta set: " << std::dec << BERTI_TABLE_SIZE << std::endl;


  double total = 0.0;

  // History Table
  double aux =
      ((double)(HISTORY_TABLE_SETS * HISTORY_TABLE_WAYS) * (double)(contarBits(IP_MASK) + contarBits(ADDR_MASK) + contarBits(TIME_MASK)) / (8.0 * 1024.0));
  total += aux;
  std::cout << "  History Table: " << aux << " KB" << std::endl;

  // Berti Table
  aux = ((double)BERTI_TABLE_DELTA_SIZE * (std::log2((double)CONFIDENCE_MAX) + (double)contarBits(DELTA_MASK) + 2.0)) / (8.0 * 1024.0);
  aux += ((double)BERTI_TABLE_SIZE * std::log2((double)CONFIDENCE_MAX)) / (8.0 * 1024.0);
  total += aux;
  std::cout << "  Berti Table: " << aux << " KB" << std::endl;

  // Cache extra bits
  aux = (((double)intern_->NUM_SET * (double)intern_->NUM_WAY) * (double)contarBits(TIME_MASK)) / (8.0 * 1024.0);
  total += aux;
  std::cout << "  Cache extra bits: " << aux << " KB" << std::endl;

  // MSHR extra bits
  aux = ((double)intern_->MSHR_SIZE * (double)contarBits(TIME_MASK)) / (8.0 * 1024.0);
  total += aux;
  std::cout << "  MSHR extra bits: " << aux << " KB" << std::endl;

  std::cout << "Total storage: " << total << " KB" << std::endl;
}

void berti::prefetcher_cycle_operate() {}

uint32_t berti::prefetcher_cache_operate(champsim::address full_addr, champsim::address full_ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                         uint32_t metadata_in)
{
  uint64_t addr = full_addr.to<uint64_t>();
  uint64_t ip = full_ip.to<uint64_t>();
  auto latencyt = inner->latencyt;
  auto scache = inner->scache;
  auto historyt = inner->historyt;

  uint64_t line_addr = (addr >> LOG2_BLOCK_SIZE); // Line addr

  // if (!this->warmup) {
  //   std::cerr << "Cycle: " << current_cycle() << " IP " << std::hex << ip << " Demand Load: " << line_addr << std::dec << std::endl;
  //   if (latencyt->get(line_addr)) {
  //     std::cerr << "Cycle: " << current_cycle() << " IP " << std::hex << ip << " Late Prefetch: " << line_addr << std::dec << std::endl;
  //   }
  // }

  if (line_addr == 0)
    return metadata_in;

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI] operate";
    std::cout << " ip: " << std::hex << ip;
    std::cout << " full_address: " << addr;
    std::cout << " line_address: " << line_addr << std::dec << std::endl;
  }

  uint64_t ip_hash = inner->ip_hash(ip) & IP_MASK;

  inner->update_last_addr(ip_hash, line_addr);

  if (!cache_hit) // This is a miss
  {
    if (latencyt->get(line_addr)) {
      return metadata_in;
    }

    if constexpr (champsim::debug_print)
      std::cout << "[BERTI] operate cache miss" << std::endl;

    latencyt->add(line_addr, ip_hash, false, current_cycle()); // Add @ to latency
    historyt->add(ip_hash, line_addr, current_cycle());        // Add to the table
  } else if (cache_hit && scache->is_pf(line_addr))          // Hit bc prefetch
  {
    if constexpr (champsim::debug_print)
      std::cout << "[BERTI] operate cache hit because of pf" << std::endl;

    scache->set_pf(line_addr, false);

    uint64_t latency = scache->get_latency(line_addr); // Get latency

    if (latency > LAT_MASK)
      latency = 0;

    inner->find_and_update(latency, ip_hash, current_cycle() & TIME_MASK, line_addr);
    historyt->add(ip_hash, line_addr, current_cycle() & TIME_MASK);
  } else {
    if constexpr (champsim::debug_print)
      std::cout << "[BERTI] operate cache hit" << std::endl;
    // >>>>> BERTI_ONLY_MISS_OR_PF_HIT (por defecto 1): solo se emiten prefetches en fallos y en aciertos sobre una línea prebuscada; con 0 los aciertos normales también disparan >>>>>
#ifndef BERTI_ONLY_MISS_OR_PF_HIT
#define BERTI_ONLY_MISS_OR_PF_HIT (1)
#endif
    if (BERTI_ONLY_MISS_OR_PF_HIT)
      return metadata_in;
    // <<<<< BERTI_ONLY_MISS_OR_PF_HIT <<<<<
  }

  std::vector<delta_t> deltas(BERTI_TABLE_DELTA_SIZE);
  inner->get(ip_hash, deltas);

  int nm = 0;
  for (auto i : deltas) {
    uint64_t last_addr = inner->get_last_addr(ip_hash);
    uint64_t p_addr = (last_addr + i.delta) << LOG2_BLOCK_SIZE;
    uint64_t p_b_addr = (p_addr >> LOG2_BLOCK_SIZE);

    if (i.rpl == BERTI_R)
      continue;
    if (p_b_addr == 0)
      continue;

    if ((p_addr >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)) {
      cross_page++;
#ifdef NO_CROSS_PAGE
      // We do not cross virtual page
      continue;
#endif
    } else
      no_cross_page++;

    float mshr_load = intern_->get_mshr_occupancy_ratio() * 100;

    // >>>>> [ANALISIS-MLOP] INICIO: BERTI_ALL_TO_L1 (por defecto 0) hace que todo prefetch con nivel (L1, L2 y L2R) llene L1D >>>>>
#ifndef BERTI_ALL_TO_L1
#define BERTI_ALL_TO_L1 (0)
#endif
    bool fill_this_level = (BERTI_ALL_TO_L1 ? true : (i.rpl == BERTI_L1)) && (mshr_load < MSHR_LIMIT);
    // <<<<< [ANALISIS-MLOP] FIN <<<<<

    if (i.rpl == BERTI_L1 && mshr_load >= MSHR_LIMIT)
      pf_to_l2_bc_mshr++;
    if (fill_this_level)
      pf_to_l1++;
    else
      pf_to_l2++;

    delta_founds[i.delta]++;
    ip_issue_pf[ip_hash] += 1;
    if (prefetch_line(champsim::address{p_addr}, fill_this_level, metadata_in)) {
      nm++;
      ++average_issued;

      if constexpr (champsim::debug_print) {
        std::cout << "[BERTI] operate prefetch delta: " << i.delta;
        std::cout << " addr: " << std::hex << last_addr << std::dec;
        std::cout << " p_addr: " << std::hex << p_b_addr << std::dec;
        std::cout << " tag: " << std::hex << ip_hash << std::dec;
        std::cout << " this_level: " << +fill_this_level << std::endl;
      }

      if (fill_this_level) {
        if (!scache->get(p_b_addr)) {
          latencyt->add(p_b_addr, ip_hash, true, current_cycle());
          // if (!this->warmup)
          //   std::cerr << "Cycle: " << current_cycle() << " Prefetch Addr: " << std::hex << p_b_addr << std::dec << std::endl;
        }
      }
    } else {
      pq_full++;
      if constexpr (champsim::debug_print) {
        std::cout << "[BERTI] can't prefetch delta: " << i.delta;
        std::cout << " PQ full ";
        std::cout << " addr: " << std::hex << last_addr << std::dec;
        std::cout << " p_addr: " << std::hex << p_b_addr << std::dec;
        std::cout << " tag: " << std::hex << ip_hash << std::dec;
        std::cout << " this_level: " << +fill_this_level << std::endl;
      }
    }
  }

  return metadata_in;
}

uint32_t berti::prefetcher_cache_fill(champsim::address full_addr, long set, long way, uint8_t prefetch, champsim::address full_evicted_addr,
                                      uint32_t metadata_in)
{
  uint64_t addr = full_addr.to<uint64_t>();
  uint64_t evicted_addr = full_evicted_addr.to<uint64_t>();
  auto latencyt = inner->latencyt;
  auto scache = inner->scache;
  auto historyt = inner->historyt;

  uint64_t line_addr = (addr >> LOG2_BLOCK_SIZE);                 // Line addr
  uint64_t evicted_line_addr = (evicted_addr >> LOG2_BLOCK_SIZE); // Line addr
  uint64_t tag = latencyt->get_tag(line_addr);
  uint64_t cycle = latencyt->del(line_addr) & TIME_MASK;
  uint64_t latency = 0;

  // if (!this->warmup) {
  //   if (prefetch) {
  //     std::cerr << "Cycle: " << current_cycle() << " Prefetch Fill " << std::hex << line_addr << std::dec << std::endl;
  //   } else {
  //     std::cerr << "Cycle: " << current_cycle() << " Cache Fill " << std::hex << line_addr << std::dec << std::endl;
  //   }
  //   std::cerr << "Cycle: " << current_cycle() << " Evicted line " << std::hex << evicted_line_addr << std::dec << std::endl;
  // }

  // if (tag == 0x4e90) {
  //   std::cout << std::hex << tag << " Fill @: " << line_addr << " cycle: " << std::dec << current_cycle() << " " << (int)prefetch << std::endl;
  // }

  if constexpr (champsim::debug_print) {
    std::cout << "[BERTI] fill addr: " << std::hex << line_addr;
    std::cout << " event_cycle: " << cycle;
    std::cout << " prefetch: " << +prefetch << std::endl;
    std::cout << " latency: " << latency << std::endl;
  }

  if (cycle != 0 && ((current_cycle() & TIME_MASK) > cycle))
    latency = (current_cycle() & TIME_MASK) - cycle;

  if (latency > LAT_MASK) {
    latency = 0;
    cant_track_latency++;
  } else {
    if (latency != 0) {
      // Calculate average latency
      if (average_latency.num == 0)
        average_latency.average = (float)latency;
      else {
        average_latency.average = average_latency.average + ((((float)latency) - average_latency.average) / average_latency.num);
      }
      average_latency.num++;
    }
  }

  // Add to the shadow cache
  scache->add(static_cast<uint32_t>(set), static_cast<uint32_t>(way), line_addr, prefetch, latency);

  if (latency != 0 && !prefetch) {
    inner->find_and_update(latency, tag, cycle, line_addr);
  }
  return metadata_in;
}

void berti::prefetcher_final_stats()
{
  std::cout << "BERTI "
            << "TO_L1: " << pf_to_l1 << " TO_L2: " << pf_to_l2;
  std::cout << " TO_L2_BC_MSHR: " << pf_to_l2_bc_mshr << std::endl;

  std::cout << "BERTI PQ Full: " << pq_full << std::endl;

  std::cout << "BERTI AVG_LAT: ";
  std::cout << average_latency.average << " NUM_TRACK_LATENCY: ";
  std::cout << average_latency.num << " NUM_CANT_TRACK_LATENCY: ";
  std::cout << cant_track_latency << std::endl;

  std::cout << "BERTI CROSS_PAGE " << cross_page;
  std::cout << " NO_CROSS_PAGE: " << no_cross_page << std::endl;

  std::cout << "BERTI";
  std::cout << " FOUND_BERTI: " << found_berti;
  std::cout << " NO_FOUND_BERTI: " << no_found_berti << std::endl;

  std::cout << "BERTI";
  std::cout << " AVERAGE_ISSUED: " << ((1.0 * average_issued) / average_num);
  std::cout << std::endl;

  std::cout << "Deltas found" << std::endl;
  std::map<int, int> sorted(delta_founds.begin(), delta_founds.end());
  for (auto it = sorted.begin(); it != sorted.end(); ++it) {
    std::cout << "\t" << it->first << " -> " << it->second << std::endl;
  }

  std::cout << "Tag issue" << std::endl;
  std::map<int, int> ipsorted(ip_issue_pf.begin(), ip_issue_pf.end());
  for (auto it = ipsorted.begin(); it != ipsorted.end(); ++it) {
    std::cout << "\t" << std::hex << it->first << std::dec << " -> " << it->second << std::endl;
  }

  std::cout << "Berti table (tag conf; delta conf rpl)" << std::endl;
  for (auto const& [tag, entry] : inner->bertit) {
    std::cout << "\t" << std::hex << tag << std::dec << " conf: " << entry->conf << " deltas:";
    for (auto const& d : entry->deltas) {
      if (d.delta != 0)
        std::cout << " (" << d.delta << " c" << d.conf << " r" << +d.rpl << ")";
    }
    std::cout << std::endl;
  }

}
