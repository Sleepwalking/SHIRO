--[[
  SHIRO
  ===
  Copyright (c) 2017-2018 Kanru Hua. All rights reserved.

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

package.path = package.path .. ";" ..
  mypath .. "?.lua;" .. mypath .. "external/?.lua"

json = require("dkjson")
getopt = require("getopt")
shiro_cli = require("cli-common")

opts = getopt(arg, "mdteE")

if opts.h then
  print("Usage:")
  print("shiro-lab2seg.lua path-to-index-file\n" ..
        "  -m path-to-phonemap -d label-directory -t hop-time\n" ..
        "  -e label-extension -E feature-extension")
  return
end

local input_index = opts._[1]
local input_phonemap = opts.m
local hop_time = tonumber(opts.t)
local ext = opts.e or ".txt"
local ext_feature = opts.E or ".f"

if input_index == nil then
  print("Error: shiro-lab2seg requires an input index file.")
  return
end

if input_phonemap == nil then
  print("Error: shiro-lab2seg requires an input phonemap.")
  return
end

if hop_time == nil then
  hop_time = 0.01
end

-- read and parse index file
local file_list = shiro_cli.load_index_file(input_index,
  (opts.d or ".") .. "/", {}, {})
if file_list == false then return end

local fh = io.open(input_phonemap, "r")
if fh == nil then
  print("Error: cannot open " .. input_phonemap)
  return
end
local pm = json.decode(fh:read("*a"))
io.close(fh)
if shiro_cli.checkpm(pm) == false then return end

local seg = {file_list = {}}
for i = 1, #file_list do
  local label_path = file_list[i].path .. ext
  local feature_path = file_list[i].path .. ext_feature
  fh = io.open(label_path)
  if fh == nil then
    print("Error: cannot open " .. label_path)
    return
  end
  local lab = shiro_cli.parse_lab(fh:read("*a"))

  local states = {}
  for j = 1, #lab do
    local p = lab[j].label
    local pst = pm.phone_map[p]
    if pst == nil then
      print("Error: phoneme " .. p .. " is not defined in the phone map.")
      return
    end
    local t1 = math.floor(lab[j].t1 / hop_time)
    local nst = #pst.states
    for k = 1, nst do
      states[#states + 1] = {
        time = t1,
        dur = pst.states[k].dur,  -- duration state
        out = pst.states[k].out,  -- output state for each stream
        jmp = {},                 -- jumps (besides forward transition)
        ext = {p, k - 1}          -- extra information
      }
    end
  end
  seg.file_list[i] = {filename = feature_path, states = states}

  io.close(fh)
end

print(json.encode(seg, {indent = true,
  keyorder = {"filename", "states", "time", "dur", "out", "jmp", "ext"}}))
