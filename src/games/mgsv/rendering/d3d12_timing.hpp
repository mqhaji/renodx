// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_RENDERING_D3D12_TIMING_HPP_
#define RENODX_GAMES_MGSV_RENDERING_D3D12_TIMING_HPP_

// GPU timestamps for the private D3D12 work, recorded only while a diagnostic
// client has asked for them. Ordinary rendering issues no queries.

#include <d3d12.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <shared_mutex>

namespace mgsv::rendering::d3d12_timing {

// Each bridge consumer is one stream. The consumer names the spans of its own
// submission, in recording order.
enum class Stream : uint8_t {
  LIGHTING,
  RECONSTRUCTION,
  COUNT,
};
inline constexpr size_t STREAM_COUNT = static_cast<size_t>(Stream::COUNT);
inline constexpr uint32_t MAX_SPANS = 47u;

// Plain data copied to the client. No pointers or owning objects cross modules.
struct Span {
  std::array<char, 40> name = {};
  double sum_ms = 0.;
  double min_ms = 0.;
  double max_ms = 0.;
};
struct StreamReport {
  uint32_t submissions = 0u;
  uint32_t span_count = 0u;
  // First to last timestamp of each submission.
  Span total;
  std::array<Span, MAX_SPANS> spans;
};
struct Report {
  std::array<StreamReport, STREAM_COUNT> streams;
  // Submissions each stream has still to begin timing.
  std::array<uint32_t, STREAM_COUNT> remaining = {};
};

namespace internal {

inline constexpr uint32_t RANGE_COUNT = 8u;
inline constexpr uint32_t RANGE_QUERIES = MAX_SPANS + 1u;

struct Range {
  enum class State : uint8_t {
    FREE,
    RECORDING,
    RECORDED,
    SUBMITTED,
  };
  State state = State::FREE;
  Stream stream = Stream::LIGHTING;
  uint32_t timestamps = 0u;
  // The bridge's D3D12 fence value that covers this range's command list.
  uint64_t completion = 0u;
  // String literals or SDK-owned names, which outlive the queries.
  std::array<const char*, MAX_SPANS> names = {};
};

struct State {
  // Guards the report. The bridge's caller already serializes everything else.
  std::shared_mutex mutex;
  Report report;
  Microsoft::WRL::ComPtr<ID3D12QueryHeap> heap;
  Microsoft::WRL::ComPtr<ID3D12Resource> readback;
  std::array<Range, RANGE_COUNT> ranges;
  uint64_t frequency = 0u;
};
static State state;
static std::array<std::atomic<uint32_t>, STREAM_COUNT> remaining = {};

}  // namespace internal

// Times the next submissions of every stream and starts a new report.
inline void Request(uint32_t submissions) {
  const std::unique_lock lock(internal::state.mutex);
  internal::state.report = {};
  for (auto& counter : internal::remaining) {
    counter.store(submissions, std::memory_order_release);
  }
}

inline void Read(Report* report) {
  const std::shared_lock lock(internal::state.mutex);
  *report = internal::state.report;
  for (size_t stream = 0u; stream < STREAM_COUNT; ++stream) {
    report->remaining[stream] = internal::remaining[stream].load(std::memory_order_acquire);
  }
}

// Brackets one recorded command list. It does nothing unless its stream has
// timed submissions left and a query range is free.
class Scope {
 public:
  Scope(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12GraphicsCommandList* commands, Stream stream)
      : commands(commands), counter(&internal::remaining[static_cast<size_t>(stream)]) {
    if (counter->load(std::memory_order_acquire) == 0u) return;
    auto& state = internal::state;
    if (state.heap == nullptr) {
      const D3D12_QUERY_HEAP_DESC heap = {.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = internal::RANGE_COUNT * internal::RANGE_QUERIES};
      const D3D12_HEAP_PROPERTIES memory = {.Type = D3D12_HEAP_TYPE_READBACK};
      const D3D12_RESOURCE_DESC buffer = {
          .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Width = heap.Count * sizeof(uint64_t),
          .Height = 1u,
          .DepthOrArraySize = 1u,
          .MipLevels = 1u,
          .SampleDesc = {.Count = 1u},
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      };
      if (FAILED(queue->GetTimestampFrequency(&state.frequency)) || state.frequency == 0u
          || FAILED(device->CreateQueryHeap(&heap, IID_PPV_ARGS(&state.heap)))
          || FAILED(device->CreateCommittedResource(&memory, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                                    nullptr, IID_PPV_ARGS(&state.readback)))) {
        // Timing is unavailable on this device. Stop asking for it.
        state.heap.Reset();
        state.readback.Reset();
        for (auto& pending : internal::remaining) {
          pending.store(0u, std::memory_order_release);
        }
        return;
      }
    }
    // Recording and submission alternate, so a range still waiting for its
    // submission belongs to a list that was never submitted.
    for (auto& candidate : state.ranges) {
      if (candidate.state == internal::Range::State::RECORDING || candidate.state == internal::Range::State::RECORDED) {
        candidate.state = internal::Range::State::FREE;
      }
    }
    const auto free = std::ranges::find(state.ranges, internal::Range::State::FREE, &internal::Range::state);
    if (free == state.ranges.end()) return;
    counter->fetch_sub(1u, std::memory_order_acq_rel);
    range = &*free;
    *range = {.state = internal::Range::State::RECORDING, .stream = stream};
    Stamp();
  }

  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;

  ~Scope() {
    if (range == nullptr) return;
    // The recording was abandoned. Time another submission in its place.
    range->state = internal::Range::State::FREE;
    counter->fetch_add(1u, std::memory_order_acq_rel);
  }

  // Ends the span of the work recorded since the previous timestamp.
  void Mark(const char* name) {
    if (range == nullptr || range->timestamps > MAX_SPANS) return;
    range->names[range->timestamps - 1u] = name;
    Stamp();
  }

  // Resolves the timestamps. Call after the last Mark, before the list closes.
  void Finish() {
    if (range == nullptr) return;
    const UINT first = First();
    commands->ResolveQueryData(internal::state.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, range->timestamps,
                               internal::state.readback.Get(), first * sizeof(uint64_t));
    range->state = internal::Range::State::RECORDED;
    range = nullptr;
  }

 private:
  [[nodiscard]] UINT First() const {
    return static_cast<UINT>(range - internal::state.ranges.data()) * internal::RANGE_QUERIES;
  }

  void Stamp() {
    commands->EndQuery(internal::state.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, First() + range->timestamps);
    ++range->timestamps;
  }

  ID3D12GraphicsCommandList* commands;
  std::atomic<uint32_t>* counter;
  internal::Range* range = nullptr;
};

// The bridge calls this once its queue has a fence value covering the list
// that was just recorded.
inline void Submitted(uint64_t completion) {
  for (auto& range : internal::state.ranges) {
    if (range.state == internal::Range::State::RECORDED) {
      range.state = internal::Range::State::SUBMITTED;
      range.completion = completion;
    }
  }
}

// Reads every range whose command list the GPU has finished.
inline void Collect(uint64_t completed) {
  auto& state = internal::state;
  if (state.readback == nullptr) return;
  for (size_t index = 0u; index < state.ranges.size(); ++index) {
    auto& range = state.ranges[index];
    if (range.state != internal::Range::State::SUBMITTED || range.completion > completed) continue;
    range.state = internal::Range::State::FREE;
    const size_t first = index * internal::RANGE_QUERIES * sizeof(uint64_t);
    const D3D12_RANGE read = {.Begin = first, .End = first + (range.timestamps * sizeof(uint64_t))};
    void* mapped = nullptr;
    if (FAILED(state.readback->Map(0u, &read, &mapped))) continue;
    std::array<uint64_t, internal::RANGE_QUERIES> ticks = {};
    std::memcpy(ticks.data(), static_cast<const std::byte*>(mapped) + first, range.timestamps * sizeof(uint64_t));
    const D3D12_RANGE written = {};
    state.readback->Unmap(0u, &written);

    const std::unique_lock lock(state.mutex);
    auto& stream = state.report.streams[static_cast<size_t>(range.stream)];
    const auto add = [first_submission = stream.submissions == 0u, frequency = state.frequency](Span* span, const char* name, uint64_t begin, uint64_t end) {
      const double milliseconds = (end > begin ? static_cast<double>(end - begin) * 1000. / static_cast<double>(frequency) : 0.);
      std::snprintf(span->name.data(), span->name.size(), "%s", name != nullptr ? name : "");
      span->sum_ms += milliseconds;
      span->min_ms = (first_submission ? milliseconds : std::min(span->min_ms, milliseconds));
      span->max_ms = std::max(span->max_ms, milliseconds);
    };
    stream.span_count = range.timestamps - 1u;
    for (uint32_t span = 0u; span < stream.span_count; ++span) {
      add(&stream.spans[span], range.names[span], ticks[span], ticks[span + 1u]);
    }
    add(&stream.total, "total", ticks[0], ticks[range.timestamps - 1u]);
    ++stream.submissions;
  }
}

// Call before the D3D12 device is released.
inline void Release() {
  for (auto& counter : internal::remaining) {
    counter.store(0u, std::memory_order_release);
  }
  internal::state.ranges = {};
  internal::state.heap.Reset();
  internal::state.readback.Reset();
}

}  // namespace mgsv::rendering::d3d12_timing

#endif  // RENODX_GAMES_MGSV_RENDERING_D3D12_TIMING_HPP_
