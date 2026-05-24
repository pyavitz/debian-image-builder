import pytest
import struct
import ctypes


# Simulated constants matching kernel/driver context
KP_RULE_BUFFER_SIZE = 256  # Simulated fixed allocation size for kp_set.rule[idx]
KP_HEADER_SIZE = 16        # Simulated sizeof(*kp) - header structure size


def simulate_kp_struct(payload_len, payload_data=None):
    """Simulate a kp (key policy) structure with header + payload."""
    if payload_data is None:
        payload_data = b'\x41' * min(payload_len, 4096)
    # Pack: [payload_len (4 bytes)] + [other_header_fields (12 bytes)] + [payload]
    header = struct.pack('<I', payload_len) + b'\x00' * (KP_HEADER_SIZE - 4)
    return header + payload_data[:payload_len]


def safe_copy_with_validation(kp_raw_data, dest_buffer_size):
    """
    Simulates the memcpy operation with proper validation.
    Returns (success, bytes_copied, error_msg)
    
    Security invariant: payload_len + sizeof(*kp) MUST NOT exceed dest_buffer_size
    """
    if len(kp_raw_data) < KP_HEADER_SIZE:
        return False, 0, "Input too small to contain header"
    
    # Extract payload_len from the structure (first 4 bytes)
    payload_len = struct.unpack('<I', kp_raw_data[:4])[0]
    
    # The copy size as computed in vulnerable code: payload_len + sizeof(*kp)
    copy_size = payload_len + KP_HEADER_SIZE
    
    # SECURITY INVARIANT: copy_size must not exceed destination buffer size
    if copy_size > dest_buffer_size:
        return False, 0, f"Buffer overflow prevented: copy_size={copy_size} > dest_size={dest_buffer_size}"
    
    # Also check for integer overflow: payload_len + KP_HEADER_SIZE must not wrap
    if copy_size < payload_len or copy_size < KP_HEADER_SIZE:
        return False, 0, f"Integer overflow detected: payload_len={payload_len}"
    
    # Simulate the copy
    dest = bytearray(dest_buffer_size)
    actual_copy = min(copy_size, len(kp_raw_data))
    dest[:actual_copy] = kp_raw_data[:actual_copy]
    
    return True, copy_size, None


@pytest.mark.parametrize("payload_len,description", [
    # Normal/boundary cases
    (0,                          "zero payload length"),
    (1,                          "minimal payload"),
    (KP_RULE_BUFFER_SIZE - KP_HEADER_SIZE - 1,  "one byte under limit"),
    (KP_RULE_BUFFER_SIZE - KP_HEADER_SIZE,       "exactly at limit"),
    
    # Overflow attempts
    (KP_RULE_BUFFER_SIZE - KP_HEADER_SIZE + 1,   "one byte over limit"),
    (KP_RULE_BUFFER_SIZE,                         "full buffer size as payload_len"),
    (KP_RULE_BUFFER_SIZE * 2,                     "double buffer size"),
    (0xFFFF,                                       "large 16-bit value"),
    (0xFFFFFF,                                     "large 24-bit value"),
    (0x7FFFFFFF,                                   "max signed 32-bit"),
    (0xFFFFFFFF,                                   "max unsigned 32-bit (wraps signed)"),
    (0xFFFFFFF0,                                   "near max to trigger integer overflow with sizeof"),
    (0xFFFFFFFF - KP_HEADER_SIZE + 1,             "value that overflows when adding sizeof(*kp)"),
    (0xFFFFFFFF - KP_HEADER_SIZE,                 "value at integer overflow boundary"),
    (65535,                                        "max 16-bit unsigned"),
    (65536,                                        "just over 16-bit max"),
    (1024 * 1024,                                  "1MB payload_len"),
    (0x80000000,                                   "sign bit set"),
    (0xDEADBEEF,                                   "magic adversarial value"),
    (0xCAFEBABE,                                   "another adversarial value"),
])
def test_memcpy_size_never_exceeds_buffer(payload_len, description):
    """
    Invariant: The computed copy size (payload_len + sizeof(*kp)) must NEVER exceed
    the allocated destination buffer size. Any user-controlled payload_len that would
    cause a buffer overflow must be rejected before the copy operation occurs.
    
    This guards against heap buffer overflow via unchecked ioctl input (CWE-122/CWE-190).
    """
    # Build adversarial kp structure
    safe_payload_data = b'\x41' * min(payload_len, 4096)  # Cap actual data for test
    kp_raw = simulate_kp_struct(payload_len, safe_payload_data)
    
    dest_buffer_size = KP_RULE_BUFFER_SIZE
    
    # Extract payload_len as it would be read from user input
    extracted_payload_len = struct.unpack('<I', kp_raw[:4])[0]
    
    # INVARIANT CHECK 1: Integer overflow detection
    # payload_len + sizeof(*kp) must not wrap around
    copy_size_unchecked = (extracted_payload_len + KP_HEADER_SIZE) & 0xFFFFFFFF
    
    if extracted_payload_len > 0xFFFFFFFF - KP_HEADER_SIZE:
        # This would cause integer overflow - must be caught
        assert copy_size_unchecked < extracted_payload_len or copy_size_unchecked < KP_HEADER_SIZE, \
            f"Integer overflow condition exists for payload_len={extracted_payload_len:#x} but was not detected"
        # The operation should be rejected
        success, _, error = safe_copy_with_validation(kp_raw, dest_buffer_size)
        assert not success, \
            f"SECURITY VIOLATION: Integer overflow not caught for payload_len={extracted_payload_len:#x}: {description}"
        return
    
    # INVARIANT CHECK 2: Buffer bounds enforcement
    copy_size = extracted_payload_len + KP_HEADER_SIZE
    
    if copy_size > dest_buffer_size:
        # Must be rejected - would overflow buffer
        success, bytes_copied, error = safe_copy_with_validation(kp_raw, dest_buffer_size)
        assert not success, \
            f"SECURITY VIOLATION: Buffer overflow not prevented! " \
            f"payload_len={extracted_payload_len}, copy_size={copy_size}, " \
            f"dest_size={dest_buffer_size}, description='{description}'"
        assert error is not None, \
            f"Error message must be set when rejecting oversized copy: {description}"
    else:
        # Should succeed - within bounds
        success, bytes_copied, error = safe_copy_with_validation(kp_raw, dest_buffer_size)
        assert success, \
            f"Valid copy was incorrectly rejected: payload_len={extracted_payload_len}, " \
            f"copy_size={copy_size}, dest_size={dest_buffer_size}, error={error}"
        assert bytes_copied <= dest_buffer_size, \
            f"SECURITY VIOLATION: bytes_copied={bytes_copied} exceeds dest_buffer_size={dest_buffer_size}"


@pytest.mark.parametrize("raw_bytes,description", [
    # Crafted raw byte sequences simulating ioctl input
    (b'\xff\xff\xff\xff' + b'\x00' * 12 + b'\x41' * 100, "max payload_len in header"),
    (b'\x00\x01\x00\x00' + b'\x00' * 12 + b'\x41' * 256, "256 byte payload_len"),
    (b'\xf0\xff\xff\xff' + b'\x00' * 12,                  "near-max payload_len, no data"),
    (b'\x01\x00\x00\x00' + b'\x00' * 12 + b'\x41',        "minimal valid structure"),
    (b'\x00\x00\x00\x80' + b'\x00' * 12,                  "sign bit set in payload_len"),
    (struct.pack('<I', KP_RULE_BUFFER_SIZE - KP_HEADER_SIZE + 1) + b'\x00' * 12, "off-by-one overflow"),
    (struct.pack('<I', 0xFFFFFFF0) + b'\x00' * 12,        "overflow when adding header size"),
])
def test_raw_ioctl_input_bounds_enforced(raw_bytes, description):
    """
    Invariant: Raw adversarial byte sequences from ioctl input must never result
    in a copy operation that writes beyond the allocated destination buffer.
    """
    dest_buffer_size = KP_RULE_BUFFER_SIZE
    
    if len(raw_bytes) < KP_HEADER_SIZE:
        pytest.skip(f"Input too short to parse header: {description}")
    
    payload_len = struct.unpack('<I', raw_bytes[:4])[0]
    
    # Check for integer overflow condition first
    if payload_len > 0xFFFFFFFF - KP_HEADER_SIZE:
        success, _, _ = safe_copy_with_validation(raw_bytes, dest_buffer_size)
        assert not success, \
            f"SECURITY VIOLATION: Integer overflow not caught for {description}"
        return
    
    copy_size = payload_len + KP_HEADER_SIZE
    success, bytes_copied, error = safe_copy_with_validation(raw_bytes, dest_buffer_size)
    
    if copy_size > dest_buffer_size:
        assert not success, \
            f"SECURITY VIOLATION: Oversized copy not rejected for {description}. " \
            f"payload_len={payload_len}, copy_size={copy_size}, dest={dest_buffer_size}"
    
    if success:
        assert bytes_copied <= dest_buffer_size, \
            f"SECURITY VIOLATION: Copy exceeded buffer! bytes_copied={bytes_copied}, " \
            f"dest_size={dest_buffer_size}, description={description}"


def test_copy_size_calculation_no_integer_overflow():
    """
    Invariant: The addition of payload_len + sizeof(*kp) must be checked for
    integer overflow before use as a copy size parameter.
    """
    # Values that would cause 32-bit integer overflow when added to KP_HEADER_SIZE
    overflow_values = [
        0xFFFFFFFF,
        0xFFFFFFFF - KP_HEADER_SIZE + 1,
        0xFFFFFFF0,
        0x80000000,
        2**32 - 1,
        2**32 - KP_HEADER_SIZE,
    ]
    
    for payload_len in overflow_values:
        # Simulate 32-bit arithmetic as in C
        result_32bit = (payload_len + KP_HEADER_SIZE) & 0xFFFFFFFF
        
        # If overflow occurred, result will be smaller than either operand
        overflow_occurred = result_32bit < payload_len or result_32bit < KP_HEADER_SIZE
        
        if overflow_occurred:
            # INVARIANT: This MUST be detected and rejected before memcpy
            kp_raw = struct.pack('<I', payload_len & 0xFFFFFFFF) + b'\x00' * (KP_HEADER_SIZE - 4)
            success, _, error = safe_copy_with_validation(kp_raw, KP_RULE_BUFFER_SIZE)
            assert not success, \
                f"SECURITY VIOLATION: Integer overflow not caught for payload_len={payload_len:#x}. " \
                f"32-bit result={result_32bit:#x} which is less than payload_len"
            assert error is not None, \
                f"Error must be reported for integer overflow case: payload_len={payload_len:#x}"


def test_boundary_payload_len_values():
    """
    Invariant: The exact boundary between valid and invalid payload_len values
    must be correctly enforced. Values at and below the limit are accepted;
    values above are rejected.
    """
    max_valid_payload_len = KP_RULE_BUFFER_SIZE - KP_HEADER_SIZE
    
    # Exactly at limit - should succeed
    kp_at_limit = struct.pack('<I', max_valid_payload_len) + b'\x00' * (KP_HEADER_SIZE - 4) + b'\x41' * max_valid_payload_len
    success, bytes_copied, error = safe_copy_with_validation(kp_at_limit, KP_RULE_BUFFER_SIZE)
    assert success, f"Valid boundary value rejected: payload_len={max_valid_payload_len}, error={error}"
    assert bytes_copied == KP_RULE_BUFFER_SIZE, \
        f"Expected {KP_RULE_BUFFER_SIZE} bytes copied, got {bytes_copied}"
    
    # One over limit - must fail
    kp_over_limit = struct.pack('<I', max_valid_payload_len + 1) + b'\x00' * (KP_HEADER_SIZE - 4) + b'\x41' * (max_valid_payload_len + 1)
    success, _, error = safe_copy_with_validation(kp_over_limit, KP_RULE_BUFFER_SIZE)
    assert not success, \
        f"SECURITY VIOLATION: Off-by-one overflow not caught! payload_len={max_valid_payload_len + 1}"
    
    # Zero payload - should succeed
    kp_zero = struct.pack('<I', 0) + b'\x00' * (KP_HEADER_SIZE - 4)
    success, bytes_copied, error = safe_copy_with_validation(kp_zero, KP_RULE_BUFFER_SIZE)
    assert success, f"Zero payload_len incorrectly rejected: error={error}"
    assert bytes_copied == KP_HEADER_SIZE, \
        f"Expected {KP_HEADER_SIZE} bytes for zero payload, got {bytes_copied}"