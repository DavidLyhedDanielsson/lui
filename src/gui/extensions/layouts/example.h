#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>
#include <gui/parseHelpers.h>

using namespace GUI;

const InitFunctions* functions;

struct Data
{
    // Some data that the layout needs to functions
    // ...
};

// Any optional function does not need to be declared
extern "C"
{
    // This function is called once when registering the extension.
    // InitFunctions contain necessary function pointers. These pointers can be
    // saved and used whenever.
    // fontHeight is often needed, so it is sent here.
    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    // Returns the number of elements this layout needs. Make sure to call
    // count in InitFunctions, since you need to handle nested layouts/elements
    // Optional. Default: 1
    int Count(lua_State* state);
    // Measures this layouts total size
    // A width or height of -1 means that this layout doesn't have a specific
    // size, and it is up to the parent layout to give this widget a size.
    //
    // Optional. Default: width = -1, height = -1
    void Measure(lua_State* state, int32_t* width, int32_t* height);
    // Parse this layout from the current top of the lua stack.
    //
    // This function is only called once, so it is fine to allocate memory here
    // without worrying about leaks, unlike the Build function.
    //
    // Save data inside the widget by allocating your custom Data struct and
    // storing its address inside widget->data.
    //
    // It is possible to parse any children by calling the InitFunctions->parse
    int ParseLayout(lua_State* state, Layout* layout, int defaults);
    // Place all children in their final position by setting their bounds
    // member to wherever you want them to be.
    //
    // This function may be called whenever, so take care with dynamic memory
    int BuildLayout(Layout* layout, Element** children, int32_t childCount);
    // Cast the data pointer to your custom Data struct and deallocate any
    // dynamically allocated memory inside of it. DO NOT manually deallocate
    // the pointer itself, as this is done automatically.
    //
    // Optional
    void Destroy(void* data, lua_State* state);

    // A layout cannot be interacted with by itself, but it it sometimes
    // necessary to know when a child does something so that it can be updated.
    //
    // Returning true means the event was handled and it does not need to be
    // sent to this layout's parent.
    //
    // "widget" is the widget that was interacted with
    //
    // All of these are optional, and by default returns false
    bool OnChildClicked(Layout* layout
                        , lua_State* state
                        , Widget* widget
                        , int32_t mouseX
                        , int32_t mouseY);
    bool OnChildReleased(Layout* layout
                            , lua_State* state
                            , Widget* widget
                            , int32_t mouseX
                            , int32_t mouseY);
    bool OnChildUpdate(Layout* layout
                        , lua_State* state
                        , Widget* widget
                        , int32_t mouseX
                        , int32_t mouseY);
}