/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/DrawBufferManager.h"

#include "experimental/graphite/src/Buffer.h"
#include "experimental/graphite/src/CommandBuffer.h"
#include "experimental/graphite/src/ResourceProvider.h"

namespace skgpu {

namespace {

// TODO: Tune these values on real world data
static constexpr size_t kVertexBufferSize = 16 << 10; // 16 KB
static constexpr size_t kIndexBufferSize =   2 << 10; //  2 KB
static constexpr size_t kUniformBufferSize = 2 << 10; //  2 KB

void* map_offset(BindBufferInfo binding) {
    // DrawBufferManager owns the Buffer, and this is only ever called when we know
    // it's okay to remove 'const' from the Buffer*
    return SkTAddOffset<void>(const_cast<Buffer*>(binding.fBuffer)->map(),
                              static_cast<ptrdiff_t>(binding.fOffset));
}

template <size_t BufferBlockSize>
size_t sufficient_block_size(size_t requiredBytes) {
    // Always request a buffer at least 'requiredBytes', but keep them in multiples of
    // 'BufferBlockSize' for improved reuse.
    static constexpr size_t kMaxSize   = std::numeric_limits<size_t>::max();
    static constexpr size_t kMaxBlocks = kMaxSize / BufferBlockSize;
    size_t blocks = (requiredBytes / BufferBlockSize) + 1;
    size_t bufferSize = blocks > kMaxBlocks ? kMaxSize : (blocks * BufferBlockSize);
    SkASSERT(requiredBytes < bufferSize);
    return bufferSize;
}

} // anonymous namespace

DrawBufferManager::DrawBufferManager(ResourceProvider* resourceProvider,
                                     size_t uniformStartAlignment)
        : fResourceProvider(resourceProvider)
        , fUniformStartAlignment(uniformStartAlignment) {}

DrawBufferManager::~DrawBufferManager() {}

static bool can_fit(size_t requestedSize,
                    Buffer* buffer,
                    size_t currentOffset,
                    size_t alignment) {
    size_t startOffset = SkAlignTo(currentOffset, alignment);
    return requestedSize <= (buffer->size() - startOffset);
}

std::tuple<VertexWriter, BindBufferInfo> DrawBufferManager::getVertexWriter(size_t requiredBytes) {
    if (!requiredBytes) {
        return {VertexWriter(), BindBufferInfo()};
    }
    if (fCurrentVertexBuffer &&
        !can_fit(requiredBytes, fCurrentVertexBuffer.get(), fVertexOffset, /*alignment=*/1)) {
        fUsedBuffers.push_back(std::move(fCurrentVertexBuffer));
    }

    if (!fCurrentVertexBuffer) {
        size_t bufferSize = sufficient_block_size<kVertexBufferSize>(requiredBytes);
        fCurrentVertexBuffer = fResourceProvider->findOrCreateBuffer(bufferSize,
                                                                     BufferType::kVertex,
                                                                     PrioritizeGpuReads::kNo);
        fVertexOffset = 0;
        if (!fCurrentVertexBuffer) {
            return {VertexWriter(), BindBufferInfo()};
        }
    }
    BindBufferInfo bindInfo;
    bindInfo.fBuffer = fCurrentVertexBuffer.get();
    bindInfo.fOffset = fVertexOffset;
    fVertexOffset += requiredBytes;
    return {VertexWriter(map_offset(bindInfo), requiredBytes), bindInfo};
}

void DrawBufferManager::returnVertexBytes(size_t unusedBytes) {
    SkASSERT(fVertexOffset >= unusedBytes);
    fVertexOffset -= unusedBytes;
}

std::tuple<IndexWriter, BindBufferInfo> DrawBufferManager::getIndexWriter(size_t requiredBytes) {
    if (!requiredBytes) {
        return {IndexWriter(), BindBufferInfo()};
    }
    if (fCurrentIndexBuffer &&
        !can_fit(requiredBytes, fCurrentIndexBuffer.get(), fIndexOffset, /*alignment=*/1)) {
        fUsedBuffers.push_back(std::move(fCurrentIndexBuffer));
    }

    if (!fCurrentIndexBuffer) {
        size_t bufferSize = sufficient_block_size<kIndexBufferSize>(requiredBytes);
        fCurrentIndexBuffer = fResourceProvider->findOrCreateBuffer(bufferSize,
                                                                    BufferType::kIndex,
                                                                    PrioritizeGpuReads::kNo);
        fIndexOffset = 0;
        if (!fCurrentIndexBuffer) {
            return {IndexWriter(), BindBufferInfo()};
        }
    }
    BindBufferInfo bindInfo;
    bindInfo.fBuffer = fCurrentIndexBuffer.get();
    bindInfo.fOffset = fIndexOffset;
    fIndexOffset += requiredBytes;
    return {IndexWriter(map_offset(bindInfo), requiredBytes), bindInfo};
}

std::tuple<UniformWriter, BindBufferInfo> DrawBufferManager::getUniformWriter(
        size_t requiredBytes) {
    if (!requiredBytes) {
        return {UniformWriter(), BindBufferInfo()};
    }
    if (fCurrentUniformBuffer &&
        !can_fit(requiredBytes,
                 fCurrentUniformBuffer.get(),
                 fUniformOffset,
                 fUniformStartAlignment)) {
        fUsedBuffers.push_back(std::move(fCurrentUniformBuffer));
    }

    if (!fCurrentUniformBuffer) {
        size_t bufferSize = sufficient_block_size<kUniformBufferSize>(requiredBytes);
        fCurrentUniformBuffer = fResourceProvider->findOrCreateBuffer(bufferSize,
                                                                      BufferType::kUniform,
                                                                      PrioritizeGpuReads::kNo);
        fUniformOffset = 0;
        if (!fCurrentUniformBuffer) {
            return {UniformWriter(), BindBufferInfo()};
        }
    }
    fUniformOffset = SkAlignTo(fUniformOffset, fUniformStartAlignment);
    BindBufferInfo bindInfo;
    bindInfo.fBuffer = fCurrentUniformBuffer.get();
    bindInfo.fOffset = fUniformOffset;
    fUniformOffset += requiredBytes;
    return {UniformWriter(map_offset(bindInfo), requiredBytes), bindInfo};
}

void DrawBufferManager::transferToCommandBuffer(CommandBuffer* commandBuffer) {
    for (auto& buffer : fUsedBuffers) {
        buffer->unmap();
        commandBuffer->trackResource(std::move(buffer));
    }
    fUsedBuffers.clear();
}

} // namespace skgpu
