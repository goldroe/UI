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

struct UI_Vec2 {
    float x, y;
    UI_Vec2() : x(0), y(0) {}
    UI_Vec2(float x, float y) : x(x), y(y) {}
};

struct UI_Vec3 {
    float x, y, z;
    UI_Vec3() : x(0), y(0), z(0) {}
    UI_Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

union UI_Vec4 {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
    UI_Vec4() : x(0), y(0), z(0), w(0) {}
    UI_Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct UI_Rect {
    float x, y;
    float width, height;
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
    UI_Size_Pixels,
    UI_Size_TextSize,
};

struct UI_Size {
    UI_SizeType type;
    union {
        float value;
    };
};

enum UI_WidgetFlags {
    UI_WidgetEdit,   
};

struct UI_Widget {
    UI_WidgetFlags flags;
    char *label;
    bool active;
    UI_Rect rect;
    float text_offset;
    int cursor;

    UI_Widget *next;
};

enum UI_LayoutFlags {
    UI_LayoutRow,
};

struct UI_Layout {
    UI_LayoutFlags flags;
};

struct UI_State {
    // Input
    int mouse_x;
    int mouse_y;
    bool mouse_down;
    bool mouse_pressed;
    bool dragging;
    UI_Vec2 mouse_delta;
    bool key_down;
    char key;

    // Layout
    UI_Layout layout;
    UI_Vec2 next_position;

    // Internal
    UI_Widget *parent;
    UI_Widget *current_parent;

    // Rendering Data
    FontAtlas font_atlas;
    UI_Draw_Data draw_data;
    DX11_Backend_Data backend_data;
};

template <class T>
T UI_Clamp(T val, T min, T max) {
    return val < min ? min : val > max ? max : val;
}

void UI_DX11BackendInit(ID3D11Device *device, ID3D11DeviceContext *device_context);
void UI_Render();
void UI_NewFrame(HWND window);
void UI_EndFrame();

void UI_BeginRow();
void UI_EndRow();

void UI_Label(char *label);
void UI_Labelf(char *fmt, ...);
bool UI_Button(char *label);
void UI_Slider(char *label, float *f, float min, float max);
bool UI_Checkbox(char *label, bool *b);
bool UI_Field(char *label, char *input, int input_length);
bool UI_RadioButton(char *label, int *out, int value);

#endif // UI_H
