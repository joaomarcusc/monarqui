#ifndef _MONACTION_H_
#define _MONACTION_H_
#include <lua.h>
#include <lauxlib.h>

typedef struct s_monaction_entry
{
  char name[64];  
  short type;   
  char *script;   
  lua_State *luaState;
} monaction_entry;

void monaction_init_state(monaction_entry *entry);
#endif