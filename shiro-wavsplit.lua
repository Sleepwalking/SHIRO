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
if mypath == "" then
  mypath = "./"
end

json = require(mypath .. "external/dkjson")
getopt = require(mypath .. "external/getopt")
shiro_cli = require(mypath .. "cli-common")
require(mypath .. "external/misc")

opts = getopt(arg, "ntdfNl")

if opts.h then
  print("Usage:")
  print("shiro-wavsplit.lua path-to-wav-file\n")
  print("  -n num-utterances\n")
  print("  -t hop-time\n")
  print("  -d feature-dimension\n")
  print("  -f feature-type\n")
  print("  -N num-iter\n")
  print("  -l load-existing-model\n")
  return
end

if opts.n == nil then
  print("Error: shiro-wavsplit requires the number of utterances (-n).")
  return
end

if #opts._ < 1 then
  print("Error: shiro-wavsplit requires an input wav file.")
  return
end

local num_utt = tonumber(opts.n)
local dir, fullname, ext = fileparts(opts._[1])
local stem = fullname:sub(1, #fullname - #ext - 1)
local path_wav = opts._[1]
local path_raw = dir .. stem .. ".raw"
local path_param = dir .. stem .. ".param"
local path_index = dir .. stem .. ".index"
local path_pm = dir .. stem .. ".phonemap"
local path_md = dir .. stem .. ".modeldef"
local path_seg_init = dir .. stem .. ".init.segm"
local path_seg_aligned = dir .. stem .. ".aligned.segm"
local path_hsmm_uninit = dir .. stem .. ".uninit.hsmm"
local path_hsmm_flat = dir .. stem .. ".flat.hsmm"
local path_hsmm_trained = dir .. stem .. ".trained.hsmm"

if opts.l ~= nil then
  path_hsmm_trained = opts.l
end

local ndim = tonumber(opts.d or "13")
local thop = tonumber(opts.t or "0.1")
local ftype = opts.f or "mfcc"
local stprune = math.floor(0.2 * num_utt)
local extradur = math.floor(10 / thop)
local niter = tonumber(opts.N or "15")

-- Make the index file.
local fp = io.open(path_index, "wb")
local phonemes = {"sil"}
for i = 1, num_utt do
  phonemes[#phonemes + 1] = "utt"
  phonemes[#phonemes + 1] = "sil"
end
fp:write(stem .. "," .. table.concat(phonemes, " "))
fp:close()

-- Make the phone map.
fp = io.open(path_pm, "wb")
local phonemap = {
  phone_map = {
    sil = {
      durfloor = {0.5},
      states = {
        {out = {0}, dur = 0}
      }
    },
    utt = {
      durfloor = {0.5},
      states = {
        {out = {1}, dur = 1}
      }
    }
  }
}
fp:write(json.encode(phonemap, {indent = true, keyorder = {"sil", "utt"}}))
fp:close()

-- Make the model definition.
os.execute("lua " .. mypath .. "shiro-pm2md.lua " ..
  path_pm .. " -d " .. ndim .. " -t " .. thop .. " > " .. path_md)

-- Feature extraction.
os.execute(mypath .. "shiro-wav2raw -r 16000 -d 0.01 " .. path_wav)
os.execute(mypath .. "shiro-xxcc -f " .. ftype .. " -m " .. (ndim - 1) ..
  " -e -l 1024 -p " .. (thop * 16000) .. " -s 16 " .. path_raw .. " > " ..
  path_param)

-- Make the initial segmentation.
os.execute("lua " .. mypath .. "shiro-mkseg.lua " .. path_index ..
  " -m " .. path_pm .. " -d " .. dir .. " -e .param -n " .. (ndim * 1) ..
  " > " .. path_seg_init)

if opts.l == nil then
  -- Make the initial model.
  os.execute(mypath .. "shiro-mkhsmm -c " .. path_md ..
    " > " .. path_hsmm_uninit)
  os.execute(mypath .. "shiro-init -m " .. path_hsmm_uninit ..
    " -s " .. path_seg_init .. " -FT -v 1 > " .. path_hsmm_flat)

  -- Training
  os.execute(mypath .. "shiro-rest -m " .. path_hsmm_flat ..
    " -s " .. path_seg_init .. " -n " .. niter ..
    " -d " .. extradur .. " -p " .. stprune ..
    " -D -t 0 > " .. path_hsmm_trained)
end

-- Align
os.execute(mypath .. "shiro-align -m " .. path_hsmm_trained ..
  " -s " .. path_seg_init .. " -d " .. extradur ..
  " -p " .. stprune .. " > " .. path_seg_aligned)

-- Edit and export
fp = io.open(path_seg_aligned, "rb")
local seg = json.decode(fp:read("*a"))
fp:close()
local count = 0
for i, st in ipairs(seg.file_list[1].states) do
  if st.ext[1] == "utt" then
    st.ext[1] = tostring(count)
    count = count + 1
  end
end
fp = io.open(path_seg_aligned, "wb")
fp:write(json.encode(seg, {indent = true}))
fp:close()

os.execute("lua " .. mypath .. "shiro-seg2lab.lua " .. path_seg_aligned ..
  " -t " .. thop)

