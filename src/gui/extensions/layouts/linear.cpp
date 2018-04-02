/*
 * Places elements linearly either vertically or horizontally
 * 
 * If a child element has a width or height of -1 it will take up the remaining
 * space of the layout. If multiple elements have a height or width of -1 they
 * will share the remaining space.
 * 
 * Attributes:
 * direction = "vertical|vert|v|horizontal|hori|h"
 * 
 * parseHelpers.h:
 * Padding
 * 
 * Example:
 * {
 *     direction = "vert"
 *     , {
 *         ...
 *     }
 *     , {
 *         ...
 *     }
 * }
*/

#include <stdint.h>
#include <algorithm>
#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>
#include <gui/parseHelpers.h>

using namespace GUI;

enum Direction {
    VERTICAL = 0
    , HORIZONTAL
};

struct Vec2
{
    int32_t width;
    int32_t height;
};

struct Data
{
    Direction direction;
    Vec2* childSizes;
    int32_t childCount;

    float paddingTop;
    float paddingBottom;
    float paddingLeft;
    float paddingRight;
    float paddingElement;
};

const InitFunctions* functions;

const char* dirKeys[] = {
    "vertical", "vert", "v"
    , "horizontal", "hori", "h"
};
const int dirValues[] = {
    0, 0, 0
    , 1, 1, 1
};

extern "C"
{
    bool IsLayout() { return true; }

    void Measure(lua_State* state, int32_t* width, int32_t* height)
    {
        Direction direction = (Direction)GetOptionalListIndex(state, "direction", dirKeys, dirValues, 6, 0);

        int32_t returnWidth = 0;
        int32_t returnHeight = 0;
        int32_t childCount = lua_objlen(state, -1);
        for(int32_t i = 0; i < childCount; ++i) {
            int32_t childWidth = 0;
            int32_t childHeight = 0;

            lua_rawgeti(state, -1, i + 1);
            functions->measure(state, &childWidth, &childHeight);
            lua_pop(state, 1);

            if(direction == HORIZONTAL) {
                returnHeight = std::max(returnHeight, childHeight);
                if(childWidth != -1 && returnWidth != -1)
                    returnWidth += childWidth + (returnWidth == -1);
                else if(childWidth == -1)
                    returnWidth = -1;
            } else {
                returnWidth = std::max(returnWidth, childWidth);
                if(childHeight != -1 && returnHeight != -1)
                    returnHeight += childHeight + (returnHeight == -1);
                else if(childHeight == -1)
                    returnHeight = -1;
            }
        }

        // TODO: Handle -1 case
        float top;
        float bottom;
        float left;
        float right;
        Padding::Parse(state, &top, &bottom, &left, &right); // TODO: defaults?

        *width = returnWidth + left + right;
        *height = returnHeight + top + bottom;
    }

    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    int Count(lua_State* state)
    {
        int returnValue = 0;
        int32_t childCount = lua_objlen(state, -1);
        for(int32_t i = 0; i < childCount; ++i) {
            lua_rawgeti(state, -1, i + 1);
            returnValue += functions->count(state);
            lua_pop(state, 1);
        }
        return returnValue;
    }

    int ParseLayout(lua_State* state, Layout* layout, Widget* elements, int defaults)
    {
        Data data;

        Padding::Parse(state, &data.paddingTop, &data.paddingBottom, &data.paddingLeft, &data.paddingRight, defaults);
        data.paddingElement = GetOptionalNumber(state, "padding_element", 0.0f);
        data.direction = (Direction)GetOptionalListIndex(state, "direction", dirKeys, dirValues, 6, 0);
        data.childCount = lua_objlen(state, -1);
        data.childSizes = (Vec2*)functions->memalloc(sizeof(Vec2) * data.childCount);

        int32_t offset = 0;
        for(int32_t i = 0; i < data.childCount; ++i) {
            lua_rawgeti(state, -1, i + 1);
            int retVal = functions->parse(state, &elements[offset]);
            if(retVal == 0) {
                data.childCount--;
                continue;
            }

            offset += retVal;

            float childWidth = 0.0f;
            bool childWidthSet = GetNumber(state, "width", childWidth, defaults);
            float childHeight = 0.0f;
            bool childHeightSet = GetNumber(state, "height", childHeight, defaults);

            if(!childWidthSet || !childHeightSet) {
                functions->measure(state, &data.childSizes[i].width, &data.childSizes[i].height);

                if(childWidthSet)
                    data.childSizes[i].width = childWidth;
                if(childHeightSet)
                    data.childSizes[i].height = childHeight;
            } else {
                data.childSizes[i].width = childWidth;
                data.childSizes[i].height = childHeight;
            }

            lua_pop(state, 1);
        }

        layout->data = (Data*)functions->memalloc(sizeof(Data));
        *(Data*)layout->data = data;

        return offset;
    }

    void BuildLayout(Layout* layout, Element** elements, int32_t elementCount)
    {
        Data data = *(Data*)layout->data;

        float paddingElement = data.paddingElement;
        float paddingTop = data.paddingTop;
        float paddingBottom = data.paddingBottom;
        float paddingLeft = data.paddingLeft;
        float paddingRight = data.paddingRight;

        for(int32_t i = 0; i < data.childCount; ++i) {
            float remainingSize = 0.0f;
            if(data.direction == VERTICAL) {
                remainingSize = layout->bounds.height;
                for(int32_t i = 0; i < data.childCount; ++i) {
                    if(data.childSizes[i].height != -1)
                        remainingSize -= data.childSizes[i].height;
                    if(data.childSizes[i].width == -1)
                        data.childSizes[i].width = layout->bounds.width;
                }
            } else {
                remainingSize = layout->bounds.width;
                for(int32_t i = 0; i < data.childCount; ++i) {
                    if(data.childSizes[i].width != -1)
                        remainingSize -= data.childSizes[i].width;
                    if(data.childSizes[i].height == -1)
                        data.childSizes[i].height = layout->bounds.height;
                }
            }

            if(remainingSize > 0.0f) {
                int32_t unsizedElements = 0;
                if(data.direction == VERTICAL) {
                    for(int32_t i = 0; i < data.childCount; ++i) {
                        if(data.childSizes[i].height == -1)
                            unsizedElements++;
                    }
                    if(paddingElement == -1)
                        unsizedElements++;
                    if(paddingTop == -1)
                        unsizedElements++;
                    if(paddingBottom == -1)
                        unsizedElements++;

                    float elementSize = remainingSize / unsizedElements;

                    if(paddingElement == -1)
                        paddingElement = elementSize;
                    if(paddingTop == -1)
                        paddingTop = elementSize;
                    if(paddingBottom == -1)
                        paddingBottom = elementSize;

                    for(int32_t i = 0; i < data.childCount; ++i) {
                        if(data.childSizes[i].height == -1)
                            data.childSizes[i].height = elementSize;
                    }
                } else {
                    for(int32_t i = 0; i < data.childCount; ++i) {
                        if(data.childSizes[i].width == -1)
                            unsizedElements++;
                    }
                    if(paddingElement == -1)
                        unsizedElements++;
                    if(paddingLeft == -1)
                        unsizedElements++;
                    if(paddingRight == -1)
                        unsizedElements++;

                    float elementSize = remainingSize / unsizedElements;

                    if(paddingElement == -1)
                        paddingElement = elementSize;
                    if(paddingLeft == -1)
                        paddingLeft = elementSize;
                    if(paddingRight == -1)
                        paddingRight = elementSize;
                    for(int32_t i = 0; i < data.childCount; ++i) {
                        if(data.childSizes[i].width == -1)
                            data.childSizes[i].width = elementSize;
                    }
                }
            } else {
                if(data.direction == VERTICAL) {
                    for(int32_t i = 0; i < data.childCount; ++i) {
                        if(data.childSizes[i].height == -1)
                            data.childSizes[i].height = 0;
                    }
                } else {
                    for(int32_t i = 0; i < data.childCount; ++i) {
                        if(data.childSizes[i].height == -1)
                            data.childSizes[i].height = 0;
                    }
                }
            }
        }

        float x = layout->bounds.x + paddingLeft;
        float y = layout->bounds.y + paddingTop;

        for(int32_t i = 0; i < data.childCount; ++i) {
            Rect childBounds;
            childBounds.x = x;
            childBounds.y = y;
            childBounds.width = data.childSizes[i].width;
            childBounds.height = data.childSizes[i].height;
            if(data.direction == VERTICAL) {
                y += data.childSizes[i].height + paddingElement;
            } else if(data.direction == HORIZONTAL) {
                x += data.childSizes[i].width + paddingElement;
            }

            elements[i]->bounds = childBounds;
        }
    }

    void Destroy(void* data, lua_State* state)
    {
        functions->memdealloc(((Data*)data)->childSizes);
    }
}