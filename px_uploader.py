#!/usr/bin/env python
#
# Copyright 2011 Michael Smith. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#    1. Redistributions of source code must retain the above
#       copyright notice, this list of conditions and the following
#       disclaimer.
#
#    2. Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials
#       provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ''AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#
# Serial firmware uploader for the PX4FMU bootloader
#

import sys, argparse, binascii, serial, os, struct

class firmware(object):
	'''Loads a firmware file'''

	#
	# The .opfw file format that we read is described as:
	#
	# We have 100 bytes for the whole description.
	#
	# Only the first 40 are visible on the FirmwareIAP uavobject, the remaining
	# 60 are ok to use for packaging and will be saved in the flash.
	#
	# Structure is:
	#   4 bytes: header: "OpFw".
	#   4 bytes: GIT commit tag (short version of SHA1).
	#   4 bytes: Unix timestamp of compile time.
	#   2 bytes: target platform. Should follow same rule as BOARD_TYPE and BOARD_REVISION in board define files.
	#  26 bytes: commit tag if it is there, otherwise branch name. '-dirty' may be added if needed. Zero-padded.
	#  ---- 40 bytes limit ---
	#  20 bytes: SHA1 sum of the firmware.
	#  40 bytes: free for now.
	#

	def __init__(self, path):

		# read the file
		f = open(path, "rb")

		# get the header
		f.seek(0, os.SEEK_END)
		if (f.tell() <= 100):
			raise RuntimeError("firmware file is too small")
		f.seek(-100, os.SEEK_END)
		imagelen = f.tell()
		binheader = f.read(100)

		# parse the header
		header = struct.unpack_from('<4sIIBB26s', binheader)
		if (header[0] != 'OpFw'):
			raise RuntimeError("bad file signature %" % header[0])
		self.board_type = header[3]
		self.board_rev = header[4]

		# get the body data
		f.seek(0)
		self.image = f.read(imagelen)


class uploader(object):
	'''Uploads a firmware file to the PX FMU bootloader'''

	NOP		= chr(0x00)
	OK		= chr(0x10)
	FAILED		= chr(0x11)
	INSYNC		= chr(0x12)
	EOC		= chr(0x20)
	GET_SYNC	= chr(0x21)
	GET_DEVICE	= chr(0x22)
	CHIP_ERASE	= chr(0x23)
	CHIP_VERIFY	= chr(0x24)
	PROG_MULTI	= chr(0x27)
	READ_MULTI	= chr(0x28)
	REBOOT		= chr(0x30)
	
	PROG_MULTI_MAX	= 60		# protocol max is 255, must be multiple of 4
	READ_MULTI_MAX	= 60		# protocol max is 255, something overflows with >= 64

	def __init__(self, portname):
		self.port = serial.Serial(portname, 115200, timeout=10)

	def __send(self, c):
#		print("send " + binascii.hexlify(c))
		self.port.write(str(c))

	def __recv(self, count = 1):
		c = self.port.read(count)
		if (len(c) < 1):
			raise RuntimeError("timeout waiting for data")
#		print("recv " + binascii.hexlify(c))
		return c

	def __getSync(self):
		c = self.__recv()
		if (c != self.INSYNC):
			raise RuntimeError("unexpected 0x%x instead of INSYNC" % ord(c))
		c = self.__recv()
		if (c != self.OK):
			raise RuntimeError("unexpected 0x%x instead of OK" % ord(c))

	# attempt to get back into sync with the bootloader
	def __sync(self):
		# send a stream of ignored bytes longer than the longest possible conversation
		# that we might still have in progress
		self.__send(uploader.NOP * (uploader.PROG_MULTI_MAX + 2))
		self.port.flushInput()
		self.__send(uploader.GET_SYNC 
				+ uploader.EOC)
		self.__getSync()

	# send the CHIP_ERASE command and wait for the bootloader to become ready
	def __erase(self):
		self.__send(uploader.CHIP_ERASE 
				+ uploader.EOC)
		self.__getSync()

	# send a PROG_MULTI command to write a collection of bytes
	def __program_multi(self, data):
		self.__send(uploader.PROG_MULTI
				+ chr(len(data)))
		self.__send(data)
		self.__send(uploader.EOC)
		self.__getSync()
		
	# verify multiple bytes in flash
	def __verify_multi(self, data):
		self.__send(uploader.READ_MULTI
				+ chr(len(data))
				+ uploader.EOC)
		programmed = self.__recv(len(data))
		if (programmed != data):
			print("got    " + binascii.hexlify(programmed))
			print("expect " + binascii.hexlify(data))
			return False
		self.__getSync()
		return True
		
	# send the reboot command
	def __reboot(self):
		self.__send(uploader.REBOOT)

	# split a sequence into a list of size-constrained pieces
	def __split_len(self, seq, length):
    		return [seq[i:i+length] for i in range(0, len(seq), length)]

	# upload code
	def __program(self, fw):
		code = fw.image
		groups = self.__split_len(code, uploader.PROG_MULTI_MAX)
		for bytes in groups:
			self.__program_multi(bytes)

	# verify code
	def __verify(self, fw):
		self.__send(uploader.CHIP_VERIFY
				+ uploader.EOC)
		self.__getSync()
		code = fw.image
		groups = self.__split_len(code, uploader.READ_MULTI_MAX)
		for bytes in groups:
			if (not self.__verify_multi(bytes)):
				raise RuntimeError("Verification failed")

	# verify whether the bootloader is present and responding
	def check(self):
		self.__sync()

	# get the board's info structure
	def identify(self):
		self.__send(uploader.GET_DEVICE
				+ uploader.EOC)
		binboardinfo = self.__recv(32)
		self.__getSync()
		boardinfo = struct.unpack_from('<IBBBBIIIIII', binboardinfo)
		if (boardinfo[0] != 0xbdbdbdbd):
			raise RuntimeError("board ID structure magic mismatch")

		self.board_type = boardinfo[1]
		self.board_rev = boardinfo[2]
		self.bl_rev = boardinfo[3]
		self.hw_type = boardinfo[4]

	# upload the firmware
	def upload(self, fw, erase_params = False):
		print("erase...")
		self.__erase()
		print("program...")
		self.__program(fw)
		print("verify...")
		self.__verify(fw)
		print("done.")
		self.__reboot()
	

# Parse commandline arguments
parser = argparse.ArgumentParser(description="Firmware uploader for the PX autopilot system.")
parser.add_argument('--port', action="store", required=True, help="Serial port to which the FMU is attached.")
parser.add_argument('firmware', action="store", help="Firmware file to be uploaded")
args = parser.parse_args()

# Load the firmware file
fw = firmware(args.firmware)
print("loaded firmware for %x,%x" % (fw.board_type, fw.board_rev))

# Connect to the device and identify it
up = uploader(args.port)
up.check()
up.identify()
print("connected to board %x,%x " % (up.board_type, up.board_rev))

if (up.board_type != fw.board_type):
	raise RuntimeError("Firmware not suitable for this board")

up.upload(fw)
