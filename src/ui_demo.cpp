// Main demo for testing the UI library
// Uses Win32 and D3D11

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

// #include <ft2build.h>
// #include FT_FREETYPE_H

#include "UI.h"

#define WIDTH 1028
#define HEIGHT 800

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
    case WM_SIZE: {
        // NOTE: Resize render target view
        if (swapchain) {
            d3d_context->OMSetRenderTargets(0, 0, 0);

            // Release all outstanding references to the swap chain's buffers.
            render_target->Release();

            // Preserve the existing buffer count and format.
            // Automatically choose the width and height to match the client rect for HWNDs.
            HRESULT hr = swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                                            
            // Perform error handling here!

            // Get buffer and create a render-target-view.
            ID3D11Texture2D* buffer;
            hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &buffer);
            // Perform error handling here!

            hr = d3d_device->CreateRenderTargetView(buffer, NULL, &render_target);
            // Perform error handling here!
            buffer->Release();

            d3d_context->OMSetRenderTargets(1, &render_target, NULL);
        }
        break;
    }
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
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        d3d_device->CreateRasterizerState(&desc, &rasterizer_state);
    }

    UI_DX11BackendInit(d3d_device, d3d_context);
    
    bool display_fps = true;
    int radio = 0;
    float slider = 1.0f;
    float frames_per_second = 0.0f;

    const int sample_len = 128;
    char sample_field[sample_len];

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

        UI_NewFrame(window);

        UI_RowBegin("Menu");
            if (UI_Button("File")) {
                printf("File\n");
            }
            if (UI_Button("Edit")) {
                printf("Edit\n");
            }
            if (UI_Button("Help")) {
                printf("Help\n");
            }
            // UI_BorderColorPop();
        UI_RowEnd();

        UI_Widget *widget = UI_WidgetBuild("Table", (UI_WidgetFlags)(UI_WidgetFlags_DrawBorder | UI_WidgetFlags_DrawBackground));
        widget->pref_size[UI_Axis_X] = UI_SIZE_PARENT(0.5f);
        widget->pref_size[UI_Axis_Y] = UI_SIZE_PARENT(1.0f);

        ui_state.parent_stack.push(widget);

        UI_RowBegin("TableHeader");
            UI_Button("First Name");
            UI_Button("Last Name");
            UI_Button("ID");
        UI_RowEnd();

        // UI_Slider("Slider", &slider, 0.0f, 1.0f);

        // if (UI_Field("Field", sample_field, sample_len)) {
        //     printf("%s\n", sample_field);
        // }

        // UI_Checkbox("Display FPS", &display_fps);

        // if (display_fps) {
        //     UI_Labelf("FPS:  %d", (int)frames_per_second);
        // }

        float bg_color[4] = {1, 1, 1, 1};
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

        d3d_context->OMSetBlendState(nullptr, NULL, 0xffffffff);

        UI_EndFrame();

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
        last_counter = end_counter;}
    

    return 0;
}
