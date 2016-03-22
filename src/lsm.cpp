#include <iostream>
#include <fstream>
#include <cassert>

#include "lsm.h"

lsm::lsm(const std::string& filename_suffix,
         size_t global_mem,
         size_t segment_size,
         size_t cleaning_width)
  : policy{filename_suffix, global_mem}
  , global_mem{global_mem}
  , segment_size{segment_size}
  , cleaning_width{cleaning_width}
  , accesses{}
  , hits{}
  , evicted_bytes{}
  , evicted_items{}
  , map{}
  , head{nullptr}
  , segments{}
  , free_segments{}
  , appid{~0u}
{
  srand(0);

  if (global_mem % segment_size != 0) {
    std::cerr <<
      "WARNING: global_mem not a multiple of segment_size" << std::endl;
  }
  segments.resize(global_mem / segment_size);
  free_segments = segments.size();
  std::cerr << "global_mem " << global_mem
            << " segment_size " << segment_size
            << " segment_count " << segments.size() << std::endl;

  // Check for enough segments to sustain cleaning width and head segment.
  assert(segments.size() > 2 * cleaning_width);

  // Sets up head.
  rollover();
}

lsm::~lsm() {}

size_t lsm::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  if (appid == ~0u)
    appid = r->appid;
  assert(r->appid == appid);

  if (!warmup)
    ++accesses;

  auto it = map.find(r->kid);
  if (it != map.end()) {
    if (!warmup)
      ++hits;

    auto list_it = it->second;
    segment* old_segment = list_it->seg;
    int32_t old_request_size = list_it->req.size();
    if (old_request_size  == r->size()) {
      // Promote this item to the front.
      old_segment->queue.erase(list_it);
      old_segment->queue.emplace_front(old_segment, *r);
      map[r->kid] = old_segment->queue.begin();

      return 1;
    } else {
      // If the size changed, then we have to put it in head. Just
      // falling through to the code below handles that. We still
      // count this record as a single hit, even though some miss
      // would have had to have happened to shootdown the old stale
      // sized value. This is to be fair to other policies like gLRU
      // that don't detect these size changes.
    }
  } else {
    // If miss in hash table, then count a miss. This get will count
    // as a second access that hits below.
  }

  if (head->filled_bytes + r->size() > segment_size)
    rollover();
  assert(head->filled_bytes + r->size() <= segment_size);

  // Add the new request.
  head->queue.emplace_front(head, *r);
  map[r->kid] = head->queue.begin();
  head->filled_bytes += r->size();
 
  return PROC_MISS;
}

size_t lsm::get_bytes_cached() const
{
  return 0;
}

void lsm::log() {
  std::ofstream out{"lsm" + filename_suffix + ".data"};
  out << "app policy global_mem segment_size cleaning_width hits accesses hit_rate"
      << std::endl;
  out << appid << " "
      << "lsm" << " "
      << global_mem << " "
      << segment_size << " "
      << cleaning_width  << " "
      << hits << " "
      << accesses << " "
      << double(hits) / accesses
      << std::endl;
}

void lsm::dump_util(const std::string& filename) {}

void lsm::rollover()
{
  bool rolled = false;
  for (auto& segment : segments) {
    if (segment)
      continue;

    segment.emplace();
    --free_segments;
    head = &segment.value();
    rolled = true;
    break;
  }
  assert(rolled);

  if (free_segments < cleaning_width)
    clean();
}

void lsm::dump_usage()
{
  std::cerr << "[";
  for (auto& segment : segments) 
    std::cerr << (bool(segment) ? "X" : "_");
  std::cerr << "]" << std::endl;
}

double lsm::get_running_hit_rate() {
  return double(hits) / accesses;
}


auto lsm::choose_cleaning_sources() -> std::vector<segment*>
{
  std::vector<segment*> srcs{};

  for (size_t i = 0; i < cleaning_width; ++i) {
    for (size_t j = 0; j < 1000000; ++j) {
      int r = rand() % segments.size();
      auto& segment = segments.at(r);

      // Don't pick free segments.
      if (!segment)
        continue;

      // Don't pick the head segment.
      if (&segment.value() == head)
        continue;

      // Don't pick any segment we've already picked!
      bool already_picked = false;
      for (auto& already_in : srcs)
        already_picked |= (already_in == &segment.value());
      if (already_picked)
        continue;

      srcs.emplace_back(&segment.value());
      break;
    }
    // Check for enough in use segments during cleaning; if not then a bug!
    assert(srcs.size() == i + 1);
  }

  return srcs;
}

auto lsm::choose_cleaning_destination() -> segment* {
  for (auto& segment : segments) {
    // Don't pick a used segment. (This also guarantees we don't pick
    // the same free segment more than once.)
    if (segment)
      continue;

    // Construct, clean empty segment here.
    segment.emplace();
    --free_segments;

    return &segment.value();
  }

  assert(false);
}

void lsm::dump_cleaning_plan(std::vector<segment*> srcs,
                             std::vector<segment*> dsts)
{
  std::cerr << "Cleaning Plan [";
  for (auto& segment : segments) {
    if (segment) {
      for (auto& src : srcs) {
        if (src == &segment.value()) {
          std::cerr << "S";
          goto next;
        }
      }
      for (auto& dst: dsts) {
        if (dst == &segment.value()) {
          std::cerr << "D";
          goto next;
        }
      }
      std::cerr << "X";
      goto next;
    }
    std::cerr << "-";
next:
    continue;
  }
  std::cerr << "]" << std::endl;
} 

void lsm::clean()
{
  /*
  const char* spinner = "|/-\\";
  static uint8_t last = 0;
  std::cerr << spinner[last++ & 0x03] << '\r';
  */

  std::vector<segment*> src_segments = choose_cleaning_sources();

  // One iterator for each segment to be cleaned. We use these to keep a
  // finger into each queue and merge them into a sorted order in the
  // destination segments.
  std::vector<lru_queue::iterator> its{};
  for (auto* segment : src_segments)
    its.emplace_back(segment->queue.begin());

  size_t dst_index = 0;
  segment* dst = choose_cleaning_destination();
  std::vector<segment*> dst_segments{};
  dst_segments.push_back(dst);

  while (true) {
    item* item = nullptr;
    size_t it_to_incr = 0;
    for (size_t i = 0; i < src_segments.size(); ++i) {
      auto& it = its.at(i);
      if (it == src_segments.at(i)->queue.end())
        continue;
      if (!item || it->req.time > item->req.time) {
        item = &*it;
        it_to_incr = i;
      }
    }

    // All done with all its.
    if (!item) {
      for (size_t i = 0; i < src_segments.size(); ++i)
        assert(its.at(i) == src_segments.at(i)->queue.end());
      break;
    }

    // Check to see if this version is still needed.
    auto it = map.find(item->req.kid);
    // If not in the hash table, just drop it.
    if (it == map.end()) {
      ++its.at(it_to_incr);
      continue;
    }
    // If hash table pointer refers to a different version, drop this one.
    if (&*it->second != item) {
      ++its.at(it_to_incr);
      continue;
    }

    // Check to see if there is room for the item in the dst.
    if (dst->filled_bytes + item->req.size() > segment_size) {
      ++dst_index;
      // Break out of relocation if we are out of free space our dst segs.
      if (dst_index == cleaning_width - 1)
        break;
      // Rollover to new dst.
      dst = choose_cleaning_destination();
      dst_segments.push_back(dst);
    }
    assert(dst->filled_bytes + item->req.size() <= segment_size);
    ++its.at(it_to_incr);

    // Relocate *to the back* to retain timestamp sort order.
    dst->queue.emplace_back(dst, item->req);
    map[item->req.kid] = dst->queue.end();
    dst->filled_bytes += item->req.size();
  }

  // Clear items that are going to get thrown on the floor from the hashtable.
  for (size_t i = 0; i < src_segments.size(); ++i) {
    auto& it = its.at(i);
    while (it != src_segments.at(i)->queue.end()) {
      // Need to do a double check here. The item we are evicting may
      // still exist in the hash table but it may point to a newer version
      // of this object. In that case, skip the erase from the hash table.
      auto hash_it = map.find(it->req.kid);
      if (hash_it != map.end()) {
        item* from_list = &*it;
        item* from_hash = &*hash_it->second;

        if (from_list == from_hash) {
          map.erase(it->req.kid);
          ++evicted_items;
          evicted_bytes += from_list->req.size();
        }
      }

      ++it;
    }
  }

  const bool debug = true;
  if (debug) {
    // Sanity check - none of the items left in the hash table should point
    // into a src_segment.
    for (auto& entry : map) {
      item& item = *entry.second;
      for (segment* src : src_segments)
        assert(item.seg != src);
    }

    // Sanity check - none of the segments should contain more data than their
    // rated capacity.
    for (auto& segment : segments) {
      if (!segment)
        continue;

      size_t bytes = 0;
      for (const auto& item : segment->queue) {
        assert(item.seg == &segment.value());
        bytes += item.req.size();
      }
      assert(bytes <= segment_size);
    }

    //dump_cleaning_plan(src_segments, dst_segments);
  } 

  // Reset each src segment as free for reuse.
  for (auto* src : src_segments) {
    for (auto& segment : segments) {
      if (src == &*segment) {
        segment = nullopt;
        ++free_segments;
      }
    }
  }
}


