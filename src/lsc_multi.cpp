#include <iostream>
#include <fstream>
#include <cassert>
#include <unordered_set>
#include <algorithm>

#include "lsc_multi.h"

lsc_multi::application::application(
    size_t appid,
    size_t min_mem,
    size_t target_mem)
  : appid{appid}
  , min_mem{min_mem}
  , target_mem{target_mem}
  , credit_bytes{}
  , bytes_in_use{}
  , accesses{}
  , hits{}
  , shadow_q_hits{}
  , shadow_q{}
  , cleaning_q{}
  , cleaning_it{cleaning_q.end()}
{
  shadow_q.expand(10 * 1024 * 1024);
}

lsc_multi::application::~application() {}

lsc_multi::lsc_multi(stats stat)
  : policy{stat}
  , cleaner{cleaning_policy::OLDEST_ITEM}
  , apps{}
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

lsc_multi::~lsc_multi() {}

void lsc_multi::add_app(size_t appid, size_t min_memory, size_t target_memory)
{
  assert(head->filled_bytes == 0);
  assert(apps.find(appid) == apps.end());
  apps.emplace(appid, application{appid, min_memory, target_memory});
}

size_t lsc_multi::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  if (stat.apps.empty())
    stat.apps.insert (r->appid);
  assert(stat.apps.count(r->appid) == 1);

  auto appit = apps.find(r->appid);
  assert(appit != apps.end());
  application& app = appit->second;

  if (!warmup) {
    ++stat.accesses;
    ++app.accesses;
  }

  auto it = map.find(r->kid);
  if (it != map.end()) {
    if (!warmup) {
      ++stat.hits;
      ++app.hits;
    }

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

  // Bill the correct app for its use of log space.
  app.bytes_in_use += r->size();

  std::vector<size_t> appids{};
  for (size_t appid : stat.apps)
    appids.push_back(appid);

  // Check to see if the access would have hit in the app's shadow Q.
  if (app.proc(r) != PROC_MISS) {
    ++app.shadow_q_hits;
    // Hit in shadow Q! We get to steal!
    const size_t steal_size = 4096;
    for (int i = 0; i < 10; ++i) {
      size_t victim = appids.at(rand() % appids.size());
      auto it = apps.find(victim);
      assert(it != apps.end());
      application& other_app = it->second;
      if (app.try_steal_from(other_app, steal_size))
        break;
    }
    // May not have been able to steal, but oh well.
  }
 
  return PROC_MISS;
}

size_t lsc_multi::get_bytes_cached() const
{
  return 0;
}

void lsc_multi::dump_util(const std::string& filename) {}

void lsc_multi::rollover(double timestamp)
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

void lsc_multi::dump_usage()
{
  std::cerr << "[";
  for (auto& segment : segments) 
    std::cerr << (bool(segment) ? "X" : "_");
  std::cerr << "]" << std::endl;
}

double lsc_multi::get_running_hit_rate() {
  return double(stat.hits) / stat.accesses;
}

auto lsc_multi::choose_cleaning_sources() -> std::vector<segment*>
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

auto lsc_multi::choose_cleaning_sources_random() -> std::vector<segment*>
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

auto lsc_multi::choose_cleaning_sources_oldest_item() -> std::vector<segment*>
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

auto lsc_multi::choose_cleaning_sources_rumble() -> std::vector<segment*>
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

auto lsc_multi::choose_cleaning_sources_round_robin() -> std::vector<segment*>
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


auto lsc_multi::choose_cleaning_destination() -> segment* {
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

void lsc_multi::dump_cleaning_plan(std::vector<segment*> srcs,
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

auto lsc_multi::find_app_most_in_need() -> application* {
  application* r = nullptr;
  for (auto& p : apps) {
    application& app = p.second;
    if (app.target_mem + app.credit_bytes <= app.min_mem)
      continue;
    if (app.cleaning_it == app.cleaning_q.end())
      continue;
    if (r == nullptr || app.need() > r->need())
      r = &app;
  }
  return r;
}

void lsc_multi::clean()
{
  /*
  const char* spinner = "|/-\\";
  static uint8_t last = 0;
  std::cerr << spinner[last++ & 0x03] << '\r';
  */

  std::vector<segment*> src_segments = choose_cleaning_sources();

  // Push all items into the app-specific cleaning queues.
  for (segment* segment : src_segments) {
    for (auto& item : segment->queue) {
      auto appit = apps.find(item.req.appid);
      assert(appit != apps.end());
      application& app = appit->second;

      // Remove all objects in the segments to clean from the apps
      // memory budget. Will get re-added for each object that gets
      // 'salvaged'.
      app.bytes_in_use -= item.req.size();

      // Check to see if this version is still needed.
      auto it = map.find(item.req.kid);
      // If not in the hash table, just drop it.
      if (it == map.end())
        continue;
      // If hash table pointer refers to a different version, drop this one.
      if (&*it->second != &item)
        continue;

      app.add_to_cleaning_queue(&item);
    }
  }

  // Sort each app's queue and reset its cleaning
  // iterator.
  for (auto& p : apps) {
    application& app = p.second;
    app.sort_cleaning_queue();
  }

  std::cerr << std::endl;

  size_t dst_index = 0;
  segment* dst = choose_cleaning_destination();
  std::vector<segment*> dst_segments{};
  dst_segments.push_back(dst);

  while (true) {
    application* selected_app = find_app_most_in_need();

    // All done with all its.
    if (!selected_app) {
      stat.cleaned_ext_frag_bytes += (stat.segment_size - dst->filled_bytes);
      ++stat.cleaned_generated_segs;

      break;
    }

    item* item = *selected_app->cleaning_it;

    auto it = map.find(item->req.kid);

    // Should exist else we would have thrown it out in the first
    // pass constructing the per-app lists.
    if (it == map.end()) {
      std::cerr << "Found dead object during cleaning" << std::endl;
      item->req.dump();
    }
    assert(it != map.end());
    // If hash table pointer refers to a different version, drop this one.
    if (&*it->second != item) {
      ++selected_app->cleaning_it;
      continue;
    }

    // Check to see if there is room for the item in the dst.
    if (dst->filled_bytes + item->req.size() > stat.segment_size) {
      //std::cerr << "Couldn't put in item of size "
                //<< item->req.size() << " bytes" << std::endl;
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
    ++selected_app->cleaning_it;

    // Relocate *to the back* to retain timestamp sort order.
    dst->queue.emplace_back(dst, item->req);
    map[item->req.kid] = (--dst->queue.end());
    dst->filled_bytes += item->req.size();
    dst->low_timestamp = item->req.time;
    
    // Bill the app that hosts it for the space use for preserving this.
    selected_app->bytes_in_use += item->req.size();
  }

  // Clear items that are going to get thrown on the floor from the hashtable.
  for (auto& p : apps) {
    application& app = p.second;
    while (app.cleaning_it != app.cleaning_q.end()) {
      // Need to do a double check here. The item we are evicting may
      // still exist in the hash table but it may point to a newer version
      // of this object. In that case, skip the erase from the hash table.
      auto hash_it = map.find((*app.cleaning_it)->req.kid);
      if (hash_it != map.end()) {
        item* from_list = *app.cleaning_it;
        item* from_hash = &*hash_it->second;

        if (from_list == from_hash) {
          map.erase((*app.cleaning_it)->req.kid);
          ++stat.evicted_items;
          stat.evicted_bytes += from_list->req.size();
        }
      }

      ++app.cleaning_it;
    }
    assert(app.cleaning_it == app.cleaning_q.end());
    app.cleaning_q.clear();
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


