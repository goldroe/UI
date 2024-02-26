#ifndef UI_H
#define UI_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#undef near
#undef far
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#endif // _WIN32

#include <vector>
#include <stack>

struct UI_State;
extern UI_State ui_state;

#define UI_CLAMP(V, MIN, MAX) (V < MIN ? MIN: V > MAX ? MAX : V)
#define UI_MIN(A, B) ((A) < (B) ? (A) : (B))
#define UI_MAX(A, B) ((A) > (B) ? (A) : (B))

#define STACK_CLEAR(Stack) while (!Stack.empty()) Stack.pop();

#define UI_SIZE_FIXED(PX) {UI_Size_Pixels, (PX)}
#define UI_SIZE_TEXT(V) {UI_Size_TextBounds, (V)}
#define UI_SIZE_PARENT(P) {UI_Size_ParentPercent, (P)}

void UI_BorderColor(float r, float g, float b, float a);
void UI_BorderColorPop();

struct FontGlyph {
    float ax;
    float ay;
    float bx;
    float by;
    float bt;
    float bl;
    float to;
};

struct FontAtlas {
    FontGlyph glyphs[128];
    int width;
    int height;
    int max_bmp_height;
    float ascend;
    float descend;
    int bbox_height;
    float glyph_width;
    float glyph_height;
};

union UI_Vec2 {
    struct {
        float x, y;
    };
    struct {
        float e[2];
    };

    UI_Vec2() : x(0), y(0) {}
    UI_Vec2(float x, float y) : x(x), y(y) {}
    
    float &operator[](int index) {
        return e[index];
    }
};

union UI_Vec3 {
    struct {
        float x, y, z;
    };
    struct {
        float e[3];
    };

    UI_Vec3() : x(0), y(0), z(0) {}
    UI_Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    float &operator[](int index) {
        return e[index];
    }

};

union UI_Vec4 {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a; 
    };
    struct {
        float e[4];
    };

    UI_Vec4() : x(0), y(0), z(0), w(0) {}
    UI_Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    float &operator[](int index) {
        return e[index];
    }
};

struct UI_Rect {
    union {
        struct { float x; float y; };
        struct { UI_Vec2 p; };
    };

    float width;
    float height;
};

struct UI_Vertex {
    UI_Vec2 position;
    UI_Vec4 color;
    UI_Vec2 uv;
};

struct DX11_Constant_Buffer {
    float mvp[4][4];
};

struct DX11_Backend_Data {
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;
    ID3D11RasterizerState *rasterizer_state;
    ID3D11BlendState *blend_state;
    ID3D11DepthStencilState *depth_stencil_state;

    int vertex_buffer_size;
    ID3D11Buffer *vertex_buffer;
    ID3D11Buffer *constant_buffer;

    ID3D11InputLayout *input_layout;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;

    ID3D11ShaderResourceView *font_texture_view;
    ID3D11SamplerState *font_sampler;
};

struct UI_Draw_Data {
    UI_Vec2 target_pos;
    UI_Vec2 target_size;
    UI_Vertex *vertex_list;
    int vertex_count;
    int vertex_capacity;
};

enum UI_SizeType {
    UI_Size_Invalid,
    // NOTE: Rigid sized
    UI_Size_Pixels,
    UI_Size_TextBounds,
    // NOTE: Upward dependent
    UI_Size_ParentPercent,
    // NOTE: Downward dependent
    UI_Size_ChildrenSum,
};

struct UI_Size {
    UI_SizeType type;
    float value;
};

enum UI_WidgetFlags {
    UI_WidgetFlags_DrawText           = 0x8,
    UI_WidgetFlags_DrawBorder         = 0x10,
    UI_WidgetFlags_DrawBackground     = 0x20,
    UI_WidgetFlags_DrawHotEffects     = 0x40,
    UI_WidgetFlags_DrawActiveEffects  = 0x80,
};

enum UI_Axis {
    UI_Axis_Nil = -1,
    UI_Axis_X,
    UI_Axis_Y,
};

struct UI_Widget {
    UI_WidgetFlags flags;
    char *label;
    bool active;
    
    UI_Rect rect;
    UI_Size pref_size[2];
    UI_Axis child_layout_axis;
    UI_Vec2 relative_pos;
    UI_Vec2 actual_size;

    float text_offset;
    int cursor;

    UI_Vec4 bg_color;
    UI_Vec4 border_color;
    UI_Vec4 text_color;

    // Children
    UI_Widget *first;
    UI_Widget *last;

    // Siblings
    UI_Widget *next;
    UI_Widget *prev;

    UI_Widget *parent;
};

struct UI_State {
    // Input
    int mouse_x = -1;
    int mouse_y = -1;
    bool mouse_down;
    bool mouse_pressed;
    bool dragging;
    UI_Vec2 mouse_delta;
    bool key_down;
    char key;

    // Internal
    UI_Widget *old_root;
    UI_Widget *root;

    // Rendering Data
    FontAtlas font_atlas;
    UI_Draw_Data draw_data;
    DX11_Backend_Data backend_data;

    // Layout stacks
    std::stack<UI_Widget*> parent_stack;
    std::stack<UI_Size> pref_width_stack;
    std::stack<UI_Size> pref_height_stack;
    std::stack<UI_Vec4> bg_color_stack;
    std::stack<UI_Vec4> border_color_stack;
    std::stack<UI_Vec4> text_color_stack;

    // TODO: Hash list
    std::vector<UI_Widget*> old_list;
    std::vector<UI_Widget*> widget_list;
};

void UI_DX11BackendInit(ID3D11Device *device, ID3D11DeviceContext *device_context);
void UI_Render();
void UI_NewFrame(HWND window);
void UI_EndFrame();

UI_Widget *UI_WidgetBuild(char *string, UI_WidgetFlags flags);

void UI_RowBegin(char *label);
void UI_RowEnd();

bool UI_Button(char *label);

#endif // UI_H
