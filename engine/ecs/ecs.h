#ifndef OPENLUA_ECS_H
#define OPENLUA_ECS_H

#include <lua.h>

/* Entity/Component/System world API.  Registered as the global `ecs` table. */
void ecs_api_register(lua_State *L);

#endif
