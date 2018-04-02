#include "lib.h"

#include "luahelpers.h"

#include <cstdlib>
#include <unordered_map>
#include <algorithm>

const static float WHITE_UV = 1.0f / 512.0f;

using namespace GUI;

Color ParseColor(lua_State* state) {
    Color color = { 0, 0, 0, 255 };

    if(lua_istable(state, -1)) {
        int count = lua_objlen(state, -1);
        if(count != 0) {
            if(count > 4) 
                count = 4;

            for(int i = 0; i < count; ++i) {
                lua_rawgeti(state, -1, i + 1);
                ((uint8_t*)&color)[i] = lua_tonumber(state, -1);
                lua_pop(state, 1);
            }
        } else {
            lua_pushnil(state);
            while(lua_next(state, -2) != 0) {
                const char* component = lua_tostring(state, -2);

                if(streq(component, "red", "r")) {
                    color.r = lua_tonumber(state, -1);
                } else if(streq(component, "green", "g")) {
                    color.g = lua_tonumber(state, -1);
                } else if(streq(component, "blue", "b")) {
                    color.b = lua_tonumber(state, -1);
                } else if(streq(component, "alpha", "a")) {
                    color.a = lua_tonumber(state, -1);
                }

                lua_pop(state, 1);
            }
        }
    } else if(lua_isstring(state, -1)) {
        const char* value = lua_tostring(state, -1);

        if(value[0] == '#') {
            // #AABBCCDD
            ++value;
        } else if(strcmp(value, "0x") == 0) {
            //0xAABBCCDD
            value += 2;
        }

        int length = std::strlen(value);
        if(length > 8) {
            //std::cerr << "Color element has more than 4 components, only the first 4 will be used" << std::endl;
            length = 8;
        } else if(length % 2 != 0) {
            //std::cerr << "Color element has an uneven number of components, last component will be ignored" << std::endl;
            --length;
        }

        for(int i = 0; i < length / 2; ++i) {
            char buf[3] = { value[i * 2], value[i * 2 + 1], '\0' };
            ((uint8_t*)&color)[i] = std::strtol(buf, nullptr, 16); // TODO: Is this safe?
        }
    }

    return color;
}

void CreateQuad(Vertex* vertexBuffer
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
                    , uint8_t a)
{
    vertexBuffer[0] = { x, y, uMin, vMax, r, g, b, a };
    vertexBuffer[1] = { x, y + height, uMin, vMin, r, g, b, a };
    vertexBuffer[2] = { x + width, y + height, uMax, vMin, r, g, b, a };
    vertexBuffer[3] = { x + width, y, uMax, vMax, r, g, b, a };

    elementBuffer[0] = elementOffset;
    elementBuffer[1] = elementOffset + 1;
    elementBuffer[2] = elementOffset + 3;
    elementBuffer[3] = elementOffset + 1;
    elementBuffer[4] = elementOffset + 2;
    elementBuffer[5] = elementOffset + 3;
}

void CreateColoredQuad(GUI::Widget* widget
                        , float x
                        , float y
                        , float width
                        , float height
                        , uint8_t r
                        , uint8_t g
                        , uint8_t b
                        , uint8_t a)
{
    CreateQuad(widget->vertices + widget->offsetData.vertex
                , widget->indicies + widget->offsetData.index
                , widget->offsetData.vertexBegin
                , x
                , y
                , width
                , height
                , WHITE_UV
                , WHITE_UV
                , WHITE_UV
                , WHITE_UV
                , r
                , g
                , b
                , a);
    
    widget->offsetData.vertex += 4;
    widget->offsetData.index += 6;
    widget->offsetData.vertexBegin += 4;
}

bool QuadContains(GUI::Vertex vertices[6], float x, float y)
{
    /*float minX = vertices[0].x;
    float maxX = vertices[1].x;
    float minY = vertices[0].y;
    float maxY = vertices[2].y;

    return x >= minX && x <= maxX && y >= minY && y <= maxY;*/
    return false;
}

void AdjustVertices(GUI::Vertex* vertices
                        , uint32_t vertexCount
                        , Origin origin
                        , float posX
                        , float posY)
{
    float offsetX = vertices[0].x;
    float offsetY = vertices[0].y;

    switch(origin & CENTER_HORIZONTAL) {
        case LEFT:
            for(size_t i = 1; i < vertexCount; ++i) {
                offsetX = std::min(offsetX, vertices[i].x);
            }
            break;
        case RIGHT:
            for(size_t i = 1; i < vertexCount; ++i) {
                offsetX = std::max(offsetX, vertices[i].x);
            }
            break;
        case CENTER_HORIZONTAL:{
            float min = offsetX;
            float max = offsetX;
            for(size_t i = 1; i < vertexCount; ++i) {
                min = std::min(min, vertices[i].x);
                max = std::max(max, vertices[i].x);
            }
            offsetX = (max - min) * 0.5f;
            break;}
    }

    switch(origin & CENTER_VERTICAL) {
        case TOP:
            for(size_t i = 1; i < vertexCount; ++i) {
                offsetY = std::min(offsetY, vertices[i].y);
            }
            break;
        case BOTTOM:
            for(size_t i = 1; i < vertexCount; ++i) {
                offsetY = std::max(offsetY, vertices[i].y);
            }
            break;
        case CENTER_VERTICAL: {
            float min = offsetY;
            float max = offsetY;
            for(size_t i = 1; i < vertexCount; ++i) {
                min = std::min(min, vertices[i].y);
                max = std::max(max, vertices[i].y);
            }
            offsetY = min + (max - min) * 0.5f;
            break;}
    }

    for(size_t i = 0; i < vertexCount; ++i) {
        vertices[i].x -= offsetX;
        vertices[i].x += posX;
        vertices[i].y -= offsetY;
        vertices[i].y += posY;
    }
}

void AdjustText(GUI::Vertex* vertices
                        , uint32_t textLength
                        , Origin origin
                        , float posX
                        , float posY)
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    switch(origin & CENTER_HORIZONTAL) {
        case LEFT:
            // No need to do anything
            break;
        case RIGHT:
            offsetX = vertices[(textLength - 1) * 4 + 3].x;
            break;
        case CENTER_HORIZONTAL:
            offsetX = vertices[(textLength - 1) * 4 + 3].x * 0.5f;
            break;
    }

    switch(origin & CENTER_VERTICAL) {
        case TOP:
            break;
        case BOTTOM:
            offsetY = vertices[1].y;
            break;
        case CENTER_VERTICAL:
            offsetY = vertices[1].y * 0.5f;
            break;
    }

    for(size_t i = 0; i < textLength * 4; ++i) {
        vertices[i].x += posX - offsetX;
        vertices[i].y += posY -offsetY;
    }
}

Origin GetOrigin(lua_State* state)
{
    Origin origin = (Origin)(Origin::TOP | Origin::LEFT);
    if(FieldExists(state, "align_vertical")) {
        const char* value = lua_tostring(state, -1);
        if(streq(value, "top")) {
            origin = (Origin)((origin & CENTER_HORIZONTAL) | Origin::TOP);
        } else if(streq(value, "bottom")) {
            origin = (Origin)((origin & CENTER_HORIZONTAL) | Origin::BOTTOM);
        } else if(streq(value, "center")) {
            origin = (Origin)((origin & CENTER_HORIZONTAL) | Origin::CENTER_VERTICAL);
        }
        lua_pop(state, 1);
    }
    if(FieldExists(state, "align_horizontal")) {
        const char* value = lua_tostring(state, -1);
        if(streq(value, "left")) {
            origin = (Origin)((origin & CENTER_VERTICAL) | Origin::LEFT);
        } else if(streq(value, "right")) {
            origin = (Origin)((origin & CENTER_VERTICAL) | Origin::RIGHT);
        } else if(streq(value, "center")) {
            origin = (Origin)((origin & CENTER_VERTICAL) | Origin::CENTER_HORIZONTAL);
        }
        lua_pop(state, 1);
    }
    return origin;
}

void GetPointInRect(GUI::Rect rect, Origin origin, float* outX, float* outY)
{
    switch(origin & CENTER_HORIZONTAL) {
        case LEFT:
            *outX = rect.x;
            break;
        case RIGHT:
            *outX = rect.x + rect.width;
            break;
        case CENTER_HORIZONTAL:
            *outX = rect.x + rect.width * 0.5f;
            break;
    }

    switch(origin & CENTER_VERTICAL) {
        case TOP:
            *outY = rect.y;
            break;
        case BOTTOM:
            *outY = rect.y + rect.height;
            break;
        case CENTER_VERTICAL:
            *outY = rect.y + rect.height * 0.5f;
            break;
    }
}