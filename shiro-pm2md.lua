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

opts = getopt(arg, "d")

if opts.h then
  print("Usage:")
  print("shiro-pm2md.lua path-to-phonemap-file -d dimension")
  return
end

local input_pm = opts._[1]
local ndim = tonumber(opts.d or "12")

if input_pm == nil then
  print("Error: shiro-pm2md requires an input phonemap file.")
  return
end

local fh = io.open(input_pm, "r")
if fh == nil then
  print("Error: cannot open " .. input_pm)
  return
end
local pm = json.decode(fh:read("*a"))
io.close(fh)
if pm == nil then
  print("Error: " .. input_pm .. " is not a valid JSON file.")
  return
end
if shiro_cli.checkpm(pm) == false then return end

local maxdurstate = 0
local maxoutstate = {}
local nstream = 0
for p, pv in pairs(pm.phone_map) do
  for i, pst in ipairs(pv.states) do
    maxdurstate = math.max(maxdurstate, pst.dur + 1)
    if nstream == 0 then
      nstream = #pst.out
      maxoutstate = pst.out
    else
      if nstream ~= #pst.out then
        print("Error: inconsistent number of streams.")
        return
      end
      for j = 1, nstream do
        maxoutstate[j] = math.max(maxoutstate[j], pst.out[j] + 1)
      end
    end
  end
end

output_md = {
  ndurstate = maxdurstate,
  streamdef = {}
}
for i = 1, nstream do
  output_md.streamdef[i] =
    {nstate = maxoutstate[i], ndim = ndim, nmix = 1, weight = 1}
end

print(json.encode(output_md, {indent = true,
  keyorder = {"ndurstate", "streamdef", "nstate", "ndim", "nmix", "weight"}}))
