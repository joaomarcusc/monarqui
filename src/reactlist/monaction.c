#include "monaction.h"
#include <lua.h>
#include <lauxlib.h>

void monaction_init_state(monaction_entry *entry)
{
  int status;
  if(!entry->state_initialized)
  {
    entry->luaState = luaL_newstate();
    luaL_openlibs(entry->luaState);     
    status = luaL_loadfile(entry->luaState, entry->script);     
    status = status & lua_pcall(entry->luaState, 0, 0, 0);
    if (status) 
    {
      /* Get the error message in case something went wrong*/
      fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(entry->luaState, -1));
      exit(1);
    }
    entry->state_initialized = 1;
  }
}

void monaction_close_state(monaction_entry *entry)
{
  if(entry->state_initialized)
  {
    lua_close(entry->luaState);
    entry->luaState = NULL;
    entry->state_initialized = 0;
  }
}