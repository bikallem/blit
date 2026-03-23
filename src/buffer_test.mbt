///|
test "Buffer::new creates empty buffer" {
  let buf = @blit.Buffer::new()
  assert_eq(buf.length(), 0)
}

///|
test "Buffer::write_byte and to_bytes" {
  let buf = @blit.Buffer::new(size_hint=4)
  buf.write_byte(b'a')
  buf.write_byte(b'b')
  buf.write_byte(b'c')
  assert_eq(buf.length(), 3)
  assert_eq(buf.to_bytes(), b"abc")
}

///|
test "Buffer::write_bytes" {
  let buf = @blit.Buffer::new(size_hint=4)
  buf.write_bytes(b"hello")
  assert_eq(buf.to_bytes(), b"hello")
}

///|
test "Buffer::write_bytesview" {
  let src : Bytes = b"abcde"
  let buf = @blit.Buffer::new(size_hint=4)
  buf.write_bytesview(src[1:4])
  assert_eq(buf.to_bytes(), b"bcd")
}

///|
test "Buffer::write_fixed" {
  let src : FixedArray[Byte] = [b'x', b'y', b'z']
  let buf = @blit.Buffer::new(size_hint=4)
  buf.write_fixed(src, 1, 2)
  assert_eq(buf.to_bytes(), b"yz")
}

///|
test "Buffer grows on demand" {
  let buf = @blit.Buffer::new(size_hint=2)
  for i in 0..<100 {
    buf.write_byte((i % 256).to_byte())
  }
  assert_eq(buf.length(), 100)
  let bytes = buf.to_bytes()
  for i in 0..<100 {
    assert_eq(bytes[i], (i % 256).to_byte())
  }
}

///|
test "Buffer::get_byte" {
  let buf = @blit.Buffer::new(size_hint=4)
  buf.write_bytes(b"abc")
  assert_eq(buf.get_byte(0), b'a')
  assert_eq(buf.get_byte(1), b'b')
  assert_eq(buf.get_byte(2), b'c')
}

///|
test "Buffer::copy_from_self non-overlapping" {
  let buf = @blit.Buffer::new(size_hint=16)
  buf.write_bytes(b"abcd")
  buf.copy_from_self(0, 4)
  assert_eq(buf.to_bytes(), b"abcdabcd")
}

///|
test "Buffer::copy_from_self overlapping (pattern repeat)" {
  let buf = @blit.Buffer::new(size_hint=16)
  buf.write_bytes(b"ab")
  // distance=2, len=6 -> overlapping, should repeat "ab" pattern
  buf.copy_from_self(0, 6)
  assert_eq(buf.to_bytes(), b"abababab")
}

///|
test "Buffer::reset reuses memory" {
  let buf = @blit.Buffer::new(size_hint=4)
  buf.write_bytes(b"hello")
  let cap_before = buf.data().length()
  buf.reset()
  assert_eq(buf.length(), 0)
  assert_eq(buf.data().length(), cap_before)
  buf.write_bytes(b"world")
  assert_eq(buf.to_bytes(), b"world")
}

///|
test "Buffer::write_byte_unchecked" {
  let buf = @blit.Buffer::new(size_hint=4)
  buf.ensure_capacity(3)
  buf.write_byte_unchecked(b'x')
  buf.write_byte_unchecked(b'y')
  buf.write_byte_unchecked(b'z')
  assert_eq(buf.to_bytes(), b"xyz")
}
