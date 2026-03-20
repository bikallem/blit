# blit

Fast byte-level blit, fill, match, and uninit allocation for MoonBit.

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

### Uninit Allocation

Allocate without zeroing memory. The caller **must** fully initialize all elements before reading.

```moonbit
@blit.make_uninit(len)      // -> FixedArray[Byte]
@blit.make_uninit_int(len)  // -> FixedArray[Int]
@blit.make_uninit_uint(len) // -> FixedArray[UInt]
```

## License

Apache-2.0
