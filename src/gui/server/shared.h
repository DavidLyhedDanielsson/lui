namespace NetGUI
{
    namespace FUNCTIONS
    {
        enum FUNCTIONS
        {
            InitGUI = 0
            , BuildGUI
            , ReloadGUI
            , DestroyGUI
            , ResolutionChanged
            , UpdateGUI
            , RegisterSharedLibrary
            , MouseDown
            , MouseUp
            , Scroll
            , GetFontTextureData
            , GetFontTextureWidth
            , GetFontTextureHeight
            , GetDrawListCount
            , GetDrawLists
        };
    }

    struct Header {
        int32_t vertexCount;
        int32_t indexCount;
        int32_t textureIndex;
        GUI::Rect clipRect;
    };
}