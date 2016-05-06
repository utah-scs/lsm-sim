#include <iostream>
#include <fstream>
#include <cassert>
#include <unordered_set>
#include <algorithm>

#include "lsm.h"

lsm::lsm(stats stat)
  : policy{stat}
  , cleaner{cleaning_policy::OLDEST_ITEM}
  , map{}
  , head{nullptr}
  , segments{}
  , free_segments{}
{
 
  srand(0);

  if (stat.global_mem % stat.segment_size != 0) {
    std::cerr <<
      "WARNING: global_mem not a multiple of segment_size" << std::endl;
  }
  segments.resize(stat.global_mem / stat.segment_size);
  free_segments = segments.size();
  std::cerr << "global_mem " << stat.global_mem
            << " segment_size " << stat.segment_size
            << " segment_count " << segments.size() << std::endl;

  // Check for enough segments to sustain cleaning width and head segment.
  assert(segments.size() > 2 * stat.cleaning_width);

  // Sets up head.
  rollover(0.);
}

lsm::~lsm() {}

size_t lsm::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  if (stat.apps.empty())
    stat.apps.insert (r->appid);
  assert(stat.apps.count(r->appid) == 1);

  if (!warmup)
    ++stat.accesses;

  auto it = map.find(r->kid);
  if (it != map.end()) {
    if (!warmup)
      ++stat.hits;

    auto list_it = it->second;
    segment* old_segment = list_it->seg;
    int32_t old_request_size = list_it->req.size();
    if (old_request_size  == r->size()) {
      // Promote this item to the front.
      old_segment->queue.erase(list_it);
      old_segment->queue.emplace_front(old_segment, *r);
      map[r->kid] = old_segment->queue.begin();

      ++old_segment->access_count;

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

  if (head->filled_bytes + r->size() > stat.segment_size)
    rollover(r->time);
    //assert(head->filled_bytes + r->size() <= stat.segment_size);

  // Add the new request.
  head->queue.emplace_front(head, *r);
  map[r->kid] = head->queue.begin();
  head->filled_bytes += r->size();

  ++head->access_count;
 
  return PROC_MISS;
}

size_t lsm::get_bytes_cached() const
{
  return 0;
}

void lsm::dump_util(const std::string& filename) {}

void lsm::rollover(double timestamp)
{
  bool rolled = false;
  for (auto& segment : segments) {
    if (segment)
      continue;

    segment.emplace();
    --free_segments;
    head = &segment.value();
    head->low_timestamp = timestamp;
    rolled = true;
    break;
  }
  assert(rolled);

  if (free_segments < stat.cleaning_width)
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
  return double(stat.hits) / stat.accesses;
}

auto lsm::choose_cleaning_sources() -> std::vector<segment*>
{
  switch (cleaner) {
    case cleaning_policy::OLDEST_ITEM:
      return choose_cleaning_sources_oldest_item();
    case cleaning_policy::ROUND_ROBIN:
      return choose_cleaning_sources_round_robin();
    case cleaning_policy::RUMBLE:
      return choose_cleaning_sources_rumble();
    case cleaning_policy::RANDOM:
    default:
      return choose_cleaning_sources_random();
  }
}

auto lsm::choose_cleaning_sources_random() -> std::vector<segment*>
{
  std::vector<segment*> srcs{};

  for (size_t i = 0; i < stat.cleaning_width; ++i) {
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

auto lsm::choose_cleaning_sources_oldest_item() -> std::vector<segment*>
{
  std::vector<segment*> srcs{};

  for (auto& segment : segments) {
    // Don't pick free segments.
    if (!segment)
      continue;

    // Don't pick the head segment.
    if (&segment.value() == head)
      continue;

    srcs.emplace_back(&segment.value());
  }

  std::sort(srcs.begin(), srcs.end(),
      [](const segment* left, const segment* right) {
        return left->low_timestamp < right->low_timestamp;
      });
  //std::cout << "Sorted list" << std::endl;
  //for (segment* src : srcs)
    //std::cout << src->low_timestamp << std::endl;
  srcs.resize(stat.cleaning_width);
  //std::cout << "Selected list" << std::endl;
  //for (segment* src : srcs)
    //std::cout << src->low_timestamp << std::endl;
  //std::cout << "==============" << std::endl;

  for (auto& segment : segments) {
    // Don't pick free segments.
    if (!segment)
      continue;
    segment->access_count = 0;
  }

  // Check for enough in use segments during cleaning; if not then a bug!
  assert(srcs.size() == stat.cleaning_width);

  return srcs;
}

auto lsm::choose_cleaning_sources_rumble() -> std::vector<segment*>
{
  std::vector<segment*> srcs{};

  for (auto& segment : segments) {
    // Don't pick free segments.
    if (!segment)
      continue;

    // Don't pick the head segment.
    if (&segment.value() == head)
      continue;

    srcs.emplace_back(&segment.value());
  }

  std::sort(srcs.begin(), srcs.end(),
      [](const segment* left, const segment* right) {
        return left->access_count < right->access_count;
      });
  //std::cout << "Sorted list" << std::endl;
  //for (segment* src : srcs)
    //std::cout << src->access_count << std::endl;
  srcs.resize(stat.cleaning_width);
  //std::cout << "Selected list" << std::endl;
  //for (segment* src : srcs)
    //std::cout << src->access_count << std::endl;
  //std::cout << "==============" << std::endl;

  for (auto& segment : segments) {
    // Don't pick free segments.
    if (!segment)
      continue;
    segment->access_count = 0;
  }

  // Check for enough in use segments during cleaning; if not then a bug!
  assert(srcs.size() == stat.cleaning_width);

  return srcs;
}

auto lsm::choose_cleaning_sources_round_robin() -> std::vector<segment*>
{
  static size_t next = 0;

  std::vector<segment*> srcs{};

  while (srcs.size() < stat.cleaning_width) {
    auto& segment = segments.at(next);

    if (!segment)
      continue;

    // Don't pick the head segment.
    if (&segment.value() == head)
      continue;

    srcs.emplace_back(&segment.value());

    ++next;
    next %= segments.size();
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

      stat.cleaned_ext_frag_bytes += (stat.segment_size - dst->filled_bytes);
      ++stat.cleaned_generated_segs;

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

    assert(src_segments.at(it_to_incr) == item->seg);

    // Check to see if there is room for the item in the dst.
    if (dst->filled_bytes + item->req.size() > stat.segment_size) {
      stat.cleaned_ext_frag_bytes += (stat.segment_size - dst->filled_bytes);
      ++stat.cleaned_generated_segs;

      ++dst_index;
      // Break out of relocation if we are out of free space our dst segs.
      if (dst_index == stat.cleaning_width - 1)
        break;

      // Rollover to new dst.
      dst = choose_cleaning_destination();
      dst_segments.push_back(dst);
    }
    assert(dst->filled_bytes + item->req.size() <= stat.segment_size);
    ++its.at(it_to_incr);

    // Relocate *to the back* to retain timestamp sort order.
    dst->queue.emplace_back(dst, item->req);
    map[item->req.kid] = (--dst->queue.end());
    dst->filled_bytes += item->req.size();
    dst->low_timestamp = item->req.time;
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
          ++stat.evicted_items;
          stat.evicted_bytes += from_list->req.size();
        }
      }

      ++it;
    }
  }

  const bool debug = false;
  if (debug) {
    // Sanity check - none of the items left in the hash table should point
    // into a src_segment.
    for (auto& entry : map) {
      item& item = *entry.second;
      for (segment* src : src_segments)
        assert(item.seg != src);
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

  if (debug) {
    // Sanity check - none of the segments should contain more data than their
    // rated capacity.
    size_t stored_in_whole_cache = 0;
    for (auto& segment : segments) {
      if (!segment)
        continue;

      size_t bytes = 0;
      for (const auto& item : segment->queue) {
        assert(item.seg == &segment.value());
        bytes += item.req.size();
      }
      assert(bytes <= stat.segment_size);
      assert(bytes == segment->filled_bytes);
      stored_in_whole_cache += bytes;
    }

    // Sanity check - the sum of all of the requests active in the hash table
    // should not be greater than the combined space in all of the in use
    // segments.
    size_t reachable_from_map = 0;
    std::unordered_set<int32_t> seen{};
    for (auto& entry : map) {
      item& item = *entry.second;
      // HT key had better only point to objects with the same kid.
      if (entry.first != item.req.kid) {
        std::cerr << "Mismatch! map entry "
                  << entry.first << " != " << item.req.kid << std::endl;
        item.req.dump();
      }
      assert(entry.first == item.req.kid);
      // Better not see the same kid twice among the objects in the HT.
      assert(seen.find(item.req.kid) == seen.end());
      reachable_from_map += item.req.size();
      seen.insert(item.req.kid);
    }
    if (reachable_from_map > stored_in_whole_cache) {
      std::cerr << "reachable_from_map: " << reachable_from_map << std::endl
                << "stored_in_whole_cache: " << stored_in_whole_cache
                << std::endl;
    }
    assert(reachable_from_map <= stored_in_whole_cache);
  } 
}


