require("lfs")
require("posix")

function deltree(dir) 
  for filename, attr in walk_tree(dir) do
    if attr.mode == "file" then
      os.remove(filename)
    else
      deltree(filename)
    end
  end
  os.remove(dir)
  return true
end

function walk_tree(dir)
  assert(dir and dir ~= "", "directory parameter is missing or empty")
  if string.sub(dir, -1) == "/" then
    dir=string.sub(dir, 1, -2)
  end

  local function yieldtree(dir)
    if posix.stat(dir) ~= nil then
      for entry in lfs.dir(dir) do
	if entry ~= "." and entry ~= ".." then
	  entry=dir.."/"..entry
	  local attr=lfs.attributes(entry)
	  coroutine.yield(entry,attr)
	  if attr.mode == "directory" then
	    yieldtree(entry)
	  end
	end
      end
    end
  end

  return coroutine.wrap(function() yieldtree(dir) end)
end

function initialize (state)
  return true
end

function conf_action_preload(dir) 
  for filename, attr in walk_tree(dir) do      
    if attr.mode == "directory" and string.sub(filename,-8) == "CVS/Base" and posix.stat(filename) ~= nil then
      deltree(filename)
    end
  end  
  return true
end

function event_action(event, base_path, file_path, timestamp) 
  local full_path = base_path.."/"..file_path
  f = io.open("/tmp/base","w+")
  io.output(f)
  io.write(full_path.."\n")
  io.close(f)
  if posix.stat(full_path) ~= nil then
    local attr = lfs.attributes(full_path)
    if attr.mode == "directory" and string.sub(full_path,-8) == "CVS/Base" then
      deltree(full_path)
    end
  end
  return true
end