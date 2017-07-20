--[[
  misc.lua
  Kanru Hua, 2017.

  Code snippets taken from my other lua projects.
  Public domain.
]]--

string.delimit = function(str, delim)
  local parts = {}
  local last = 1
  for i = 1, #str do
    local curr = string.sub(str, i, i)
    if curr == delim then
      local token = string.sub(str, last, i - 1)
      parts[#parts + 1] = token
      last = i + 1
    end
    if i == #str then
      local token = string.sub(str, last, i)
      parts[#parts + 1] = token
      last = i + 1
    end
  end
  return parts
end

string.rmext = function(path)
  for i = 1, #path do
    if string.sub(path, -i, -i) == "." then
      return string.sub(path, 1, -i - 1)
    elseif string.sub(path, -i, -i) == "/" or
           string.sub(path, -i, -i) == "\\" then
      return path
    end
  end
  return path
end

math.round = function(num)
  return math.floor(num + 0.5)
end

-- recursively print the contents of a table
inspect = function(obj, full_inspect, monochrome, level)
  if level == nil then
    level = 0
  end
  if monochrome == nil then
    monochrome = false
  end
  if full_inspect == nil then
    full_inspect = false
  end
  local red = "\x1b[31m"
  local green = "\x1b[32m"
  local reset = "\x1b[0m"
  if monochrome then
    red = ""
    green = ""
    reset = ""
  end
  local repspace = string.rep(" ", level)
  if type(obj) == "table" then
    print(repspace .. green .. "size: " .. #obj .. reset)
    local count = 0
    for k, v in pairs(obj) do
      count = count + 1
      if count > 100 then break end
    end
    if (not full_inspect) and count >= 100 then
      print(repspace .. "Table is too large for display.")
    else
      for k, v in pairs(obj) do
        if type(k) == "number" and (not full_inspect and k > 2) then
        else
          print(repspace .. red .. k .. reset .. " = " .. tostring(v))
          inspect(v, full_inspect, monochrome, level + 2)
        end
      end
      if (not full_inspect) and rawlen(obj) > 2 then
        print(repspace .. "...")
      end
    end
  end
end
