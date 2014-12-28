
#include "tolua_fix.h"
#include "base/CCRef.h"
#include "LuaBasicConversions.h"
#include <stdlib.h>

using namespace cocos2d;

static int s_function_ref_id = 0;

TOLUA_API void toluafix_open(lua_State* L)
{
    lua_pushstring(L, TOLUA_REFID_PTR_MAPPING);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushstring(L, TOLUA_REFID_TYPE_MAPPING);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushstring(L, TOLUA_REFID_FUNCTION_MAPPING);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

TOLUA_API int toluafix_pushusertype_ccobject(lua_State* L,
                                             int refid,
                                             int* p_refid,
                                             void* ptr,
                                             const char* type)
{
    if (ptr == NULL || p_refid == NULL)
    {
        lua_pushnil(L);
        return -1;
    }
    
    Ref* vPtr = static_cast<Ref*>(ptr);
    const char* vType = getLuaTypeName(vPtr, type);

    if (*p_refid == 0)
    {
        *p_refid = refid;

        lua_pushstring(L, TOLUA_REFID_PTR_MAPPING);
        lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: refid_ptr */
        lua_pushinteger(L, refid);                                  /* stack: refid_ptr refid */
        lua_pushlightuserdata(L, vPtr);                              /* stack: refid_ptr refid ptr */

        lua_rawset(L, -3);                  /* refid_ptr[refid] = ptr, stack: refid_ptr */
        lua_pop(L, 1);                                              /* stack: - */

        lua_pushstring(L, TOLUA_REFID_TYPE_MAPPING);
        lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: refid_type */
        lua_pushinteger(L, refid);                                  /* stack: refid_type refid */
        lua_pushstring(L, vType);                                    /* stack: refid_type refid type */
        lua_rawset(L, -3);                /* refid_type[refid] = type, stack: refid_type */
        lua_pop(L, 1);                                              /* stack: - */

        //printf("[LUA] push CCObject OK - refid: %d, ptr: %x, type: %s\n", *p_refid, (int)ptr, type);
    }

    tolua_pushusertype_and_addtoroot(L, vPtr, vType);
    
    return 0;
}

TOLUA_API int toluafix_remove_ccobject_by_refid(lua_State* L, int refid)
{
	void* ptr = NULL;
    const char* type = NULL;
    void** ud = NULL;
    if (refid == 0) return -1;

    // get ptr from tolua_refid_ptr_mapping
    lua_pushstring(L, TOLUA_REFID_PTR_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                               /* stack: refid_ptr */
    lua_pushinteger(L, refid);                                      /* stack: refid_ptr refid */
    lua_rawget(L, -2);                                              /* stack: refid_ptr ptr */
    ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);                                                  /* stack: refid_ptr */
    if (ptr == NULL)
    {
        lua_pop(L, 1);
        // Lua stack has closed, C++ object not in Lua.
        // printf("[LUA ERROR] remove CCObject with NULL ptr, refid: %d\n", refid);
        return -2;
    }

    // remove ptr from tolua_refid_ptr_mapping
    lua_pushinteger(L, refid);                                      /* stack: refid_ptr refid */
    lua_pushnil(L);                                                 /* stack: refid_ptr refid nil */
    lua_rawset(L, -3);                     /* delete refid_ptr[refid], stack: refid_ptr */
    lua_pop(L, 1);                                                  /* stack: - */


    // get type from tolua_refid_type_mapping
    lua_pushstring(L, TOLUA_REFID_TYPE_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                               /* stack: refid_type */
    lua_pushinteger(L, refid);                                      /* stack: refid_type refid */
    lua_rawget(L, -2);                                              /* stack: refid_type type */
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 2);
        printf("[LUA ERROR] remove CCObject with NULL type, refid: %d, ptr: %p\n", refid, ptr);
        return -1;
    }

    type = lua_tostring(L, -1);
    lua_pop(L, 1);                                                  /* stack: refid_type */

    // remove type from tolua_refid_type_mapping
    lua_pushinteger(L, refid);                                      /* stack: refid_type refid */
    lua_pushnil(L);                                                 /* stack: refid_type refid nil */
    lua_rawset(L, -3);                    /* delete refid_type[refid], stack: refid_type */
    lua_pop(L, 1);                                                  /* stack: - */

    // get ubox
    luaL_getmetatable(L, type);                                     /* stack: mt */
    lua_pushstring(L, "tolua_ubox");                                /* stack: mt key */
    lua_rawget(L, -2);                                              /* stack: mt ubox */
    if (lua_isnil(L, -1))
    {
        // use global ubox
        lua_pop(L, 1);                                              /* stack: mt */
        lua_pushstring(L, "tolua_ubox");                            /* stack: mt key */
        lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: mt ubox */
    };
    
    
    // cleanup root
    tolua_remove_value_from_root(L, ptr);

    lua_pushlightuserdata(L, ptr);                                  /* stack: mt ubox ptr */
//    lua_rawget(L,-2);                                               /* stack: mt ubox ud */
    toluafix_get_userdata(L, -2);
    if (lua_isnil(L, -1))
    {
        // Lua object has released (GC), C++ object not in ubox.
        //printf("[LUA ERROR] remove CCObject with NULL ubox, refid: %d, ptr: %x, type: %s\n", refid, (int)ptr, type);
        lua_pop(L, 3);
        return -3;
    }

    // cleanup peertable
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    lua_setfenv(L, -2);

    ud = (void**)lua_touserdata(L, -1);
    lua_pop(L, 1);                                                  /* stack: mt ubox */
    if (ud == NULL)
    {
        printf("[LUA ERROR] remove CCObject with NULL userdata, refid: %d, ptr: %p, type: %s\n", refid, ptr, type);
        lua_pop(L, 2);
        return -1;
    }

    // clean userdata
    *ud = NULL;

    lua_pushlightuserdata(L, ptr);                                  /* stack: mt ubox ptr */
    lua_pushnil(L);                                                 /* stack: mt ubox ptr nil */
//    lua_rawset(L, -3);                             /* ubox[ptr] = nil, stack: mt ubox */
    toluafix_set_userdata(L, -3);

    lua_pop(L, 2);
    //printf("[LUA] remove CCObject, refid: %d, ptr: %x, type: %s\n", refid, (int)ptr, type);
    return 0;
}

TOLUA_API int toluafix_ref_function(lua_State* L, int lo, int def)
{
    // function at lo
    if (!lua_isfunction(L, lo)) return 0;

    s_function_ref_id++;

    lua_pushstring(L, TOLUA_REFID_FUNCTION_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: fun ... refid_fun */
    lua_pushinteger(L, s_function_ref_id);                      /* stack: fun ... refid_fun refid */
    lua_pushvalue(L, lo);                                       /* stack: fun ... refid_fun refid fun */

    lua_rawset(L, -3);                  /* refid_fun[refid] = fun, stack: fun ... refid_ptr */
    lua_pop(L, 1);                                              /* stack: fun ... */

    return s_function_ref_id;

    // lua_pushvalue(L, lo);                                           /* stack: ... func */
    // return luaL_ref(L, LUA_REGISTRYINDEX);
}

TOLUA_API void toluafix_get_function_by_refid(lua_State* L, int refid)
{
    lua_pushstring(L, TOLUA_REFID_FUNCTION_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: ... refid_fun */
    lua_pushinteger(L, refid);                                  /* stack: ... refid_fun refid */
    lua_rawget(L, -2);                                          /* stack: ... refid_fun fun */
    lua_remove(L, -2);                                          /* stack: ... fun */
}

TOLUA_API void toluafix_remove_function_by_refid(lua_State* L, int refid)
{
    lua_pushstring(L, TOLUA_REFID_FUNCTION_MAPPING);
    lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: ... refid_fun */
    lua_pushinteger(L, refid);                                  /* stack: ... refid_fun refid */
    lua_pushnil(L);                                             /* stack: ... refid_fun refid nil */
    lua_rawset(L, -3);                  /* refid_fun[refid] = fun, stack: ... refid_ptr */
    lua_pop(L, 1);                                              /* stack: ... */

    // luaL_unref(L, LUA_REGISTRYINDEX, refid);
}

// check lua value is funciton
TOLUA_API int toluafix_isfunction(lua_State* L, int lo, const char* type, int def, tolua_Error* err)
{
    if (lua_gettop(L) >= abs(lo) && lua_isfunction(L, lo))
    {
        return 1;
    }
    err->index = lo;
    err->array = 0;
    err->type = "[not function]";
    return 0;
}

TOLUA_API int toluafix_totable(lua_State* L, int lo, int def)
{
    return lo;
}

TOLUA_API int toluafix_istable(lua_State* L, int lo, const char* type, int def, tolua_Error* err)
{
    return tolua_istable(L, lo, def, err);
}

TOLUA_API void toluafix_stack_dump(lua_State* L, const char* label)
{
    int i;
    int top = lua_gettop(L);
    printf("Total [%d] in lua stack: %s\n", top, label != 0 ? label : "");
    for (i = -1; i >= -top; i--)
    {
        int t = lua_type(L, i);
        switch (t)
        {
            case LUA_TSTRING:
                printf("  [%02d] string %s\n", i, lua_tostring(L, i));
                break;
            case LUA_TBOOLEAN:
                printf("  [%02d] boolean %s\n", i, lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNUMBER:
                printf("  [%02d] number %g\n", i, lua_tonumber(L, i));
                break;
            default:
                printf("  [%02d] %s\n", i, lua_typename(L, t));
        }
    }
    printf("\n");
}

//typedef std::unordered_map<void*, void*> ToluafixCTable;
//static std::vector<ToluafixCTable *> ctable_list;

class ToluafixCTable
{
public:
    ToluafixCTable()
    {
        char s[100];
        sprintf(s, "tolua_fix_ctable_%p", this);
        name = s;
    }
    
    int find(void *value)
    {
        int i;
        for (i=0;i<v.size();i++)
        {
            if (v.at(i) == value)
            {
                return i;
            }
        }
        return -1;
    }
    
    
    void add(void *value)
    {
        if (find(value)>=0)
        {
            return;
        }
        v.push_back(value);
        auto num = v.size();
        if (num>max)
        {
            max = num;
        }
    }
    
    void remove(void *value)
    {
        for (auto it = v.begin(); it != v.end(); ++it)
        {
            auto lt = *it;
            if (lt == value)
            {
                v.erase(it);
                break;
            }
        }
    }
    
    void rebuild(lua_State* L)
    {
        auto num = v.size();
        if ( num>0 && num+100>max )
        {
            return;
        }
        
        lua_pushstring(L, name.c_str());                            /* stack: ... string */
        lua_pushvalue(L, -1);                                       /* stack: ... string string */
        lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: ... string ubox */
        lua_newtable(L);                                            /* stack: ... string ubox newtbl */
        for (auto it = v.begin(); it != v.end(); ++it)
        {
            auto lt = *it;
            lua_pushlightuserdata(L, lt);   /* stack: ... string ubox newtbl key */
            lua_pushvalue(L, -1);           /* stack: ... string ubox newtbl key key */
            lua_rawget(L, -4);              /* stack: ... string ubox newtbl key value */
            lua_rawset(L, -3);              /* stack: ... string ubox newtbl */
        }
        lua_insert(L, -2);              /* stack: ... string newtbl ubox */
        lua_pop(L, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
    
    std::string & getName()
    {
        return name;
    }
    
private:
    std::vector<void*> v;
    size_t max = 0;
    std::string name;
};
static std::vector<ToluafixCTable *> ctable_list;

static ToluafixCTable* findCtable(void *ctable)
{
    ToluafixCTable* table = (ToluafixCTable*)ctable;
    for (auto it = ctable_list.begin(); it != ctable_list.end(); ++it)
    {
        auto lt = *it;
        if (lt == table)
        {
            return lt;
        }
    }
    return nullptr;
}

TOLUA_API void toluafix_new_ctable(lua_State* L)
{
    ToluafixCTable* table = new ToluafixCTable;
    if (table)
    {
        ctable_list.push_back(table);
        lua_pushlightuserdata(L, table);
    }
    else
    {
        lua_pushnil(L);
    }
}

TOLUA_API void toluafix_get_userdata(lua_State *L, int idx)
{
    void *ctable = lua_touserdata(L, idx);
    ToluafixCTable* table = findCtable(ctable);
    if (!table)
    {
        lua_pop(L, 1);
        lua_pushnil(L);
    }
    
    void *value = lua_touserdata(L, -1);
    auto found = table->find(value);
    if ( found>=0 )
    {
        lua_pushstring(L, table->getName().c_str());                        /* stack: ... value string */
        lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: ... value ubox */
        lua_insert(L, -2);
        lua_rawget(L, -2);
        lua_insert(L, -2);
        lua_pop(L, 1);
    }
    else
    {
        lua_pop(L, 1);
        lua_pushnil(L);
    }
}

TOLUA_API void toluafix_set_userdata(lua_State *L, int idx)
{
    void *value = lua_touserdata(L, -2);
    if (!value)
    {
        lua_pop(L, 2);
        return;
    }
    
    void *ctable = lua_touserdata(L, idx);
    ToluafixCTable* table = findCtable(ctable);
    if (!table)
    {
        lua_pop(L, 2);
        return;
    }
    
    bool checkFlag;
    if (lua_isnil(L, -1))
    {
        table->remove(value);
        checkFlag = true;
    }
    else
    {
        table->add(value);
        checkFlag = false;
    }
    
    auto name = table->getName();
    lua_pushstring(L, name.c_str());                        /* stack: ... value ud string */
    lua_rawget(L, LUA_REGISTRYINDEX);                           /* stack: ... value ud ubox */
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushstring(L, name.c_str());
        lua_pushvalue(L, -2);
        lua_rawset(L,LUA_REGISTRYINDEX);
    }
    lua_insert(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    if (checkFlag)
    {
        table->rebuild(L);
    }
}
