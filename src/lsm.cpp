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
  , accesses{0}
  , hits{0}
  , map{}
  , head{nullptr}
  , segments{}
  , free_segments{}
{
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
  if (!warmup)
    ++accesses;

  auto it = map.find(r->kid);
  if (it != map.end()) {
    auto& list_it = it->second;
    segment* old_segment = list_it->seg;
    request* old_request = &list_it->req;
    if (old_request->size() == r->size()) {
      if (!warmup)
        ++hits;

      // Promote this item to the front.
      old_segment->queue.erase(list_it);
      old_segment->queue.emplace_front(old_segment, *r);
      map[r->kid] = old_segment->queue.begin();

      return 0;
    } else {
      // Size has changed. Even though it is in cache it must have already been
      // evicted or shotdown. Since then it must have already been replaced as
      // well. This means that there must have been some intervening get miss
      // for it. So we need to count an extra access here (but not an extra
      // hit). We do need to remove old_item from the map table, but
      // it gets overwritten below anyway when r gets put in the cache.

      // Count the get miss that came between r and old_item.
      if (!warmup)
        ++accesses;      
      // Finally, we need to really put the correct sized value somewhere
      // in the LRU queue. So fall through to the evict+insert clauses.
    }
  }

  if (head->filled_bytes + r->size() > segment_size)
    rollover();
  assert(head->filled_bytes + r->size() <= segment_size);

  // Add the new request.
  head->queue.emplace_front(head, *r);
  map[r->kid] = head->queue.begin();
  head->filled_bytes += r->size();
 
  // Count this request as a hit.
  if (!warmup)
    ++hits;

  return 0;
}

size_t lsm::get_bytes_cached()
{
  return 0;
}

void lsm::log() {
  std::ofstream out{"lsm-" + filename_suffix + ".data"};
  out << "global_mem segment_size cleaning_width hits accesses" << std::endl;
  out << global_mem << " "
      << segment_size << " "
      << cleaning_width  << " "
      << hits << " "
      << accesses << std::endl;
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

  if (free_segments < cleaning_width + 2)
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
    for (auto& segment : segments) {
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

auto lsm::choose_cleaning_destinations() -> std::vector<segment*> {
  std::vector<segment*> dsts{};

  for (size_t i = 0; i < cleaning_width - 1; ++i) {
    for (auto& segment : segments) {
      // Don't pick a used segment. (This also guarantees we don't pick
      // the same free segment more than once.)
      if (segment)
        continue;

      // Construct, clean empty segment here.
      segment.emplace();
      --free_segments;
      dsts.emplace_back(&segment.value());
      break;
    }
    // Check for enough free segments during cleaning; if not then a bug!
    assert(dsts.size() == i + 1);
  }

  return dsts;
}

void lsm::clean()
{
  /*
  const char* spinner = "|/-\\";
  static uint8_t last = 0;
  std::cerr << spinner[last++ & 0x03] << '\r';
  */

  std::vector<segment*> src_segments = choose_cleaning_sources();
  std::vector<segment*> dst_segments = choose_cleaning_destinations();

  // Choose and construct each of the dst_segments. Wait until now so the src
  // selection doesn't choose them as a src.

  /*
  // Dump, just to visually verify non-overlapping sets.
  for (auto& src : src_segments)
    std::cerr << "src " << src << std::endl;
  for (auto& dst: dst_segments)
    std::cerr << "dst " << dst << std::endl;
  */

  /*
  std::cerr << "Cleaning Plan [";
  for (auto& segment : segments) {
    if (segment) {
      for (auto& src : src_segments) {
        if (src == &segment.value()) {
          std::cerr << "S";
          goto next;
        }
      }
      for (auto& dst: dst_segments) {
        if (dst == &segment.value()) {
          std::cerr << "D";
          goto next;
        }
      }
    }
    std::cerr << "-";
next:
    continue;
  }
  std::cerr << "]" << std::endl;
  */

  // One iterator for each segment to be cleaned. We use these to keep a
  // finger into each queue and merge them into a sorted order in the
  // destination segments.
  std::vector<lru_queue::iterator> its{};
  for (auto* segment : src_segments)
    its.emplace_back(segment->queue.begin());

  size_t dst_index = 0;
  segment* dst = dst_segments.at(dst_index);

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
      if (dst_index == dst_segments.size()) {
        break;
      }
      // Rollover to new dst.
      dst = dst_segments.at(dst_index);
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

        if (from_list == from_hash)
          map.erase(it->req.kid);
      }

      ++it;
    }
  }

  // Sanity check - none of the items left in the hash table should point
  // into a src_segment.
  for (auto& entry : map) {
    item& item = *entry.second;
    for (segment* src : src_segments)
      assert(item.seg != src);
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


