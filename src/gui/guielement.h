#ifndef GUIELEMENT_H__
#define GUIELEMENT_H__

#include <stdint.h>
#include <vector>
#include <lua5.1/lua.hpp>

namespace GUI
{
    enum CLOSE_ON {
        NONE
        , HOVER
        , CLICK
    };

    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    struct Rect {
        float x;
        float y;
        float width;
        float height;

        bool Contains(float x, float y) const
        {
            return (x >= this->x && x <= this->x + this->width
                    && y >= this->y && y <= this->y + this->height);
        }

        bool Nonzero() const
        {
            return x != 0.0f || y != 0.0f || width != 0.0f || height != 0.0f;
        }
    };

    struct Vertex {
        float x;
        float y;
        float u;
        float v;
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    enum GUIObjectType {
        WIDGET
        , LAYOUT
    };

    struct OffsetData
    {
        int vertex;
        int index;
        int vertexBegin;
    };

    struct Element
    {
        const GUIObjectType type;

        Rect bounds;
        int32_t extension;
        void* data;

        std::vector<Element*> children; // TODO: Replace std::vector
        Element* parent;

        Element(GUIObjectType type)
            : type(type)
        {}
    };

    // Only POD inheritance + constructor for constants
    struct Widget : public Element
    {
        bool modified;
        bool draw;
        bool update;
        int32_t layer;
        Vertex* vertices;
        int32_t vertexCount;
        uint32_t* indicies;
        int32_t indexCount;
        int32_t mask;
        uint64_t clipRect;
        OffsetData offsetData;

        Widget()
            : Element(GUIObjectType::WIDGET)
        {}
    };

    struct Layout : public Element
    {
        Layout()
            : Element(GUIObjectType::LAYOUT)
        {}
    };

    enum class Event
    {
        Click, Release
    };

    struct InitFunctions;

    // TODO: Order
    typedef void* (*MemallocCallback)(uint64_t);
    typedef void (*MemdeallocCallback)(void*);
    typedef void (*MessageCallback)(const char*);
    typedef int (*CountCallback)(lua_State*);
    typedef void (*MeasureCallback)(lua_State*, int32_t* width, int32_t* height);
    typedef int (*ParseCallback)(lua_State*, GUI::Widget*);
    typedef void (*BuildCallback)(GUI::Element*);
    typedef void (*OpenPopupCallback)(GUI::Element**, int32_t, CLOSE_ON);
    typedef void (*ClosePopupCallback)(lua_State* state);
    typedef void (*SetDrawCallback)(GUI::Element*, bool, int32_t);
    typedef bool (*QueryNumberCallback)(GUI::Element*, const char*, float*);
    typedef int (*QueryStringCallback)(GUI::Element*, const char*, char*, int32_t);
    typedef bool (*SetNumberCallback)(GUI::Element*, const char*, float);
    typedef bool (*SetStringCallback)(GUI::Element*, const char*, const char*);
    typedef void (*SetClipRectCallback)(GUI::Element*, GUI::Rect clipRect);
    typedef void (*CreateTextCallback)(GUI::Widget*, const char*, uint8_t, uint8_t, uint8_t, uint8_t);
    typedef void (*MeasureTextCallback)(const char*, int32_t*, int32_t*);
    typedef void (*StealMouseCallback)(Element* element);
    typedef void (*FreeMouseCallback)(Element* element);
    typedef Element* (*GetNamedElementCallback)(const char*);

    struct InitFunctions
    {
        MessageCallback message;
        CountCallback count;
        MeasureCallback measure;
        ParseCallback parse;
        BuildCallback build;
        OpenPopupCallback openPopup;
        ClosePopupCallback closePopup;
        MemallocCallback memalloc;
        MemdeallocCallback memdealloc;
        SetDrawCallback setDraw;
        QueryNumberCallback queryNumber;
        QueryStringCallback queryString;
        SetNumberCallback setNumber;
        SetStringCallback setString;
        SetClipRectCallback setClipRect;
        CreateTextCallback createText;
        MeasureTextCallback measureText;
        StealMouseCallback stealMouse;
        FreeMouseCallback freeMouse;
        GetNamedElementCallback getNamedElement;
    };
}

#endif