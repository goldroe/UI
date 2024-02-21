// Simple driver program to test UI library
// Just places some rectangles and changes their color and background color

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif // _MSC_VER

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

#define WIDTH 1028
#define HEIGHT 800


struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;
};

struct Vector4 {
    float r, g, b, a;
};

struct Rect {
    float x, y;
    float width, height;
};

struct Quad {
    Rect rect;
    Vector4 color;
    // Texture texture;
};


Vector4 RED = {1.0f, 0.0f, 0.0f, 1.0f};

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

int main() {
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

    struct QuadVertex {
        Vector2 position;
        Vector4 color;
        Vector2 uv;
    };

    ID3D11VertexShader *quad_vs = nullptr;
    ID3D11PixelShader *quad_ps = nullptr;
    ID3D11InputLayout *quad_input_layout = nullptr;
    {
        const char *quad_shader =
            "cbuffer VS_CONSTANT_BUFFER : register(b0) {\n"
            "matrix mvp;\n"
            "};\n"
            "Texture2D texture0 : register(t0);\n"
            "sampler sampler0 : register(s0);\n"
            "struct PS_INPUT {\n"
            "float4 pos : SV_POSITION;\n"
            "float4 color : COLOR0;\n"
            "float2 uv : TEXCOORD0;\n"
            "};\n"
            "PS_INPUT VS(float2 in_pos : POSITION, float4 in_color : COLOR0, float2 in_uv : TEXCOORD0) {\n"
            "PS_INPUT output;\n"
            "output.pos = mul(mvp, float4(in_pos, 0.5, 1));\n"
            "output.color = in_color;\n"
            "output.uv = in_uv;\n"
            "return output;\n"
            "}\n"
            "float4 PS(PS_INPUT input) : SV_TARGET {\n"
            "return input.color;\n"
            "}\n"
            ;

        UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG
        compile_flags |= D3DCOMPILE_DEBUG;
#endif

        ID3DBlob *vblob = nullptr, *pblob = nullptr, *err_blob = nullptr;
        if (D3DCompile(quad_shader, strlen(quad_shader), "quad_shader", nullptr, nullptr, "VS", "vs_5_0", compile_flags, 0, &vblob, &err_blob) != S_OK) {
            fprintf(stderr, "Error compiling vertex shader\n%s\n", quad_shader);
            if (err_blob) {
                fprintf(stderr, "%s\n", (char *)err_blob->GetBufferPointer());
                err_blob->Release();
            }
            if (vblob) {
                vblob->Release();
            }
        }
        if (D3DCompile(quad_shader, strlen(quad_shader), "quad_shader", nullptr, nullptr, "PS", "ps_5_0", compile_flags, 0, &pblob, &err_blob) != S_OK) {
            fprintf(stderr, "Error compiling pixel shader\n%s\n", quad_shader);
            if (err_blob) {
                fprintf(stderr, "%s\n", (char *)err_blob->GetBufferPointer());
                err_blob->Release();
            }
            if (vblob) {
                vblob->Release();
            }
        }

        hr = d3d_device->CreateVertexShader(vblob->GetBufferPointer(), vblob->GetBufferSize(), NULL, &quad_vs);
        assert(SUCCEEDED(hr));
        hr = d3d_device->CreatePixelShader(pblob->GetBufferPointer(), pblob->GetBufferSize(), NULL, &quad_ps);
        assert(SUCCEEDED(hr));

        D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION",  0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(QuadVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, offsetof(QuadVertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(QuadVertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = d3d_device->CreateInputLayout(desc, ARRAYSIZE(desc), vblob->GetBufferPointer(), vblob->GetBufferSize(), &quad_input_layout);
        assert(SUCCEEDED(hr));
    }

    ID3D11Buffer *quad_vb = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = 6 * sizeof(QuadVertex);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = d3d_device->CreateBuffer(&desc, nullptr, &quad_vb);
        assert(SUCCEEDED(hr));
    }

    struct QuadCB {
        float mvp[4][4];
    };

    ID3D11Buffer *quad_constant_buffer = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(QuadCB);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = d3d_device->CreateBuffer(&desc, nullptr, &quad_constant_buffer);
        assert(SUCCEEDED(hr));
    }

    Vector2 spawn_pos = {60.0f, 60.0f};

    std::vector<Quad> quads;

    Quad q;
    q.rect = {0, 0, 100, 100};
    q.color = RED;
    quads.push_back(q);

    Quad *selected_quad = &quads[0];

    float bg_color[4] = {1, 1, 1, 1};

    UI_DX11BackendInit(d3d_device, d3d_context);

    bool wireframe_mode = false;

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

        QuadCB quad_cb{};
        quad_cb.mvp[0][0] = 2.0f / (width);
        quad_cb.mvp[1][1] = 2.0f / (height);
        quad_cb.mvp[2][2] = -1.0f;
        quad_cb.mvp[3][3] = 1.0f;
        quad_cb.mvp[3][0] = -(width) / (float)(width);
        quad_cb.mvp[3][1] = -(height) / (float)(height);
        quad_cb.mvp[3][2] = 1.0f;
        {
            D3D11_MAPPED_SUBRESOURCE res{};
            hr = d3d_context->Map(quad_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
            assert(SUCCEEDED(hr));
            memcpy(res.pData, &quad_cb, sizeof(QuadCB));
            d3d_context->Unmap(quad_constant_buffer, 0);
        }

        UI_NewFrame(window);

        if (UI_Button("Create New", 40, 20)) {
            Quad quad{};
            quad.rect.x = spawn_pos.x;
            quad.rect.y = spawn_pos.y;
            quad.rect.width = 100;
            quad.rect.height = 100;
            quad.color = RED;
            if (selected_quad) quad.color = selected_quad->color;
            quads.push_back(quad);
            spawn_pos.x += 60.0f;
            spawn_pos.y += 60.0f;
            selected_quad = &quads.back();
        }

        UI_Checkbox("Box", &wireframe_mode, 200, 20);

        if (UI_Button("Delete", 40, 60)) {
        }

        if (selected_quad) {
            UI_Slider("RED", &selected_quad->color.r, 0.0f, 1.0f, 40, 100);
            UI_Slider("GREEN", &selected_quad->color.g, 0.0f, 1.0f, 40, 140);
            UI_Slider("BLUE", &selected_quad->color.b, 0.0f, 1.0f, 40, 180);
        }

        UI_Slider("BG Red", &bg_color[0], 0.0f, 1.0f, 40, 220);
        UI_Slider("BG Green", &bg_color[1], 0.0f, 1.0f, 40, 260);
        UI_Slider("BG Blue", &bg_color[2], 0.0f, 1.0f, 40, 300);

        UI_EndFrame();

        d3d_context->ClearRenderTargetView(render_target, bg_color);
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

        UINT stride = sizeof(QuadVertex);
        UINT offset = 0;

        d3d_context->IASetVertexBuffers(0, 1, &quad_vb, &stride, &offset);
        d3d_context->VSSetConstantBuffers(0, 1, &quad_constant_buffer);

        d3d_context->IASetInputLayout(quad_input_layout);
        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d_context->VSSetShader(quad_vs, 0, 0);
        d3d_context->PSSetShader(quad_ps, 0, 0);

        d3d_context->OMSetBlendState(nullptr, NULL, 0xffffffff);
        
        for (int i = 0; i < quads.size(); i++) {
            Quad quad = quads[i];
            float x0 = quad.rect.x;
            float x1 = quad.rect.x + quad.rect.width;
            float y0 = quad.rect.y;
            float y1 = quad.rect.y + quad.rect.height;
            QuadVertex verts[6] = {
                { {x0, y0}, quad.color, {0.0f, 0.0f} },
                { {x0, y1}, quad.color, {0.0f, 1.0f} },
                { {x1, y1}, quad.color, {1.0f, 1.0f} },
                { {x0, y0}, quad.color, {0.0f, 0.0f} },
                { {x1, y1}, quad.color, {1.0f, 1.0f} },
                { {x1, y0}, quad.color, {1.0f, 0.0f} },
            };
 
            D3D11_MAPPED_SUBRESOURCE res{};
            d3d_context->Map(quad_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
            memcpy(res.pData, verts, sizeof(verts));
            d3d_context->Unmap(quad_vb, 0 );
            d3d_context->Draw(6, 0);
        }


        UI_Render();

        swapchain->Present(0, 0);

        Sleep(10);
    }

    return 0;
}
