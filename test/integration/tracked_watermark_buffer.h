#pragma once

#include "common/buffer/watermark_buffer.h"

#include "absl/container/node_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Buffer {

class TrackedWatermarkBuffer : public Buffer::WatermarkBuffer {
public:
  TrackedWatermarkBuffer(std::function<void(uint64_t current_size)> update_max_size,
                         std::function<void()> on_delete, std::function<void()> below_low_watermark,
                         std::function<void()> above_high_watermark,
                         std::function<void()> above_overflow_watermark)
      : WatermarkBuffer(below_low_watermark, above_high_watermark, above_overflow_watermark),
        update_max_size_(update_max_size), on_delete_(on_delete) {}
  ~TrackedWatermarkBuffer() override { on_delete_(); }

protected:
  void checkHighAndOverflowWatermarks() override {
    update_max_size_(length());
    WatermarkBuffer::checkHighAndOverflowWatermarks();
  }

private:
  std::function<void(uint64_t current_size)> update_max_size_;
  std::function<void()> on_delete_;
};

class TrackedWatermarkBufferFactory : public Buffer::WatermarkFactory {
public:
  TrackedWatermarkBufferFactory() = default;
  ~TrackedWatermarkBufferFactory() override;
  // Buffer::WatermarkFactory
  Buffer::InstancePtr create(BufferType buffer_type, std::function<void()> below_low_watermark,
                             std::function<void()> above_high_watermark,
                             std::function<void()> above_overflow_watermark) override;

  // Number of buffers created.
  uint64_t numBuffersCreated(absl::optional<Buffer::BufferType> type = absl::nullopt) const;
  // Number of buffers still in use.
  uint64_t numBuffersActive() const;
  // Size of the largest buffer.
  uint64_t maxBufferSize(absl::optional<Buffer::BufferType> type = absl::nullopt) const;
  // Sum of the max size of all known buffers.
  uint64_t sumMaxBufferSizes(absl::optional<Buffer::BufferType> type = absl::nullopt) const;

private:
  struct BufferInfo {
    BufferType buffer_type_;
    uint64_t max_size_ = 0;
  };

  mutable absl::Mutex mutex_;
  // Id of the next buffer to create.
  uint64_t next_id_ ABSL_GUARDED_BY(mutex_) = 0;
  // Number of buffers currently in existence.
  uint64_t active_buffer_count_ ABSL_GUARDED_BY(mutex_) = 0;
  // Max size of buffers, by id.
  absl::node_hash_map<uint64_t, BufferInfo> max_sizes_ ABSL_GUARDED_BY(mutex_);
};

} // namespace Buffer
} // namespace Envoy
