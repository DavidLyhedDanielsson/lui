#include "luahelpers.h"

#include <algorithm>
#include <iostream>

#include "guielement.h"
#include "lib.h"

bool GetNumber(lua_State* state
                , const char* key
                , float& value
                , int defaults/*= -1*/)
{
    if(!FieldExists(state, key)) {
        if(defaults == -1 || !FieldExists(state, defaults, key)) {
            return false;
        }
    }

    if(!lua_isnumber(state, -1)) {
        lua_pop(state, 1);
        return false;
    }

    value = lua_tonumber(state, -1);
    lua_pop(state, 1);
    return true;
}

bool GetNumber(lua_State* state
                , const char* key
                , int32_t& value
                , int defaults/*= -1*/)
{
    float localValue = (float)value;
    bool returnValue = GetNumber(state, key, localValue, defaults);
    value = (int32_t)localValue;
    return returnValue;
}

bool GetBoolean(lua_State* state
                , const char* key
                , bool& value
                , int defaults/*= -1*/)
{
    if(!FieldExists(state, key)) {
        if(defaults == -1 || !FieldExists(state, defaults, key)) {
            return false;
        }
    }

    if(!lua_isboolean(state, -1)) {
        lua_pop(state, 1);
        return false;
    }

    value = lua_toboolean(state, -1);
    lua_pop(state, 1);
    return true;
}

bool GetString(lua_State* state
                , const char* key
                , char* buffer
                , size_t maxLength
                , size_t* actualLength
                , int defaults/*= -1*/)
{
    if(!FieldExists(state, key)) {
        if(defaults == -1 || !FieldExists(state, defaults, key)) {
            return false;
        }
    }

    if(!lua_isstring(state, -1)) {
        lua_pop(state, 1);
        return false;
    }

    const char* luastring = lua_tostring(state, -1);
    size_t luastringLength = std::strlen(luastring) + 1;

    if(actualLength != nullptr)
        *actualLength = luastringLength;

    std::memcpy(buffer, luastring, std::min(luastringLength, maxLength));
    lua_pop(state, 1);

    return true;
}

bool GetColor(lua_State* state
                , const char* key
                , GUI::Color& color
                , int defaults/*= -1*/)
{
    if(!FieldExists(state, key)
        && (defaults == -1 || !FieldExists(state, defaults, key)))
    {
        return false;
    }

    color = ParseColor(state);
    lua_pop(state, 1);
    return true;
}

bool GetListIndex(lua_State* state
                   , const char* key
                   , const char** possibleValues
                   , int possibleValuesLength
                   , int& index
                   , int defaults/*= -1*/)
{
    const char* value;
    if(!FieldExists(state, key)
        && (defaults == -1 || !FieldExists(state, defaults, key)))
    {
        return false;
    }
    value = lua_tostring(state, -1);

    for(int i = 0; i < possibleValuesLength; ++i) {
        if(streq(value, possibleValues[i])) {
            index = i;
            break;
        }
    }

    lua_pop(state, 1);
    return true;
}

bool GetListIndex(lua_State* state
                   , const char* key
                   , const char** possibleValues
                   , const int* returnValues
                   , int possibleValuesLength
                   , int& index
                   , int defaults/*= -1*/)
{
    const char* value;
    if(!FieldExists(state, key)
        && (defaults == -1 || !FieldExists(state, defaults, key)))
    {
        return false;
    }
    value = lua_tostring(state, -1);

    for(int i = 0; i < possibleValuesLength; ++i) {
        if(streq(value, possibleValues[i])) {
            index = returnValues[i];
            break;
        }
    }

    lua_pop(state, 1);
    return true;
}

float GetOptionalNumber(lua_State* state
                            , const char* key
                            , float otherwise
                            , int defaults/*= -1*/)
{
    float value = otherwise;
    GetNumber(state, key, value, defaults);
    return value;
}

int32_t GetOptionalNumber(lua_State* state
                            , const char* key
                            , int32_t otherwise
                            , int defaults/*= -1*/)
{
    int32_t value = otherwise;
    GetNumber(state, key, value, defaults);
    return value;
}

bool GetOptionalBoolean(lua_State* state
                            , const char* key
                            , bool otherwise
                            , int defaults/*= -1*/)
{
    bool value = otherwise;
    GetBoolean(state, key, value, defaults);
    return value;
}

void GetOptionalString(lua_State* state
                        , const char* key
                        , char* buffer
                        , size_t maxLength
                        , size_t* actualLength
                        , const char* otherwise
                        , int defaults/*= -1*/)
{
    if(!GetString(state, key, buffer, maxLength, actualLength, defaults))
    {
        strcpy(buffer, otherwise);
        if(actualLength)
            *actualLength = strlen(buffer);
    }
}

GUI::Color GetOptionalColor(lua_State* state
                       , const char* key
                       , const GUI::Color& otherwise
                       , int defaults/*= -1*/)
{
    GUI::Color color = otherwise;
    GetColor(state, key, color, defaults);
    return color;
}

int GetOptionalListIndex(lua_State* state
                            , const char* key
                            , const char** possibleValues
                            , int possibleValuesLength
                            , int otherwise
                            , int defaults/*= -1*/)
{
    int index = otherwise;
    GetListIndex(state, key, possibleValues, possibleValuesLength, index, defaults);
    return index;
}

int GetOptionalListIndex(lua_State* state
                            , const char* key
                            , const char** possibleValues
                            , const int* returnValues
                            , int possibleValuesLength
                            , int otherwise
                            , int defaults/*= -1*/)
{
    int index = otherwise;
    GetListIndex(state, key, possibleValues, returnValues, possibleValuesLength, index, defaults);
    return index;
}

bool FieldExists(lua_State* state
                    , int luaRef
                    , const char* key)
{
    lua_rawgeti(state, LUA_REGISTRYINDEX, luaRef);
    lua_getfield(state, -1, key);
    if(lua_isnil(state, -1)) {
        lua_pop(state, 2);
        return false;
    } else {
        // Swap table and value, and pop table
        lua_insert(state, -2);
        lua_pop(state, 1);
        return true;
    }
}

bool FieldExists(lua_State* state
                    , const char* key)
{
    lua_getfield(state, -1, key);
    if(lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return false;
    } else {
        return true;
    }
}

void PrintPair(lua_State* state, int indent)
{
    for(int i = 0; i < indent; ++i)
        std::cerr << "    ";

    if(lua_isnumber(state, -2)) {
        float index = lua_tonumber(state, -2);
        std::cerr << "[" << (int)index << "] = ";
    } else if(lua_isstring(state, -2)) {
        const char* key = lua_tostring(state, -2);
        std::cerr << key << " = ";
    }

    if(lua_isboolean(state, -1)) {
        std::cerr << lua_toboolean(state, -1) << std::endl;
    } else if(lua_isnumber(state, -1)) {
        std::cerr << lua_tonumber(state, -1) << std::endl;
    } else if(lua_isstring(state, -1)) {
        std::cerr << "\"" << lua_tostring(state, -1) << "\"" << std::endl;
    } else {
        std::cerr << "{ table with " << lua_objlen(state, -1) << " members }" << std::endl;
    }
}

void DumpElement(lua_State* state)
{
    std::cerr << "Dumped element:" << std::endl;
    lua_pushnil(state);
    while(lua_next(state, -2)) {
        if(lua_isnumber(state, -2)) {
            float index = lua_tonumber(state, -2);
            std::cerr << "[" << (int)index << "] = ";
        } else if(lua_isstring(state, -2)) {
            const char* key = lua_tostring(state, -2);
            std::cerr << key << " = ";
        }
        if(!lua_istable(state, -1)) {
            PrintPair(state, 0);
        } else if(lua_istable(state, -1)) {
            std::cerr << " {" << std::endl;
            lua_pushnil(state);
            while(lua_next(state, -2)) {
                PrintPair(state, 1);

                lua_pop(state, 1);
            }
            std::cerr << "}" << std::endl;
        }
        lua_pop(state, 1);
    }
}