return function (try_excute, path, rawfile, mypath)
  local paramfile = path .. ".param"
  try_execute(mypath .. "shiro-xxcc -l 512 -p 80 -m 12 -s 16 -da -f plpcc \"" ..
    rawfile .. "\" > \"" .. paramfile .. "\"")
end
