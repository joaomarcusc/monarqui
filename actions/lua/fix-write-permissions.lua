require("lfs")
require("posix")

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
  os.execute("chmod -R u+w "..dir)
  return true
end

function event_action(event, base_path, file_path, timestamp) 
  local full_path = base_path.."/"..file_path  
  local file_stat = posix.stat(full_path)
  if file_stat ~= nil and string.sub(file_stat.mode,2,2) ~= "w" then
    posix.chmod(full_path,"u+w")
  end
  return true
end
