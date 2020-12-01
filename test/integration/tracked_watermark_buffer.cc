#include "test/integration/tracked_watermark_buffer.h"

namespace Envoy {
namespace Buffer {

TrackedWatermarkBufferFactory::~TrackedWatermarkBufferFactory() {
  ASSERT(active_buffer_count_ == 0);
}

Buffer::InstancePtr TrackedWatermarkBufferFactory::create(
    BufferType buffer_type, std::function<void()> below_low_watermark,
    std::function<void()> above_high_watermark, std::function<void()> above_overflow_watermark) {
  absl::MutexLock lock(&mutex_);
  uint64_t id = next_id_++;
  ++active_buffer_count_;
  BufferInfo& buffer_info = max_sizes_[id];
  buffer_info.buffer_type_ = buffer_type;
  return std::make_unique<TrackedWatermarkBuffer>(
      [this, &buffer_info](uint64_t current_size) {
        absl::MutexLock lock(&mutex_);
        if (buffer_info.max_size_ < current_size) {
          buffer_info.max_size_ = current_size;
        }
      },
      [this]() {
        absl::MutexLock lock(&mutex_);
        --active_buffer_count_;
      },
      below_low_watermark, above_high_watermark, above_overflow_watermark);
}

uint64_t
TrackedWatermarkBufferFactory::numBuffersCreated(absl::optional<Buffer::BufferType> type) const {
  absl::MutexLock lock(&mutex_);
  uint64_t count = 0;
  for (auto& item : max_sizes_) {
    if (!type || item.second.buffer_type_ == type) {
      ++count;
    }
  }
  return count;
}

uint64_t TrackedWatermarkBufferFactory::numBuffersActive() const {
  absl::MutexLock lock(&mutex_);
  return active_buffer_count_;
}

uint64_t
TrackedWatermarkBufferFactory::maxBufferSize(absl::optional<Buffer::BufferType> type) const {
  absl::MutexLock lock(&mutex_);
  uint64_t val = 0;
  for (auto& item : max_sizes_) {
    if (!type || item.second.buffer_type_ == type) {
      val = std::max(val, item.second.max_size_);
    }
  }
  return val;
}

uint64_t
TrackedWatermarkBufferFactory::sumMaxBufferSizes(absl::optional<Buffer::BufferType> type) const {
  absl::MutexLock lock(&mutex_);
  uint64_t val = 0;
  for (auto& item : max_sizes_) {
    ENVOY_LOG_MISC(critical, "{} {}", static_cast<int>(item.second.buffer_type_),
                   item.second.max_size_);
    if (!type || item.second.buffer_type_ == type) {
      val += item.second.max_size_;
    }
  }
  return val;
}

} // namespace Buffer
} // namespace Envoy
