require("lfs")
require("posix")

function initialize (state)
end

function conf_action_preload(dir) 
end

function event_action(event, base_path, file_path, timestamp) 
  local f = io.open("/tmp/delete-log","a+")
  io.output(f)
  io.write(base_path.."/"..file_path.."\n")
  io.close(f)
end
