#ifndef _MONARQUI_COMMON_H
#define _MONARQUI_COMMON_H

#define STR_ATTRIB "attrib"
#define STR_MODIFY "modify"
#define STR_DELETE "delete"
#define STR_CREATE "create"
#define STR_ACCESS "access"
#define STR_MOVED_FROM "moved_from"
#define STR_MOVED_TO "moved_to"

#define MON_CREATE 1
#define MON_MODIFY 2
#define MON_DELETE 4
#define MON_ATTRIB 8
#define MON_ACCESS 16
#define MON_MOVED_FROM 32
#define MON_MOVED_TO 64

#define STR_ACT_SHELL "shell"
#define STR_ACT_LUA "lua"
#define STR_ACT_LOG "log"

#define MON_ACT_SHELL 1
#define MON_ACT_LUA 2
#define MON_ACT_LOG 4

#define STR_EVENT_SEPARATOR ","
#define DEFAULT_CONFIG_PATH "config.xml"
#include <glib.h>
#include <zmq.h>
#include <lua.h>

char *string_join(GList *list);
char *int_events_to_str(int events);
int str_events_to_int(char *str);
void bail(lua_State *L, char *msg);
void show_lua_error(lua_State *L, char *msg);
#endif
