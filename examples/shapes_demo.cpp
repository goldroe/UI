// Simple driver program to test UI library
// Just draws some 2d shapes

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif // _MSC_VER

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#undef near
#undef far
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#endif // _WIN32

#include <stdlib.h>
#include <stdio.h>

#include <vector>

#pragma warning(push)
#pragma warning(disable : 4244)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma warning(pop)

#include <ft2build.h>
#include FT_FREETYPE_H

#include "UI.h"

#define HANDMADE_MATH_USE_DEGREES
#include "HandmadeMath.h"

#define WIDTH 1028
#define HEIGHT 800

struct Vertex {
    HMM_Vec2 position;
    HMM_Vec4 color;
};

enum ShapeType {
    Shape_Triangle,
    Shape_Rect,
    Shape_Circle,
};

struct Shape {
    ShapeType type;
    HMM_Vec2 position;
    HMM_Vec4 color;
    std::vector<Vertex> vertices;

    union {
        struct {
            HMM_Vec2 size;
        } rect;
        struct {
            float r;
        } circle;
    };
};

struct CB {
    HMM_Mat4 mvp;
};

Shape CreateTriangle(HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
    Shape shape{};
    shape.type = Shape_Triangle;
    shape.position = position;
    shape.color = color;


    Vertex vertices[3];
    vertices[0].position = HMM_V2(-0.5f*size.X, 0.0f);
    vertices[0].color = color;
    vertices[1].position = HMM_V2(0.5f*size.X, 0.0f);
    vertices[1].color = color;
    vertices[2].position = HMM_V2(0.0f, size.Y);
    vertices[2].color = color;

    for (int i = 0; i < 3; i++) {
        shape.vertices.push_back(vertices[i]);
    }
    return shape;
}

Shape CreateRect(HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
    Shape shape{};
    shape.type = Shape_Rect;
    shape.position = position;
    shape.color = color;

    HMM_Vec2 bl = { -size.X/2.0f, -size.Y/2.0f };
    HMM_Vec2 br = {  size.X/2.0f, -size.Y/2.0f };
    HMM_Vec2 tl = { -size.X/2.0f,  size.Y/2.0f };
    HMM_Vec2 tr = {  size.X/2.0f,  size.Y/2.0f };
    Vertex vertices[6]{};
    vertices[0].position = bl;
    vertices[0].color = color;
    vertices[1].position = tl;
    vertices[1].color = color;
    vertices[2].position = tr;
    vertices[2].color = color;
    vertices[5].position = br;
    vertices[5].color = color;
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    for (int i = 0; i < 6; i++) {
        shape.vertices.push_back(vertices[i]);
    }
    return shape;
}

Shape CreateCircle(HMM_Vec2 position, float r, HMM_Vec4 color) {
    Shape shape{};
    shape.position = position;
    shape.type = Shape_Circle;
    shape.color = color;

    int slice_count = 16;
    shape.vertices.reserve(slice_count * 4);

    // first quadrant
    for (int slice = 0; slice < slice_count; slice++) {
        float angle = 90.0f * (float)slice / (float)slice_count;
        float next_angle =  90.0f * (float)(slice + 1) / (float)slice_count;
        HMM_Vec2 middle = HMM_V2(0.0f, 0.0f);
        HMM_Vec2 left = HMM_V2(r * cosf(next_angle * HMM_DegToRad), r * sinf(next_angle * HMM_DegToRad));
        HMM_Vec2 right = HMM_V2(r * cosf(angle * HMM_DegToRad), r * sinf(angle * HMM_DegToRad));
        Vertex vertex[3];
        vertex[0].position = middle;
        vertex[1].position = left;
        vertex[2].position = right;
        vertex[0].color = color;
        vertex[1].color = color;
        vertex[2].color = color;
        shape.vertices.push_back(vertex[0]);
        shape.vertices.push_back(vertex[1]);
        shape.vertices.push_back(vertex[2]);
    }

    // second
    for (int slice = 0; slice < slice_count; slice++) {
        int index = slice * 3;
        Vertex verts[3] = { shape.vertices[index], shape.vertices[index + 1], shape.vertices[index + 2] };
        verts[0].position.X *= -1;
        verts[1].position.X *= -1;
        verts[2].position.X *= -1;
        shape.vertices.push_back(verts[0]);
        shape.vertices.push_back(verts[1]);
        shape.vertices.push_back(verts[2]);
    }

    // third
    for (int slice = 0; slice < slice_count; slice++) {
        int index = slice * 3;
        Vertex verts[3] = { shape.vertices[index], shape.vertices[index + 1], shape.vertices[index + 2] };
        verts[0].position.X *= -1;
        verts[0].position.Y *= -1;
        verts[1].position.X *= -1;
        verts[1].position.Y *= -1;
        verts[2].position.X *= -1;
        verts[2].position.Y *= -1;
        shape.vertices.push_back(verts[0]);
        shape.vertices.push_back(verts[1]);
        shape.vertices.push_back(verts[2]);
    }

    // fourth
    for (int slice = 0; slice < slice_count; slice++) {
        int index = slice * 3;
        Vertex verts[3] = { shape.vertices[index], shape.vertices[index + 1], shape.vertices[index + 2] };
        verts[0].position.Y *= -1;
        verts[1].position.Y *= -1;
        verts[2].position.Y *= -1;
        shape.vertices.push_back(verts[0]);
        shape.vertices.push_back(verts[1]);
        shape.vertices.push_back(verts[2]);
    }
    return shape;
}

IDXGISwapChain *swapchain;
ID3D11Device *d3d_device;
ID3D11DeviceContext *d3d_context;
ID3D11RenderTargetView *render_target;

bool UI_Win32WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (UI_Win32WindowProc(window, message, wparam, lparam)) {
        return true;
    }

    LRESULT result = 0;
    switch (message) {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcA(window, message, wparam, lparam);
    }
    return result;
}

LARGE_INTEGER performance_frequency;

float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    float Result = (float)(end.QuadPart - start.QuadPart) / (float)performance_frequency.QuadPart;
    return Result;
}

LARGE_INTEGER win32_get_wall_clock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

int main() {
    QueryPerformanceFrequency(&performance_frequency);
    timeBeginPeriod(1);
    UINT desired_scheduler_ms = 1;
    timeBeginPeriod(desired_scheduler_ms);
    DWORD target_frames_per_second = 60;
    DWORD target_ms_per_frame = (int)(1000.0f / target_frames_per_second);

    HRESULT hr = 0;
#define CLASSNAME "imgui_hwnd_class"
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSA window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = CLASSNAME;
    window_class.hInstance = hinstance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    if (!RegisterClassA(&window_class)) {
        printf("RegisterClassA failed, err:%d\n", GetLastError());
    }

    // CREATE WINDOW
    HWND window = 0;
    {
        RECT rc = {0, 0, WIDTH, HEIGHT};
        if (AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE)) {
            window = CreateWindowA(CLASSNAME, "ImGUI", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hinstance, NULL);
        } else {
            printf("AdjustWindowRect failed, err:%d\n", GetLastError());
        }
        if (!window) {
            printf("CreateWindowA failed, err:%d\n", GetLastError());
        }
    }

    // INITIALIZE SWAP CHAIN
    DXGI_SWAP_CHAIN_DESC swapchain_desc{};
    {
        DXGI_MODE_DESC buffer_desc{};
        buffer_desc.Width = WIDTH;
        buffer_desc.Height = HEIGHT;
        buffer_desc.RefreshRate.Numerator = 60;
        buffer_desc.RefreshRate.Denominator = 1;
        buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapchain_desc.BufferDesc = buffer_desc;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SampleDesc.Quality = 0;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount = 1;
        swapchain_desc.OutputWindow = window;
        swapchain_desc.Windowed = TRUE;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }


    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, NULL, D3D11_SDK_VERSION, &swapchain_desc, &swapchain, &d3d_device, NULL, &d3d_context);

    ID3D11Texture2D *backbuffer;
    hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);
    
    hr = d3d_device->CreateRenderTargetView(backbuffer, NULL, &render_target);
    backbuffer->Release();

    ID3D11DepthStencilView *depth_stencil_view = nullptr;

    // DEPTH STENCIL BUFFER
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = WIDTH;
        desc.Height = HEIGHT;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D *depth_stencil_buffer = nullptr;
        hr = d3d_device->CreateTexture2D(&desc, NULL, &depth_stencil_buffer);
        hr = d3d_device->CreateDepthStencilView(depth_stencil_buffer, NULL, &depth_stencil_view);
    }

    // RASTERIZER STATE
    ID3D11RasterizerState *rasterizer_state = nullptr;
    ID3D11RasterizerState *wireframe_state = nullptr;
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        d3d_device->CreateRasterizerState(&desc, &rasterizer_state);
        desc.FillMode = D3D11_FILL_WIREFRAME;
        d3d_device->CreateRasterizerState(&desc, &wireframe_state);
    }

    ID3D11VertexShader *vertex_shader = nullptr;
    ID3D11PixelShader *pixel_shader = nullptr;
    ID3D11InputLayout *input_layout = nullptr;
    {
        const char *shader_source =
            "cbuffer VS_CONSTANT_BUFFER : register(b0) {\n"
            "matrix mvp;\n"
            "};\n"
            "struct PS_INPUT {\n"
            "float4 pos : SV_POSITION;\n"
            "float4 color : COLOR;\n"
            "};\n"
            "PS_INPUT VS(float2 in_pos : POSITION, float4 in_color : COLOR) {\n"
            "PS_INPUT output;\n"
            "output.pos = mul(mvp, float4(in_pos, 0.0, 1));\n"
            "output.color = in_color;\n"
            "return output;\n"
            "}\n"
            "float4 PS(PS_INPUT input) : SV_TARGET {\n"
            "return input.color;\n"
            "}\n";
            ;

        UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG
        compile_flags |= D3DCOMPILE_DEBUG;
#endif

        ID3DBlob *vblob = nullptr, *pblob = nullptr, *err_blob = nullptr;
        if (D3DCompile(shader_source, strlen(shader_source), "vertex_shader", nullptr, nullptr, "VS", "vs_5_0", compile_flags, 0, &vblob, &err_blob) != S_OK) {
            fprintf(stderr, "Error compiling vertex shader\n%s\n", shader_source);
            if (err_blob) {
                fprintf(stderr, "%s\n", (char *)err_blob->GetBufferPointer());
                err_blob->Release();
            }
            if (vblob) {
                vblob->Release();
            }
        }
        if (D3DCompile(shader_source, strlen(shader_source), "pixel_shader", nullptr, nullptr, "PS", "ps_5_0", compile_flags, 0, &pblob, &err_blob) != S_OK) {
            fprintf(stderr, "Error compiling pixel shader\n%s\n", shader_source);
            if (err_blob) {
                fprintf(stderr, "%s\n", (char *)err_blob->GetBufferPointer());
                err_blob->Release();
            }
            if (vblob) {
                vblob->Release();
            }
        }

        hr = d3d_device->CreateVertexShader(vblob->GetBufferPointer(), vblob->GetBufferSize(), NULL, &vertex_shader);
        assert(SUCCEEDED(hr));
        hr = d3d_device->CreatePixelShader(pblob->GetBufferPointer(), pblob->GetBufferSize(), NULL, &pixel_shader);
        assert(SUCCEEDED(hr));

        D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION",  0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, offsetof(Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = d3d_device->CreateInputLayout(desc, ARRAYSIZE(desc), vblob->GetBufferPointer(), vblob->GetBufferSize(), &input_layout);
        assert(SUCCEEDED(hr));
    }

#define MAX_VERTICES 1024
    ID3D11Buffer *vertex_buffer = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = MAX_VERTICES * sizeof(Vertex);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = d3d_device->CreateBuffer(&desc, nullptr, &vertex_buffer);
        assert(SUCCEEDED(hr));
    }

    ID3D11Buffer *constant_buffer = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(CB);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = d3d_device->CreateBuffer(&desc, nullptr, &constant_buffer);
        assert(SUCCEEDED(hr));
    }

    int sample_len = 512;
    char *sample_field = (char *)calloc(1, sample_len);
    

    std::vector<Shape> shapes;
    // Shape test = CreateTriangle({50.0f, 50.0f}, {100.0f, 100.0f}, {1, 0, 0, 1});
    Shape test = CreateCircle({400.0f, 400.0f}, 100.0f, {1, 0, 0, 1});
    shapes.push_back(test);
    // Shape circle = CreateCircle({200.0f, 100.0f}, 20.0f, {0, 0, 1, 1});

    float bg_color[4] = {1, 1, 1, 1};

    UI_DX11BackendInit(d3d_device, d3d_context);
    
    bool wireframe_mode = false;

    int radio = 0;
    float slider = 1.0f;

    float frames_per_second = 0.0f;

    LARGE_INTEGER start_counter = win32_get_wall_clock();
    LARGE_INTEGER last_counter = start_counter;
    bool window_should_close = false;
    while (!window_should_close) {
        MSG message{};
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                window_should_close = true;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        int width, height;
        {
            RECT rc;
            GetClientRect(window, &rc);
            width = rc.right - rc.left;
            height = rc.bottom - rc.top;
        }

        HMM_Mat4 projection = HMM_Orthographic_RH_ZO(0.0f, (float)width, 0.0f, (float)height, 0.0f, 1.0f);


        UI_NewFrame(window);

        UI_BeginRow();
        UI_Button("File");
        UI_Button("View");
        UI_Button("Edit");
        UI_Button("Preferences");
        UI_Button("Help");
        UI_EndRow();

        UI_Labelf("FPS:  %.1f", frames_per_second);

        UI_Checkbox(" Wireframe", &wireframe_mode);
        UI_Slider("Slider", &slider, 0.0f, 1.0f);

        if (UI_Button("Hello World!")) {
            printf("Hello World!\n");
        }

        UI_RadioButton(" White", &radio, 0);
        UI_RadioButton(" Red",   &radio, 1);
        UI_RadioButton(" Green", &radio, 2);
        UI_RadioButton(" Blue",  &radio, 3);

        if (UI_Field("Field", sample_field, sample_len)) {
            printf("%s\n", sample_field);
        }

        UI_EndFrame();

        switch (radio) {
        default:
            bg_color[0] = 1.0f;
            bg_color[1] = 1.0f;
            bg_color[2] = 1.0f;
            bg_color[3] = 1.0f;
            break;
        case 1:
            bg_color[0] = 1.0f;
            bg_color[1] = 0.0f;
            bg_color[2] = 0.0f;
            bg_color[3] = 1.0f;
            break;
        case 2:
            bg_color[0] = 0.0f;
            bg_color[1] = 1.0f;
            bg_color[2] = 0.0f;
            bg_color[3] = 1.0f;
            break;
        case 3:
            bg_color[0] = 0.0f;
            bg_color[1] = 0.0f;
            bg_color[2] = 1.0f;
            bg_color[3] = 1.0f;
            break;
        }

        FLOAT actual_bg[4];
        actual_bg[0] = bg_color[0] * slider;
        actual_bg[1] = bg_color[1] * slider;
        actual_bg[2] = bg_color[2] * slider;
        actual_bg[3] = bg_color[3] * slider;
        
        d3d_context->ClearRenderTargetView(render_target, actual_bg);
        d3d_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
        d3d_context->OMSetRenderTargets(1, &render_target, depth_stencil_view);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)width;
        viewport.Height = (float)height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        d3d_context->RSSetViewports(1, &viewport);

        if (wireframe_mode) {
            d3d_context->RSSetState(wireframe_state);
        } else {
            d3d_context->RSSetState(rasterizer_state);
        }

        UINT stride = sizeof(Vertex);
        UINT offset = 0;

        d3d_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        d3d_context->VSSetConstantBuffers(0, 1, &constant_buffer);

        d3d_context->IASetInputLayout(input_layout);
        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d_context->VSSetShader(vertex_shader, 0, 0);
        d3d_context->PSSetShader(pixel_shader, 0, 0);

        d3d_context->OMSetBlendState(nullptr, NULL, 0xffffffff);


        // draw shapes
        for (int i = 0; i < shapes.size(); i++) {
            Shape shape = shapes[i];
            HMM_Mat4 transform = HMM_Translate(HMM_V3(shape.position.X, shape.position.Y, 0.0f));

            // update vertex buffer
            {
                D3D11_MAPPED_SUBRESOURCE res{};
                hr = d3d_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
                assert(SUCCEEDED(hr));
                memcpy(res.pData, &shape.vertices[0], shape.vertices.size() * sizeof(Vertex));
                d3d_context->Unmap(vertex_buffer, 0);
            }

            // update constastn buffer
            CB cb{};
            cb.mvp = projection * transform;
            {
                D3D11_MAPPED_SUBRESOURCE res{};
                hr = d3d_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
                assert(SUCCEEDED(hr));
                memcpy(res.pData, &cb, sizeof(CB));
                d3d_context->Unmap(constant_buffer, 0);
            }

            d3d_context->Draw((int)shape.vertices.size(), 0);
            
        }

        UI_Render();

        swapchain->Present(0, 0);

        float work_seconds_elapsed = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
        DWORD work_ms = (DWORD)(1000.0f * work_seconds_elapsed);
        if (work_ms < target_ms_per_frame) {
            DWORD sleep_ms = target_ms_per_frame - work_ms;
            Sleep(sleep_ms);
        }

        LARGE_INTEGER end_counter = win32_get_wall_clock();
        float seconds_elapsed = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        frames_per_second = 1000.0f / seconds_elapsed;
        // printf("seconds: %f\n", seconds_elapsed);
        last_counter = end_counter;
    }

    return 0;
}
