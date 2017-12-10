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
require(mypath .. "external/misc")

function detect_os()
  local uname = io.popen("uname -s"):read("*l")
  if uname == nil then return "Windows" end
  return uname
end

function checkpm(pm)
  if pm.phone_map == nil or type(pm.phone_map) ~= "table" then
    print("Error: attribute \"phone_map\" not found.")
    return false
  end
  for p, pv in pairs(pm.phone_map) do
    if pv.states == nil or type(pv.states) ~= "table" then
      print("Error: attribute \"states\" not found.")
      return false
    end
    for i, pst in ipairs(pv.states) do
      if pst.out == nil or type(pst.out) ~= "table" then
        print("Error: attribute \"out\" not found.")
        return false
      end
      if pst.dur == nil or type(pst.dur) ~= "number" then
        print("Error: attribute \"dur\" not found.")
        return false
      end
    end
  end
  return true
end

function checkseg(seg)
  if seg.file_list == nil or type(seg.file_list) ~= "table" then
    print("Error: attribute \"file_list\" not found.")
    return false
  end
  for i, iseg in ipairs(seg.file_list) do
    if iseg.states == nil or type(iseg.states) ~= "table" then
      print("Error: attribute \"states\" not found.")
      return false
    end
    if iseg.filename == nil or type(iseg.filename) ~= "string" then
      print("Error: attribute \"filename\" not found.")
      return false
    end
    for j, st in ipairs(iseg.states) do
      if st.ext == nil or type(st.ext) ~= "table" then
        print("Error: attribute \"ext\" not found.")
        return false
      end
      if st.time == nil or type(st.time) ~= "number" then
        print("Error: attribute \"time\" not found.")
        return false
      end
      if #st.ext < 2 then
        print("Error: incorrect size of \"ext\".")
        return false
      end
    end
  end
  return true
end

function load_index_file(path, dir, lpad, rpad)
  local fh = io.open(path, "r")
  if fh == nil then
    print("Error: cannot open " .. input_index)
    return false
  end
  local file_list = {}
  local n = 1
  while true do
    local line = fh:read()
    if line == nil or line == "" then
      break
    end
    parts = string.delimit(line, ",")
    if #parts ~= 2 then
      print("Error: format error at line " .. n .. ".")
      return false
    end
    file_list[n] = {}
    file_list[n].path = dir .. parts[1]
    local middle = string.delimit(parts[2], " ")
    local phonemes = {}
    for i = 1, #lpad do
      phonemes[i] = lpad[i]
    end
    for i = 1, #middle do
      phonemes[#phonemes + 1]= middle[i]
    end
    for i = 1, #rpad do
      phonemes[#phonemes + 1]= rpad[i]
    end
    file_list[n].phonemes = phonemes
    n = n + 1
  end
  io.close(fh)
  return file_list
end

local function delimitcsv(str, delim)
  local parts = {{""}}
  local last = 1
  for i = 1, #str do
    local curr = string.sub(str, i, i)
    local top = parts[#parts]
    if curr == delim or curr == "\n" then
      local token = string.sub(str, last, i - 1)
      if string.byte(token, #token) == 13 then
        token = string.sub(token, 1, #token - 1)
      end
      top[#top] = token
      if curr == "\n" then
        parts[#parts + 1] = {""}
      else
        top[#top + 1] = ""
      end
      last = i + 1
    elseif i == #str then
      local token = string.sub(str, last, i)
      top[#top] = token
    end
  end
  return parts
end

local function parse_lab(str)
  local lab = delimitcsv(str, "\t")
  if #lab > 0 and #lab[1] == 1 then
    lab = delimitcsv(str, " ")
  end
  local ret = {}
  for i = 1, #lab do
    if #lab[i] < 2 then return ret end
    ret[#ret + 1] = {
      t0 = tonumber(lab[i][1]),
      t1 = tonumber(lab[i][2]),
      label = lab[i][3]
    }
  end
  return ret
end

return {checkpm = checkpm,
  checkseg = checkseg,
  load_index_file = load_index_file,
  parse_lab = parse_lab}
