#ifndef _MONACTION_H_
#define _MONACTION_H_
#include <lua.h>
#include <lauxlib.h>

typedef struct s_monaction_entry
{
  char *name;  
  short type;   
  char *script;   
  lua_State *luaState;
  short int state_initialized;
} monaction_entry;

void monaction_init_state(monaction_entry *entry);
void monaction_close_state(monaction_entry *entry);
#endif