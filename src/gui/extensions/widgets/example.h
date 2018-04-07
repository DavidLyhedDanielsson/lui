#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>
#include <gui/parseHelpers.h>

using namespace GUI;

const InitFunctions* functions;

struct Data
{
    // Some data that the widget needs to functions
    // char* text
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

    // Returns the number of elements this widget needs.
    //
    // Note: Make sure to call InitFunctions->count if this element has any
    // children and you are not 100% sure of how many elements they need
    //
    // Optional. Default: 1
    int Count(lua_State* state);
    // Measures this elements total size
    // A width or height of -1 means that this widget doesn't have a specific
    // size, and it is up to the parent layout to give this widget a size.
    //
    // Optional. Default: width = -1, height = -1
    void Measure(lua_State* state, int32_t* width, int32_t* height);
    // Parse this widget from the current top of the lua stack.
    //
    // This function is only called once, so it is fine to allocate memory here
    // without worrying about leaks, unlike the Build function.
    //
    // Save data inside the widget by allocating your custom Data struct and
    // storing its address inside widget->data.
    //
    // It is possible to parse any children by calling the InitFunctions->parse
    int ParseWidget(lua_State* state, Widget* widget, int defaults);
    // Place this widget at widget->bounds. Do this by filling the vertices and
    // indicies member of widget. Don't forget to set vertexCount.
    // This function may be called whenever, so take care with dynamic memory
    int BuildWidget(Widget* widget);
    // Any element parsed from ParseWidget will be marked
    // as a child of the widget. Here you need to give they children their
    // bounds. In this way, a widget can also sort of act as a layout.
    // 
    // Optional. Only needed if this widget has children
    void BuildChildren(Widget* widget, Element** children, int32_t childCount);
    // Cast the data pointer to your custom Data struct and deallocate any
    // dynamically allocated memory inside of it. DO NOT manually deallocate
    // the pointer itself, as this is done automatically
    //
    // Optional
    void Destroy(void* data, lua_State* state);

    // It is through these events that the main application communicates with
    // its widgets. The name of most of them gives a clear explanation of what
    // they do.
    //
    // All of these are optional.
    // Default for any event function returning a value is false
    void OnEnter(Widget* widget, lua_State* state);
    void OnExit(Widget* widget, lua_State* state);
    // When these functions return true, the event is considered handled,
    // and will not be sent to this widget's parent.
    bool OnClick(Widget* widget, lua_State* state, int32_t mouseX, int32_t mouseY);
    bool OnReleaseInside(Widget* widget, lua_State* state, int32_t mouseX, int32_t mouseY);
    bool OnReleaseOutside(Widget* widget, lua_State* state, int32_t mouseX, int32_t mouseY);
    bool OnScroll(Element*, lua_State* state, int32_t scrollX, int32_t scrollY);
    // Called only when widget->update is true. You may set widget->update to
    // true or false whenever you wish
    void OnUpdate(Widget* widget, lua_State* state, int32_t mouseX, int32_t mouseY);
    // Called when a popup created by this widget is closed, either
    // automatically or manually. mouseDown is true if the mouse is currently
    // down
    void OnPopupClosed(Widget* widget, lua_State* state, bool mouseDown);
    // Sometimes it is necessary to know when a child element does something.
    // Returning "true" means this event was handled, and should not be
    // forwarded to this widget's parent.
    bool OnChildClicked(Element*, lua_State* state, Widget* widget, int32_t mouseX, int32_t mouseY);
    bool OnChildReleased(Element*, lua_State* state, Widget* widget, int32_t mouseX, int32_t mouseY);
    bool OnChildUpdate(Element*, lua_State* state, Widget* widget, int32_t mouseX, int32_t mouseY);
    
    // Queries data from the widget.
    // Returns true if query was successful
    bool QueryNumber(Widget* widget, const char* key, float* value);
    // Returns the number of characters written to value
    int QueryString(Widget* widget, const char* key, char* value, int32_t maxLength);
    // Setters
    bool SetNumber(Widget* widget, const char* key, float value);
    bool SetString(Widget* widget, const char* key, const char* value);

}