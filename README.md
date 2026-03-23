# blit

Fast byte-level blit, fill, match, uninit allocation, and growable byte buffer for MoonBit.

On the native backend, operations use C FFI (`memmove`, `memset`, 8-byte chunk matching) for vectorized performance. On JS/Wasm backends, pure MoonBit fallback implementations are used.

## Install

```
moon add bikallem/blit
```

## API

### Blit

```moonbit
// Copy between FixedArray[Byte] buffers (safe for overlapping regions)
@blit.blit_fixed_array(dst, dst_offset, src, src_offset, length)

// Copy from Bytes into FixedArray[Byte]
@blit.blit_bytes(dst, dst_offset, src, src_offset, length)

// Copy from BytesView into FixedArray[Byte]
@blit.blit_bytesview(dst, dst_offset, src, src_offset, length)
```

### Fill

```moonbit
// Fill a region with a single byte value
@blit.fill_bytes(dst, dst_offset, value, length)
```

### Match Length

```moonbit
// Count matching bytes between two FixedArray[Byte] regions
@blit.match_length(a, a_offset, b, b_offset, max_len)

// Count matching bytes between two Bytes regions
@blit.match_length_bytes(a, a_offset, b, b_offset, max_len)

// Count matching bytes within a BytesView
@blit.match_length_bv(src, a, b, max_len)
```

### Buffer

Growable byte buffer backed by `FixedArray[Byte]`. All internal access is bounds-check free. Uses uninit allocation and blit-based growth.

```moonbit
let buf = @blit.Buffer::new(size_hint=1024)

// Write
buf.write_byte(b'a')
buf.write_bytes(b"hello")
buf.write_bytesview(some_bytes[1:4])
buf.write_fixed(fixed_arr, offset, len)

// Unchecked write (caller must ensure capacity)
buf.ensure_capacity(10)
buf.write_byte_unchecked(b'x')

// Read
buf.get_byte(0)         // bounds-check free read
buf.length()            // current byte count
buf.data()              // backing FixedArray[Byte]

// Internal copy (handles overlapping regions)
buf.copy_from_self(src_pos, len)

// Finalize: make_uninit(pos) + blit + unsafe_reinterpret_as_bytes
let bytes = buf.to_bytes()

// Reuse allocated memory
buf.reset()
```

### Uninit Allocation

Allocate without zeroing memory. The caller **must** fully initialize all elements before reading.

```moonbit
@blit.make_uninit(len)      // -> FixedArray[Byte]
@blit.make_uninit_int(len)  // -> FixedArray[Int]
@blit.make_uninit_uint(len) // -> FixedArray[UInt]
```

## License

Apache-2.0
