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

json = require(mypath .. "external/dkjson")
getopt = require(mypath .. "external/getopt")
shiro_cli = require(mypath .. "cli-common")
require(mypath .. "external/misc")

opts = getopt(arg, "te")

if opts.h then
  print("Usage:")
  print("shiro-seg2lab.lua path-to-segmentation-file\n" ..
        "  -t hop-time -e extension -s (output state-level alignment)")
  return
end

local input_seg = opts._[1]
local hop_time = tonumber(opts.t)
local output_ext = opts.e or ".txt"
local state_align = false
if opts.s then state_align = true end

if input_seg == nil then
  print("Error: shiro-seg2lab requires an input segmentation file.")
  return
end

if hop_time == nil then
  hop_time = 0.01
end

local fh = io.open(input_seg, "r")
if fh == nil then
  print("Error: cannot open " .. input_seg)
  return
end
local seg = json.decode(fh:read("*a"))
io.close(fh)
if shiro_cli.checkseg(seg) == false then return end

for i, iseg in ipairs(seg.file_list) do
  local labstr = ""
  local t0 = 0
  local t1 = 0
  local t0s = 0
  local curr_idx = 100
  local curr_st = nil
  for j, st in ipairs(iseg.states) do
    if st.ext[2] <= curr_idx then
      if curr_st ~= nil then
        labstr = labstr .. t0 .. "\t" .. t1 .. "\t" .. curr_st.ext[1] .. "\r\n"
        t0 = t1
      end
      curr_st = st
      curr_idx = st.ext[2]
    end
    t1 = st.time * hop_time
    if state_align then
      labstr = labstr .. t0s .. "\t" .. t1 .. "\t" .. st.ext[2] .. "\r\n"
    end
    t0s = t1
  end
  labstr = labstr .. t0 .. "\t" .. t1 .. "\t" .. curr_st.ext[1] .. "\r\n"
  fh = io.open(string.rmext(iseg.filename) .. output_ext, "w")
  fh:write(labstr)
  io.close(fh)
end
