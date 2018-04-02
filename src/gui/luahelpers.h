#ifndef LUAHELPERS_H__
#define LUAHELPERS_H__

#include <cstring>
#include <lua5.1/lua.hpp>
#include <cstdint>

namespace GUI {
    struct Color;
}

bool GetNumber(lua_State* state
                , const char* key
                , float& value
                , int defaults = -1);
bool GetNumber(lua_State* state
                , const char* key
                , int32_t& value
                , int defaults = -1);
bool GetBoolean(lua_State* state
                 , const char* key
                 , bool& value
                 , int defaults = -1);
bool GetString(lua_State* state
                , const char* key
                , char* buffer
                , size_t maxLength
                , size_t* actualLength
                , int defaults = -1);
bool GetColor(lua_State* state
               , const char* key
               , GUI::Color& color
               , int defaults = -1);
bool GetListIndex(lua_State* state
                   , const char* key
                   , const char** possibleValues
                   , int possibleValuesLength
                   , int& index
                   , int defaults = -1);
bool GetListIndex(lua_State* state
                   , const char* key
                   , const char** possibleValues
                   , const int* returnValues
                   , int possibleValuesLength
                   , int& index
                   , int defaults = -1);

float GetOptionalNumber(lua_State* state
                            , const char* key
                            , float otherwise
                            , int defaults = -1);
int32_t GetOptionalNumber(lua_State* state
                            , const char* key
                            , int32_t otherwise
                            , int defaults = -1);
bool GetOptionalBoolean(lua_State* state
                            , const char* key
                            , bool otherwise
                            , int defaults = -1);
void GetOptionalString(lua_State* state
                        , const char* key
                        , char* buffer
                        , size_t maxLength
                        , size_t* actualLength
                        , const char* otherwise
                        , int defaults = -1);
GUI::Color GetOptionalColor(lua_State* state
                            , const char* key
                            , const GUI::Color& otherwise
                            , int defaults = -1);
int GetOptionalListIndex(lua_State* state
                            , const char* key
                            , const char** possibleValues
                            , int possibleValuesLength
                            , int otherwise
                            , int defaults = -1);
int GetOptionalListIndex(lua_State* state
                            , const char* key
                            , const char** possibleValues
                            , const int* returnValues
                            , int possibleValuesLength
                            , int otherwise
                            , int defaults = -1);

bool FieldExists(lua_State* state
                    , int luaRef
                    , const char* key);

bool FieldExists(lua_State* state
                  , const char* key);

void DumpElement(lua_State* state);

#endif