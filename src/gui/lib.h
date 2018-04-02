#ifndef LIB_H__
#define LIB_H__

#include <lua5.1/lua.hpp>

#include "guielement.h"

#include <cstring>

#define QUAD_ALLOC(widget, count) {\
    widget->vertexCount = (count) * 4;\
    widget->indexCount = (count) * 6;\
    widget->vertices = (GUI::Vertex*)functions->memalloc(sizeof(GUI::Vertex) * widget->vertexCount);\
    widget->indicies = (uint32_t*)functions->memalloc(sizeof(uint32_t) * widget->indexCount);\
}

inline bool streq(const char* lhs, const char* rhs)
{
    return strcmp(lhs, rhs) == 0;
}

template<typename... T>
inline bool streq(const char* lhs, const char* rhs, T... args)
{
    return strcmp(lhs, rhs) == 0 || streq(lhs, args...);
}

enum Origin {
    TOP = 1
    , BOTTOM = 2
    , LEFT = 4
    , RIGHT = 8
    , CENTER_VERTICAL = TOP | BOTTOM
    , CENTER_HORIZONTAL = LEFT | RIGHT
};

GUI::Color ParseColor(lua_State* state);
void CreateQuad(GUI::Vertex* vertexBuffer
                    , uint32_t* elementBuffer
                    , int32_t elementOffset
                    , float x
                    , float y
                    , float width
                    , float height
                    , float uMin
                    , float uMax
                    , float vMin
                    , float vMax
                    , uint8_t r
                    , uint8_t g
                    , uint8_t b
                    , uint8_t a);

void CreateColoredQuad(GUI::Widget* widget
                        , float x
                        , float y
                        , float width
                        , float height
                        , uint8_t r
                        , uint8_t g
                        , uint8_t b
                        , uint8_t a);

void AdjustVertices(GUI::Vertex* vertices
                        , uint32_t vertexCount
                        , Origin origin
                        , float posX
                        , float posY);

void AdjustText(GUI::Vertex* vertices
                        , uint32_t textLength
                        , Origin origin
                        , float posX
                        , float posY);

Origin GetOrigin(lua_State* state);

void GetPointInRect(GUI::Rect rect, Origin origin, float* outX, float* outY);

#endif