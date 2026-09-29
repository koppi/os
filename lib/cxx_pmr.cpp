/**
 * cxx_pmr.cpp -- NOT Qt source at all; a minimal freestanding
 * implementation of the small slice of C++17 <memory_resource> that
 * qtbase/src/corelib/tools/qduplicatetracker_p.h uses
 * (std::pmr::monotonic_buffer_resource over a small stack buffer, for a
 * fast "have I seen this element before" check).
 *
 * This is standard-library runtime support, not Qt code: this kernel
 * links no libstdc++ at all (matching lib/cxxabi.cpp's aligned new/
 * delete and lib/cxx_chrono.cpp's <chrono> support, both real-repo
 * precedent for the same situation), and the host toolchain's own
 * prebuilt libstdc++.a's memory_resource.o pulls in real exception
 * unwinding (__cxa_begin_catch/_Unwind_Resume/__gxx_personality_v0) and
 * RTTI vtables for the pool-resource classes bundled in the same
 * translation unit -- both fundamentally incompatible with this kernel's
 * -fno-exceptions -fno-rtti build. <memory_resource>'s own header
 * declares memory_resource::~memory_resource() and
 * monotonic_buffer_resource::~monotonic_buffer_resource() as each
 * class's "key function" (the one non-inline virtual whose translation
 * unit is where the vtable gets emitted) and forward-declares
 * monotonic_buffer_resource::_Chunk without defining it -- it is a
 * private implementation detail with no fixed public layout, so this
 * file is free to define it however it likes; nothing outside this file
 * ever touches a _Chunk directly.
 *
 * Everything else declared in <memory_resource> (allocate()/
 * deallocate()/is_equal(), all three constructors QDuplicateTracker can
 * reach, and monotonic_buffer_resource's own do_allocate/do_deallocate/
 * do_is_equal/release()) is already fully inline in the header; only
 * the four symbols below are ever emitted out-of-line.
 */
#include <memory_resource>
#include <new>

namespace std {
namespace pmr {

memory_resource::~memory_resource() = default;

namespace {
class NewDeleteResource : public memory_resource
{
    void *do_allocate(size_t bytes, size_t alignment) override
    {
        return ::operator new(bytes, std::align_val_t(alignment));
    }
    void do_deallocate(void *p, size_t bytes, size_t alignment) override
    {
        ::operator delete(p, bytes, std::align_val_t(alignment));
    }
    bool do_is_equal(const memory_resource &other) const noexcept override
    {
        return this == &other;
    }
};
NewDeleteResource g_newDeleteResource;
}

memory_resource *get_default_resource() noexcept
{
    return &g_newDeleteResource;
}

class monotonic_buffer_resource::_Chunk
{
public:
    _Chunk *next;
    size_t size;
    size_t alignment;
};

void monotonic_buffer_resource::_M_new_buffer(size_t bytes, size_t alignment)
{
    size_t bufsiz = _M_next_bufsiz > bytes ? _M_next_bufsiz : bytes;
    size_t headerAlign = alignof(_Chunk) > alignment ? alignof(_Chunk) : alignment;
    size_t headerSize = (sizeof(_Chunk) + headerAlign - 1) & ~(headerAlign - 1);
    size_t total = headerSize + bufsiz;

    void *raw = _M_upstream->allocate(total, headerAlign);
    _Chunk *chunk = static_cast<_Chunk *>(raw);
    chunk->next = _M_head;
    chunk->size = total;
    chunk->alignment = headerAlign;
    _M_head = chunk;

    _M_current_buf = static_cast<char *>(raw) + headerSize;
    _M_avail = bufsiz;
    _M_next_bufsiz = static_cast<size_t>(bufsiz * 1.5);
}

void monotonic_buffer_resource::_M_release_buffers() noexcept
{
    _Chunk *chunk = _M_head;
    while (chunk) {
        _Chunk *next = chunk->next;
        _M_upstream->deallocate(chunk, chunk->size, chunk->alignment);
        chunk = next;
    }
    _M_head = nullptr;
}

monotonic_buffer_resource::~monotonic_buffer_resource()
{
    release();
}

} // namespace pmr
} // namespace std
