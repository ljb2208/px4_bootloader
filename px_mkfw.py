#!/usr/bin/env python
#
# Copyright 2012 Michael Smith. All rights reserved.
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
# PX4 firmware image generator
#
# The PX4 firmware file is a JSON-encoded Python object, containing
# metadata fields and a zlib-compressed base64-encoded firmware image.
#

import sys
import argparse
import json
import base64
import zlib
import time
import subprocess

#
# Construct a basic firmware description
#
def mkdesc():
	proto = {}
	proto['magic']		= "PX4FWv1"
	proto['board_id']	= 0
	proto['board_revision']	= 0
	proto['version']	= ""
	proto['summary']	= ""
	proto['description']	= ""
	proto['git_identity']	= ""
	proto['build_time']	= 0
	proto['image']		= base64.b64encode(bytearray())
	proto['image_size']	= 0
	return proto

# Parse commandline
parser = argparse.ArgumentParser(description="Firmware generator for the PX autopilot system.")
parser.add_argument("--prototype",	action="store", help="read a prototype description from a file")
parser.add_argument("--board_id",	action="store", help="set the board ID required")
parser.add_argument("--board_revision",	action="store", help="set the board revision required")
parser.add_argument("--version",	action="store", help="set a version string")
parser.add_argument("--summary",	action="store", help="set a brief description")
parser.add_argument("--description",	action="store", help="set a longer description")
parser.add_argument("--git_identity",	action="store", help="the working directory to check for git identity")
parser.add_argument("--image",		action="store", help="the firmware image")
args = parser.parse_args()

# Fetch the firmware descriptor prototype if specified
if args.prototype != None:
	f = open(args.prototype,"r")
	desc = json.load(f)
	f.close()
else:
	desc = mkdesc()

desc['build_time'] 		= int(time.time())

if args.board_id != None:
	desc['board_id']	= int(args.board_id)
if args.board_revision != None:
	desc['board_revision']	= int(args.board_revision)
if args.version != None:
	desc['version']		= str(args.version)
if args.summary != None:
	desc['summary']		= str(args.summary)
if args.description != None:
	desc['description']	= str(args.description)
if args.git_identity != None:
	cmd = " ".join(["git", "--git-dir", args.git_identity + "/.git", "describe", "--always", "--dirty"])
	p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).stdout
	desc['git_identity']	= p.read().strip()
	p.close()
if args.image != None:
	f = open(args.image, "rb")
	bytes = f.read()
	desc['image_size'] = len(bytes)
	desc['image'] = base64.b64encode(zlib.compress(bytes,9))

print json.dumps(desc, indent=4)
