--[[
  SHIRO
  ===
  Copyright (c) 2017 Kanru Hua. All rights reserved.

  This file is part of SHIRO.

  SHIRO is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  SHIRO is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SHIRO.  If not, see <http://www.gnu.org/licenses/>.
]]

local mypath = arg[0]:match("(.-)[^\\/]+$")
if mypath == "" then
  mypath = "./"
end

json = require(mypath .. "external/dkjson")
getopt = require(mypath .. "external/getopt")
shiro_cli = require(mypath .. "cli-common")
require(mypath .. "external/misc")

if detect_os() == "Windows" and mypath == "./" then
  mypath = ""
end

opts = getopt(arg, "dexr")

if opts.h then
  print("Usage:")
  print("shiro-fextr.lua path-to-index-file\n" ..
        "  -d input-directory -e input-extension -x feature-extractor\n" ..
        "  -n (normalize) -r forced-sample-rate")
  return
end

local input_index = opts._[1]
local opt_directory = (opts.d or ".") .. "/"
local opt_forced_samplerate = opts.r or 0
local opt_extension = opts.e or ".wav"
local opt_normalize = opts.n or false
local opt_fextract = mypath .. "extractors/extractor-xxcc-mfcc12-da-16k"
if opts.x ~= nil then opt_fextract = opts.x end

local fextract = require(opt_fextract)

if input_index == nil then
  print("Error: shiro-fextr requires an input index file.")
  return
end

-- read and parse index file
file_list = shiro_cli.load_index_file(input_index, opt_directory, {}, {})
if file_list == false then return end

function try_execute(str)
  local ret = os.execute(str)
  --print(str)
  if not ret then
    print("Error occurred when executing command " .. str)
    os.exit(1)
  end
end

for i, entry in ipairs(file_list) do
  local infile = entry.path .. opt_extension
  print("Processing " .. infile)

  local rawfile = entry.path .. ".raw"
  local cmd_wav2raw = "shiro-wav2raw"
  if opt_normalize then
    cmd_wav2raw = cmd_wav2raw .. " -N"
  end
  if opt_forced_samplerate == 0 then
    try_execute(mypath .. cmd_wav2raw .. " \"" .. infile .. "\"")
  else
    try_execute(mypath .. cmd_wav2raw .. " -r " .. opt_forced_samplerate ..
      " \"" .. infile .. "\"")
  end

  fextract(try_execute, entry.path, rawfile, mypath)
end
