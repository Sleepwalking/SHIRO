return function (try_excute, path, rawfile)
  local mfccfile = path .. ".mfcc"
  local paramfile = path .. ".param"
  try_execute("frame -l 512 -p 80 \"" .. rawfile .. "\" | " ..
    "mfcc -l 512 -m 12 -s 16 > \"" .. mfccfile .. "\"")
  try_execute("delta -l 12 -d -0.5 0 0.5 -d 0.25 0 -0.5 0 0.25 \"" ..
    mfccfile .. "\" > \"" .. paramfile .. "\"")
end
