#!/usr/bin/env python3
# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
"""Parse device output to extract LLVM profile data.

The profile data obtained from the device is raw. Thus, it must be indexed
before it can be used to generate coverage reports.

Typical usage:
    stty -F /dev/ttyUSB0 icanon
    cat /dev/ttyUSB0 | ./device_profile_data.py foo.profraw
    llvm-profdata merge -sparse foo.profraw -o foo.profdata
    llvm-cov show OBJECT_FILE -instr-profile=foo.profdata
"""
import argparse
import zlib
import re
import sys


def extract_profile_data(device_output):
    """Parse device output to extract LLVM profile data.

    This function returns the LLVM profile data as a byte array after
    verifying its length and checksum.

    Args:
        device_output: Device output.
    Returns:
        LLVM profile data.
    Raises:
        ValueError: If LLVM profile data cannot be detected in the device
            output or its length or checksum is incorrect.
    """
    device_output = re.sub(r'^.*] (.*)$',
                           r'\1',
                           device_output,
                           flags=re.MULTILINE)
    device_output = device_output.translate(
        device_output.maketrans('', '', '\r\n'))
    match = re.search(
        r"""
            LLVM\ profile\ data\ \(length:\ (?P<len>\d+)\ bytes,\ CRC32:\ (?P<crc>0x[0-9a-f]*)\):
            (?P<data> 0x [0-9a-f]+)
            \x04
        """, device_output, re.VERBOSE)
    if not match:
        raise ValueError(
            'Could not detect LLVM profile data in device output.')
    exp_length = int(match.group('len'))
    exp_checksum = int(match.group('crc'), 0)
    byte_array = int(match.group('data'), 0).to_bytes(len(match.group('data')) // 2 - 1,
                                                      byteorder='little',
                                                      signed=False)
    # Check length
    act_length = len(byte_array)
    if act_length != exp_length:
        raise ValueError(('Length check failed! ',
                          f'Expected: {exp_length}, actual: {act_length}.'))
    # Check checksum
    act_checksum = zlib.crc32(byte_array)
    if act_checksum != exp_checksum:
        raise ValueError(
            ('Checksum check failed! ',
             f'Expected: {exp_checksum}, actual: {act_checksum}.'))
    return byte_array


def main():
    """Parses command line arguments and extracts the profile data from device
    output."""
    argparser = argparse.ArgumentParser(
        description='Extract LLVM profile data from device output.')
    argparser.add_argument(dest='output_file',
                           type=argparse.FileType('wb'),
                           default=sys.stdout,
                           help='output file for writing LLVM profile data')
    argparser.add_argument('--input_file',
                           type=argparse.FileType('rb'),
                           default=sys.stdin.buffer,
                           help='device output')
    args = argparser.parse_args()

    args.output_file.write(
        extract_profile_data(args.input_file.read().decode('ascii', 'ignore')))


if __name__ == '__main__':
    main()
