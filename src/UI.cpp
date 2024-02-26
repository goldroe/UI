// TODO: Pack some icons, and font textures into a texture atlas (and a single white pixel for rectangles?)
// TODO: Shapes like rounded rectangles and circles
// TODO: Think up a way to specify widget attributes (push/pop?)
// TODO: Hash the widgets and identify with key

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif // _MSC_VER

#include "UI.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

#include "stb_image.h"

#include <ft2build.h>
#include FT_FREETYPE_H

UI_State ui_state;

bool UI_InRect(int x, int y, UI_Rect rect) {
    if (x >= rect.x && x <= (rect.x + rect.width) &&
        y >= rect.y && y <= (rect.y + rect.height)) {
        return true;
    }
    return false;
}

void UI_BorderColor(float r, float g, float b, float a) {
    UI_Vec4 color = {r, g, b, a};
    ui_state.border_color_stack.push(color);
}

void UI_BorderColorPop() {
    ui_state.border_color_stack.pop();
}

UI_Widget *active_widget = nullptr;

bool UI_AnyActive() {
    return active_widget != nullptr;
}

bool UI_IsActive(UI_Widget *widget) {
    assert(widget->label != nullptr);
    if (!active_widget) return false;
    return strcmp(active_widget->label, widget->label) == 0;
}
const UI_Vec4 RED  =   {1.0f, 0.0f, 0.0f, 1.0f};
const UI_Vec4 GREEN  = {0.0f, 1.0f, 0.0f, 1.0f};
const UI_Vec4 BLUE  =  {0.0f, 0.0f, 1.0f, 1.0f};

const UI_Vec4 BLACK = {0.0f, 0.0f, 0.0f, 1.0f};
const UI_Vec4 WHITE = {1.0f, 1.0f, 1.0f, 1.0f};
const UI_Vec4 DARKGRAY  = {0.5f, 0.5, 0.5, 1.0f};
const UI_Vec4 GRAY  = {0.86f, 0.86f, 0.86f, 1.0f};
const UI_Vec4 LIGHTGRAY  = {0.93f, 0.93f, 0.93f, 1.0f};

bool UI_Win32WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CHAR: {
        ui_state.key = (char)(wparam);
        return true;
    }
    case WM_KEYUP:
    case WM_KEYDOWN: {
        ui_state.key_down = (message == WM_KEYDOWN);
        return true;
    }
    case WM_LBUTTONUP:
    case WM_LBUTTONDOWN:
        ui_state.mouse_pressed = ui_state.mouse_down && (message == WM_LBUTTONUP);
        ui_state.mouse_down = (message == WM_LBUTTONDOWN);
        return true;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        float dx = (float)(x - ui_state.mouse_x);
        float dy =(float) (y - ui_state.mouse_y);
        ui_state.mouse_x = x;
        ui_state.mouse_y = y;
        ui_state.mouse_delta = {dx, dy};
        ui_state.dragging = (wparam & MK_LBUTTON);
        return true;
    }
    }
    return false;
}

void UI_DX11BackendInit(ID3D11Device *device, ID3D11DeviceContext *device_context) {
    DX11_Backend_Data *bd = &ui_state.backend_data;
    bd->device = device;
    bd->device_context = device_context;
}

void *UI_GetBackendData() {
    return (void *)&ui_state.backend_data;
}

void UI_Render() {
    DX11_Backend_Data *backend = (DX11_Backend_Data *)UI_GetBackendData();
    UI_Draw_Data *draw_data = &ui_state.draw_data;

    ID3D11Device *device = backend->device;
    ID3D11DeviceContext *context = backend->device_context;

    if (!backend->vertex_buffer || backend->vertex_buffer_size < draw_data->vertex_count) {
        if (backend->vertex_buffer) {
            backend->vertex_buffer->Release();
            backend->vertex_buffer = nullptr;
        }
        D3D11_BUFFER_DESC vb_desc{};
        vb_desc.Usage = D3D11_USAGE_DYNAMIC;
        vb_desc.ByteWidth = draw_data->vertex_capacity * sizeof(UI_Vertex);
        vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (device->CreateBuffer(&vb_desc, nullptr, &backend->vertex_buffer) != S_OK) {
            return;
        }
        backend->vertex_buffer_size = draw_data->vertex_capacity;
    }

    if (!backend->constant_buffer) {
        D3D11_BUFFER_DESC cb_desc{};
        cb_desc.ByteWidth = sizeof(DX11_Constant_Buffer);
        cb_desc.Usage = D3D11_USAGE_DYNAMIC;
        cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (device->CreateBuffer(&cb_desc, nullptr, &backend->constant_buffer) != S_OK) {
            return;
        }
    }

    // NOTE: Upload vertex list data to vertex buffer
    D3D11_MAPPED_SUBRESOURCE vertex_resource{};
    if (context->Map(backend->vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertex_resource) != S_OK) {
        return;
    }
    memcpy(vertex_resource.pData, draw_data->vertex_list, draw_data->vertex_count * sizeof(UI_Vertex));
    context->Unmap(backend->vertex_buffer, 0);

    // NOTE: Create orthographic projection matrix and upload to constant buffer
    {
        float left = draw_data->target_pos.x;
        float right = draw_data->target_pos.x + draw_data->target_size.x;
        float top = draw_data->target_pos.y;
        float bottom = draw_data->target_pos.y + draw_data->target_size.y;
        float near = -1.0f;
        float far = 0.0f;
        float mvp[4][4] = {};
        mvp[0][0] = 2.0f / (right - left);
        mvp[1][1] = 2.0f / (top - bottom);
        mvp[2][2] = 1.0f / (near - far);
        mvp[3][3] = 1.0f;
        mvp[3][0] = (left + right) / (left - right);
        mvp[3][1] = (bottom + top) / (bottom - top);
        mvp[3][2] = (near) / (near - far);

        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        if (context->Map(backend->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK) {
            return;
        }
        DX11_Constant_Buffer *constant_buffer = (DX11_Constant_Buffer *)mapped_resource.pData;
        memcpy(constant_buffer->mvp, mvp, sizeof(mvp));
        context->Unmap(backend->constant_buffer, 0);
    }

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = draw_data->target_pos.x;
    viewport.TopLeftY = draw_data->target_pos.y;
    viewport.Width = draw_data->target_size.x;
    viewport.Height = draw_data->target_size.y;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    UINT stride = sizeof(UI_Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &backend->vertex_buffer, &stride, &offset);
    context->VSSetConstantBuffers(0, 1, &backend->constant_buffer);

    context->IASetInputLayout(backend->input_layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(backend->vertex_shader, 0, 0);

    context->PSSetShader(backend->pixel_shader, 0, 0);
    context->PSSetSamplers(0, 1, &backend->font_sampler);
    context->PSSetShaderResources(0, 1, &backend->font_texture_view);

    context->RSSetState(backend->rasterizer_state);
    context->RSSetViewports(1, &viewport);

    float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(backend->blend_state, blend_factor, 0xffffffff);
    context->OMSetDepthStencilState(backend->depth_stencil_state, 0);

    context->Draw(draw_data->vertex_count, 0);
}

void UI_DX11CreateDeviceObjects(DX11_Backend_Data *bd) {
    ID3D11Device *device = bd->device;
    assert(device);

    // VERTEX AND PIXEL SHADER
    {
        const char *vertex_src =
            "cbuffer VS_CONSTANT_BUFFER : register(b0){\n"
            "matrix mvp;\n"
            "};\n"
            "struct PS_INPUT {\n"
            "float4 pos : SV_POSITION;\n"
            "float4 color : COLOR0;\n"
            "float2 uv : TEXCOORD0;\n"
            "};\n"
            "Texture2D texture0 : register(t0);\n"
            "sampler sampler0 : register(s0);\n"
            "PS_INPUT VS(float2 in_pos : POSITION, float4 in_color : COLOR0, float2 in_uv : TEXCOORD0) {\n"
            "PS_INPUT output;\n"
            "output.pos = mul(mvp, float4(in_pos, 0.0, 1.0));\n"
            "output.color = in_color;\n"
            "output.uv = in_uv;\n"
            "return output;\n"
            "}\n";
        const char *pixel_src =
            "struct PS_INPUT {\n"
            "float4 pos : SV_POSITION;\n"
            "float4 color : COLOR0;\n"
            "float2 uv : TEXCOORD0;\n"
            "};\n"
            "Texture2D texture0 : register(t0);\n"
            "sampler sampler0 : register(s0);\n"
            "float4 PS(PS_INPUT input) : SV_TARGET {\n"
            "return texture0.Sample(sampler0, input.uv).r * input.color;\n"
            "}\n";

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG 
        flags |= D3DCOMPILE_DEBUG;
#endif

        ID3DBlob *vertex_blob = nullptr;
        ID3DBlob *pixel_blob = nullptr;
        ID3DBlob *error_blob = nullptr;
        HRESULT hr = D3DCompile(vertex_src, strlen(vertex_src), NULL, NULL, NULL, "VS", "vs_5_0", 0, 0, &vertex_blob, &error_blob);
        if (FAILED(hr)) {
            printf("Error compiling vertex shader\n%s\n", vertex_src);
            if (error_blob) {
                printf("%s\n", (char *)error_blob->GetBufferPointer());
                error_blob->Release();
            }
            if (vertex_blob) {
                vertex_blob->Release();
            }
            assert(false);
        }
        hr = D3DCompile(pixel_src, strlen(pixel_src), NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &pixel_blob, &error_blob);
        if (FAILED(hr)) {
            printf("Error compiling pixel shader\n%s\n", pixel_src);
            if (error_blob) {
                printf("%s\n", (char *)error_blob->GetBufferPointer());
                error_blob->Release();
            }
            if (vertex_blob) {
                vertex_blob->Release();
            }
            assert(false);
        }
    
        hr = bd->device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), NULL, &bd->vertex_shader);
        assert(SUCCEEDED(hr));
        hr = bd->device->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), NULL, &bd->pixel_shader);
        assert(SUCCEEDED(hr));

        // INPUT LAYOUT
        D3D11_INPUT_ELEMENT_DESC input_layout_desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(UI_Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(UI_Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(UI_Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        hr = bd->device->CreateInputLayout(input_layout_desc, ARRAYSIZE(input_layout_desc), vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &bd->input_layout);
        assert(SUCCEEDED(hr));
    }

    // DEPTH-STENCIL STATE
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        bd->device->CreateDepthStencilState(&desc, &bd->depth_stencil_state);
    }

    // BLEND STATE
    {
        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bd->device->CreateBlendState(&desc, &bd->blend_state);
    }

    // RASTERIZER STATE
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = false;
        desc.DepthClipEnable = false;
        bd->device->CreateRasterizerState(&desc, &bd->rasterizer_state);
    }

    // FONT TEXTURE VIEW
    const char *font_name = "fonts/arial.ttf";
    int font_height = 16;
    FontAtlas atlas{};
    {
        FT_Library ft_lib;
        int err = FT_Init_FreeType(&ft_lib);
        if (err) {
            printf("Error creaing freetype library: %d\n", err);
        }

        FT_Face face;
        err = FT_New_Face(ft_lib, font_name, 0, &face);
        if (err == FT_Err_Unknown_File_Format) {
            printf("Format not supported\n");
        } else if (err) {
            printf("Font file could not be read\n");
        }

        err = FT_Set_Pixel_Sizes(face, 0, font_height);
        if (err) {
            printf("Error setting pixel sizes of font\n");
        }

        int bbox_ymax = FT_MulFix(face->bbox.yMax, face->size->metrics.y_scale) >> 6;
        int bbox_ymin = FT_MulFix(face->bbox.yMin, face->size->metrics.y_scale) >> 6;
        int height = bbox_ymax - bbox_ymin;
        float ascend = face->size->metrics.ascender / 64.f;
        float descend = face->size->metrics.descender / 64.f;
        float bbox_height = (float)(bbox_ymax - bbox_ymin);
        float glyph_height = (float)face->size->metrics.height / 64.f;
        float glyph_width = (float)(face->bbox.xMax - face->bbox.xMin) / 64.f;

        int atlas_width = 0;
        int atlas_height = 0;
        int max_bmp_height = 0;
        for (unsigned char c = 32; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                printf("Error loading char %c\n", c);
                continue;
            }

            atlas_width += face->glyph->bitmap.width;
            if (atlas_height < (int)face->glyph->bitmap.rows) {
                atlas_height = face->glyph->bitmap.rows;
            }

            int bmp_height = face->glyph->bitmap.rows + face->glyph->bitmap_top;
            if (max_bmp_height < bmp_height) {
                max_bmp_height = bmp_height;
            }
        }

        // +1 for the white pixel
        atlas_width = atlas_width + 1;
        int atlas_x = 1;
        
        // Pack glyph bitmaps
        unsigned char *bitmap = (unsigned char *)calloc(atlas_width * atlas_height + 1, 1);
        bitmap[0] = 255;
        for (unsigned char c = 32; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                printf("Error loading char '%c'\n", c);
            }

            FontGlyph *glyph = &atlas.glyphs[c];
            glyph->ax = (float)(face->glyph->advance.x >> 6);
            glyph->ay = (float)(face->glyph->advance.y >> 6);
            glyph->bx = (float)face->glyph->bitmap.width;
            glyph->by = (float)face->glyph->bitmap.rows;
            glyph->bt = (float)face->glyph->bitmap_top;
            glyph->bl = (float)face->glyph->bitmap_left;
            glyph->to = (float)atlas_x / atlas_width;
            
            // Write glyph bitmap to atlas
            for (int y = 0; y < glyph->by; y++) {
                unsigned char *dest = bitmap + y * atlas_width + atlas_x;
                unsigned char *source = face->glyph->bitmap.buffer + y * face->glyph->bitmap.width;
                memcpy(dest, source, face->glyph->bitmap.width);
            }

            atlas_x += face->glyph->bitmap.width;
        }

        atlas.width = atlas_width;
        atlas.height = atlas_height;
        atlas.max_bmp_height = max_bmp_height;
        atlas.ascend = ascend;
        atlas.descend = descend;
        atlas.bbox_height = height;
        atlas.glyph_width = glyph_width;
        atlas.glyph_height = glyph_height;

        FT_Done_Face(face);
        FT_Done_FreeType(ft_lib);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = atlas_width;
        desc.Height = atlas_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        ID3D11Texture2D *font_texture = nullptr;
        D3D11_SUBRESOURCE_DATA sr_data{};
        sr_data.pSysMem = bitmap;
        sr_data.SysMemPitch = atlas_width;
        sr_data.SysMemSlicePitch = 0;
        HRESULT hr = bd->device->CreateTexture2D(&desc, &sr_data, &font_texture);
        assert(SUCCEEDED(hr));
        assert(font_texture != nullptr);
        hr = bd->device->CreateShaderResourceView(font_texture, nullptr, &bd->font_texture_view);
        assert(SUCCEEDED(hr));

        ui_state.font_atlas = atlas;
    }

    // FONT SAMPLER
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        HRESULT hr = bd->device->CreateSamplerState(&desc, &bd->font_sampler);
        assert(SUCCEEDED(hr));
    }

}

void UI_DX11NewFrame() {
    DX11_Backend_Data *bd = &ui_state.backend_data;
    if (!bd->font_sampler) {
        UI_DX11CreateDeviceObjects(bd);
    }
}

float UI_GetTextWidthRanged(char *text, int start, int end, FontAtlas *font) {
    float width = 0.0f;
    for (char *ptr = text + start; ptr < text + end; ptr++) {
        FontGlyph glyph = font->glyphs[*ptr];
        width += glyph.ax;
    }
    return roundf(width);
}

float UI_GetTextWidth(char *text, FontAtlas *font) {
    return UI_GetTextWidthRanged(text, 0, (int)strlen(text), font);
}

float UI_GetTextHeight(char *text, FontAtlas *font) {
    float height = font->glyph_height;
    for (char *ptr = text; *ptr; ptr++) {
        if (*ptr == '\n') {
            height += font->glyph_height;
        }
    }
    return roundf(height);
}

void UI_PushVertex(UI_Draw_Data *draw_data, UI_Vertex vertex) {
    draw_data->vertex_count++;
    if (draw_data->vertex_count >= draw_data->vertex_capacity) {
        draw_data->vertex_capacity += (draw_data->vertex_capacity / 2) + 1; // new_cap = cap * 1.5h
        draw_data->vertex_list = (UI_Vertex *)realloc(draw_data->vertex_list, draw_data->vertex_capacity * sizeof(UI_Vertex));
    }
    draw_data->vertex_list[draw_data->vertex_count - 1] = vertex;
}

void UI_DrawLine(UI_Vec2 start, UI_Vec2 end, UI_Vec4 color, float thickness) {
    float angle = atan2f(end.y - start.y, end.x - start.x);
    float half_thickness = thickness / 2.0f;
    UI_Vertex vertices[6];
    vertices[0].position = { start.x + half_thickness * cosf(angle + 0.5f*(float)M_PI), start.y + half_thickness * sinf(angle + 0.5f*(float)M_PI) };
    vertices[1].position = { end.x   + half_thickness * cosf(angle + 0.5f*(float)M_PI), end.y   + half_thickness * sinf(angle + 0.5f*(float)M_PI) };
    vertices[2].position = { start.x + half_thickness * cosf(angle - 0.5f*(float)M_PI), start.y + half_thickness * sinf(angle - 0.5f*(float)M_PI) };
    vertices[5].position = { end.x   + half_thickness * cosf(angle - 0.5f*(float)M_PI), end.y   + half_thickness * sinf(angle - 0.5f*(float)M_PI) };
    vertices[3] = vertices[1];
    vertices[4] = vertices[2];
    for (int i = 0; i < 6; i++) {
        vertices[i].color = color;
        UI_PushVertex(&ui_state.draw_data, vertices[i]);
    }
}

void UI_DrawTextOffset(char *text, FontAtlas *font, UI_Vec2 position, float offset) {
    for (char *ptr = text; *ptr; ptr++) {
        int glyph_index = *ptr;
        FontGlyph glyph = font->glyphs[glyph_index];
        float x0 = position.x + glyph.bl - offset;
        float x1 = x0 + glyph.bx;
        float y0 = position.y - glyph.bt + font->ascend;
        float y1 = y0 + glyph.by;

        float tw = glyph.bx / (float)font->width;
        float th = glyph.by / (float)font->height;
        float tx = glyph.to;
        float ty = 0.0f;

        if (position.x > offset + glyph.bl) {
            UI_Vertex vertices[6]{};
            vertices[0].position = {x0, y1};
            vertices[0].color = BLACK;
            vertices[0].uv = {tx, ty + th};
            vertices[1].position = {x0, y0};
            vertices[1].color = BLACK;
            vertices[1].uv = {tx, ty};
            vertices[2].position = {x1, y0};
            vertices[2].color = BLACK;
            vertices[2].uv = {tx + tw, ty};
            vertices[5].position = {x1, y1};
            vertices[5].color = BLACK;
            vertices[5].uv = {tx + tw, ty + th};
            vertices[3] = vertices[0];
            vertices[4] = vertices[2];
            for (int i = 0; i < 6; i++) {
                UI_PushVertex(&ui_state.draw_data, vertices[i]);
            }
        }
        position.x += glyph.ax;
    }
}

void UI_DrawText(char *text, FontAtlas *font, UI_Vec2 position) {
    for (char *ptr = text; *ptr; ptr++) {
        int glyph_index = *ptr;
        FontGlyph glyph = font->glyphs[glyph_index];
        float x0 = position.x + glyph.bl;
        float x1 = x0 + glyph.bx;
        float y0 = position.y - glyph.bt + font->ascend;
        float y1 = y0 + glyph.by;

        float tw = glyph.bx / (float)font->width;
        float th = glyph.by / (float)font->height;
        float tx = glyph.to;
        float ty = 0.0f;

        UI_Vertex vertices[6]{};
        vertices[0].position = {x0, y1};
        vertices[0].color = BLACK;
        vertices[0].uv = {tx, ty + th};
        vertices[1].position = {x0, y0};
        vertices[1].color = BLACK;
        vertices[1].uv = {tx, ty};
        vertices[2].position = {x1, y0};
        vertices[2].color = BLACK;
        vertices[2].uv = {tx + tw, ty};
        vertices[5].position = {x1, y1};
        vertices[5].color = BLACK;
        vertices[5].uv = {tx + tw, ty + th};
        vertices[3] = vertices[0];
        vertices[4] = vertices[2];
        for (int i = 0; i < 6; i++) {
            UI_PushVertex(&ui_state.draw_data, vertices[i]);
        }
        position.x += glyph.ax;
    }
}

void UI_DrawRect(UI_Rect rect, UI_Vec4 color) {
    float x0 = (float)rect.x;
    float y0 = (float)rect.y;
    float x1 = (float)rect.x + rect.width;
    float y1 = (float)rect.y + rect.height;

    UI_Vertex vertices[6]{};
    vertices[0].position = {x0, y1};
    vertices[0].color = color;
    // vertices[0].uv = {0.0f, 1.0f};
    vertices[1].position = {x0, y0};
    vertices[1].color = color;
    // vertices[1].uv = {0.0f, 0.0f};
    vertices[2].position = {x1, y0};
    vertices[2].color = color;
    // vertices[2].uv = {1.0f, 0.0f};
    vertices[5].position = {x1, y1};
    vertices[5].color = color;
    // vertices[5].uv = {1.0f, 1.0f};
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    for (int i = 0; i < 6; i++) {
        UI_PushVertex(&ui_state.draw_data, vertices[i]);
    }
}

void UI_DrawRectOutline(UI_Rect rect, UI_Vec4 color) {
    float x0 = rect.x;
    float y0 = rect.y;
    float x1 = rect.x + rect.width;
    float y1 = rect.y + rect.height;

    float thickness = 1;
    // top 
    UI_DrawRect({x0, y0, rect.width, thickness}, color);
    // bottom
    UI_DrawRect({x0, y1 - thickness, rect.width, thickness}, color);
    // left
    UI_DrawRect({x0, y0, thickness, rect.height}, color);
    // right
    UI_DrawRect({x1 - thickness, y0, thickness, rect.height}, color);
}


void UI_DrawCheckMark(UI_Vec2 position, UI_Vec2 bound, UI_Vec4 color) {
    float width = bound.x - position.x;
    float height = bound.y - position.y;

    UI_Vec2 left_tick = {position.x + 0.1f*width, position.y + 0.4f * height};
    UI_Vec2 middle = {position.x + 0.4f*width, position.y + 0.9f*height};
    UI_Vec2 right_tick = {bound.x, position.y + 0.1f * height};
    UI_DrawLine(left_tick, middle, color, 2.0f);
    UI_DrawLine(middle, right_tick, color, 2.0f);
}

UI_Widget *UI_GetParent() {
    UI_Widget *parent = nullptr;
    if (!ui_state.parent_stack.empty()) parent = ui_state.parent_stack.top();
    return parent;
}

UI_Widget *UI_FindWidget(char *label) {
    UI_Widget *result = nullptr;
    for (int i = 0; i < ui_state.old_list.size(); i++) {
        UI_Widget *widget = ui_state.old_list[i];
        if (strcmp(widget->label, label) == 0) {
            result = widget;
            break;
        }
    }
    return result;
}

UI_Widget *UI_WidgetCreate() {
    UI_Widget *widget = (UI_Widget *)calloc(1, sizeof(UI_Widget));
    return widget;
}

UI_Widget *UI_WidgetCreate(char *label) {
    UI_Widget *widget = (UI_Widget *)calloc(1, sizeof(UI_Widget));
    widget->label = (char *)malloc(strlen(label) + 1);
    strcpy(widget->label, label);
    widget->next = nullptr;
    return widget;
}

UI_Widget *UI_WidgetCopy(UI_Widget *widget) {
    UI_Widget *result = (UI_Widget *)calloc(1, sizeof(UI_Widget));
    memcpy(result, widget, sizeof(UI_Widget));
    result->active = widget->active;
    result->label = (char *)malloc(strlen(widget->label) + 1);
    strcpy(result->label, widget->label);
    result->next = nullptr;
    return result;
}

void UI_WidgetActivate(UI_Widget *widget) {
    widget->active = true;
    active_widget = UI_WidgetCopy(widget);
}

void UI_WidgetDestroy(UI_Widget *widget) {
    assert(widget);
    free(widget->label);
    free(widget);
}

void UI_WidgetDeactivate() {
    UI_WidgetDestroy(active_widget);
    active_widget = nullptr;
}

UI_Widget *UI_WidgetBuild(char *label, UI_WidgetFlags flags) {
    UI_Widget *widget = nullptr;
    UI_Widget *found = UI_FindWidget(label);
    if (found) {
        widget = UI_WidgetCopy(found);
    } else {
        widget = UI_WidgetCreate(label);
    }
    widget->flags = flags;

    widget->first = widget->last = nullptr;
    widget->next = widget->prev = nullptr;

    UI_Widget *parent = UI_GetParent();

    if (parent != nullptr) {
        if (parent->first) {
            widget->prev = parent->last;
            parent->last->next = widget;
            parent->last = widget;
        } else {
            parent->first = parent->last = widget;
        }
    } else {
        ui_state.root = widget;
    }
    widget->parent = parent;

    widget->bg_color = ui_state.bg_color_stack.top();
    widget->border_color = ui_state.border_color_stack.top();
    widget->text_color = ui_state.text_color_stack.top();
    
    ui_state.widget_list.push_back(widget);
    return widget; 
}

void UI_PushPrefSize(UI_Axis axis, UI_Size size) {
    switch (axis) {
    case UI_Axis_X:
        ui_state.pref_width_stack.push(size);
        break;
    case UI_Axis_Y:
        ui_state.pref_height_stack.push(size);
        break;
    }
    // ui_state.pref_size_stack.push(size);
}

UI_Size UI_PopPrefSize(UI_Axis axis) {
    UI_Size size = {};
    switch (axis) {
    case UI_Axis_X:
        size = !ui_state.pref_width_stack.empty() ? ui_state.pref_width_stack.top() : size;
        if (!ui_state.pref_width_stack.empty()) ui_state.pref_width_stack.pop();
        break;
    case UI_Axis_Y:
        size = !ui_state.pref_height_stack.empty() ? ui_state.pref_height_stack.top() : size;
        if (!ui_state.pref_height_stack.empty()) ui_state.pref_height_stack.pop();
    }
    return size;
}

UI_Size UI_GetNextPrefSize(UI_Axis axis) {
    UI_Size size{};
    switch (axis) {
    case UI_Axis_X:
        size = !ui_state.pref_width_stack.empty() ? ui_state.pref_width_stack.top() : size;
        break;
    case UI_Axis_Y:
        size = !ui_state.pref_height_stack.empty() ? ui_state.pref_height_stack.top() : size;
        break;
    }
    return size;
}

void UI_LayoutCalcSizesRigid(UI_Widget *widget, UI_Axis axis) {
    float size = 0;
    switch (widget->pref_size[axis].type) {
    case UI_Size_Pixels:
        size = widget->pref_size[axis].value;
        break;
    case UI_Size_TextBounds: {
        float padding = widget->pref_size[axis].value;
        size = (axis == UI_Axis_X) ? (UI_GetTextWidth(widget->label, &ui_state.font_atlas) + padding) : UI_GetTextHeight(widget->label, &ui_state.font_atlas);
        break;
    }
    }

    widget->actual_size[axis] = size;

    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        UI_LayoutCalcSizesRigid(child, axis);
    }
}

void UI_LayoutCalcSizesUpwardDependent(UI_Widget *widget, UI_Axis axis) {
    switch (widget->pref_size[axis].type) {
    case UI_Size_ParentPercent: {
        UI_Widget *rigid = nullptr;
        for (UI_Widget *p = widget->parent; p != nullptr; p = p->parent) {
            UI_Size size = p->pref_size[axis];
            if (size.type == UI_Size_Pixels || size.type == UI_Size_TextBounds) {
                rigid = p;
                break;
            }
        }
        widget->actual_size[axis] = widget->pref_size[axis].value * rigid->actual_size[axis];
        break;
    }
    default:
        break;
    }

    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        UI_LayoutCalcSizesUpwardDependent(child, axis);
    }
}

void UI_LayoutCalcSizesDownwardDependent(UI_Widget *widget, UI_Axis axis) {
    float children_sum = 0;
    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        UI_LayoutCalcSizesDownwardDependent(child, axis);
        children_sum += child->actual_size[axis];
    }
        
    switch (widget->pref_size[axis].type) {
    case UI_Size_ChildrenSum:
        widget->actual_size[axis] = children_sum;
        break;
    }
}

void UI_LayoutResolveSize(UI_Widget *widget, UI_Axis axis) {
    UI_Widget *parent = widget->parent;
    if (parent) {
        widget->actual_size[axis] = UI_CLAMP(widget->actual_size[axis], 0, parent->actual_size[axis]);
    }

    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        UI_LayoutResolveSize(child, axis);
    }
}

void UI_LayoutPlaceWidgets(UI_Widget *widget, UI_Axis axis) {
    float current_pos = 0;
    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        child->relative_pos[axis] = current_pos;

        // NOTE: Move relative position in the layout axis for children
        if (axis == widget->child_layout_axis) {
            current_pos += child->actual_size[axis]; 
        }
    }

    float absolute_pos = 0;
    for (UI_Widget *p = widget; p != nullptr; p = p->parent) {
        absolute_pos += p->relative_pos[axis];
    }

    widget->rect.p[axis] = absolute_pos;
    widget->rect.width = widget->actual_size[UI_Axis_X];
    widget->rect.height = widget->actual_size[UI_Axis_Y];

    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        UI_LayoutPlaceWidgets(child, axis);
    }
}

void UI_LayoutRoot(UI_Widget *root, UI_Axis axis) {
    UI_LayoutCalcSizesRigid(root, axis);
    UI_LayoutCalcSizesUpwardDependent(root, axis);
    UI_LayoutCalcSizesDownwardDependent(root, axis);
    UI_LayoutResolveSize(root, axis);
    UI_LayoutPlaceWidgets(root, axis);
}

void UI_NewFrame(HWND window) {
    RECT client_rect;
    GetClientRect(window, &client_rect);
    UI_Vec2 dim = {(float)(client_rect.right - client_rect.left), (float)(client_rect.bottom - client_rect.top)};
    ui_state.draw_data.target_size = dim;
    ui_state.draw_data.target_pos = {0.0f, 0.0f};
    ui_state.draw_data.vertex_count = 0;

    // Clear layout stacks
    STACK_CLEAR(ui_state.parent_stack);
    STACK_CLEAR(ui_state.bg_color_stack);
    STACK_CLEAR(ui_state.pref_width_stack);
    STACK_CLEAR(ui_state.pref_height_stack);

    ui_state.bg_color_stack.push(WHITE);
    ui_state.border_color_stack.push(GRAY);
    ui_state.text_color_stack.push(BLACK);

    UI_Widget *root = UI_WidgetBuild("~Root", (UI_WidgetFlags)(UI_WidgetFlags_DrawBackground | UI_WidgetFlags_DrawBorder));
    root->child_layout_axis = UI_Axis_Y;
    root->rect = {0, 0, dim.x, dim.y};
    root->pref_size[UI_Axis_X] = UI_SIZE_FIXED(dim.x);
    root->pref_size[UI_Axis_Y] = UI_SIZE_FIXED(dim.y);
    root->bg_color = WHITE;
    root->border_color = WHITE;

    ui_state.parent_stack.push(root);

    // DX11
    UI_DX11NewFrame();
}

void UI_DrawLayoutRoot(UI_Widget *widget) {
    if (widget->flags & UI_WidgetFlags_DrawBackground) {
        UI_DrawRect(widget->rect, widget->bg_color);
    }
    if (widget->flags & UI_WidgetFlags_DrawBorder) {
        UI_DrawRectOutline(widget->rect, widget->border_color);
    }
    if (widget->flags & UI_WidgetFlags_DrawText) {
        UI_DrawText(widget->label, &ui_state.font_atlas, UI_Vec2(widget->rect.x + widget->pref_size[UI_Axis_X].value / 2.0f, widget->rect.y));
    }
    if (widget->flags & UI_WidgetFlags_DrawHotEffects) {
        UI_DrawRect(widget->rect, UI_Vec4(0.25f, 0.75f, 1.0f, 0.15f));
    }
    if (widget->flags & UI_WidgetFlags_DrawActiveEffects) {
        UI_DrawRect(widget->rect, UI_Vec4(0.25f, 0.75f, 1.0f, 0.15f));
    }

    for (UI_Widget *child = widget->first; child != nullptr; child = child->next) {
        UI_DrawLayoutRoot(child);
    }
}

void UI_EndFrame() {
    ui_state.mouse_pressed = false;
    ui_state.key_down = false;

    UI_Widget *root = ui_state.root;
    UI_LayoutRoot(root, UI_Axis_X);
    UI_LayoutRoot(root, UI_Axis_Y);

    UI_DrawLayoutRoot(root);

    // Free old list
    for (int i = 0; i < ui_state.old_list.size(); i++) {
        UI_Widget *w = ui_state.old_list[i];
        free(w);
    }

    // Swap current build data to old
    ui_state.old_root = ui_state.root;
    ui_state.root = nullptr;
    ui_state.old_list.swap(ui_state.widget_list);
    ui_state.widget_list.clear();

    // NOTE: If active widget wasn't built this frame then no longer active
    if (UI_AnyActive()) {
        UI_Widget *active = UI_FindWidget(active_widget->label);
        if (active == nullptr) {
            active_widget = nullptr;
        }    
    }

    UI_Render();
}


// NOTE: UI Widget Objects

/* Basic Widget Logic
   bool DoButton(char *label, ...) {
     if (active) {
       if (mouse_went_up) {
         if (hot) result = true;
         SetNotActive();
       }
     } else if (hot) {
       if (mouse_down) SetActive();
     }
     if (inside) SetHot();
   }

*/

bool UI_ButtonBehavior(UI_Widget *widget, bool *hover) {
    bool clicked = false;
    bool inside = false;
    if (widget) {
        UI_Rect rect = widget->rect;
        inside = UI_InRect(ui_state.mouse_x, ui_state.mouse_y, rect);

        if (UI_IsActive(widget)) {
            if (ui_state.mouse_pressed) {
                if (inside) clicked = true;
                UI_WidgetDeactivate();
            }
        } else if (inside) {
            if (ui_state.mouse_down) UI_WidgetActivate(widget);
        }

    }
    if (hover) {
        *hover = inside;
    }

    return clicked;
}

void UI_RowBegin(char *label) {
    UI_Widget *widget = UI_WidgetBuild(label, (UI_WidgetFlags)(UI_WidgetFlags_DrawBackground | UI_WidgetFlags_DrawBorder));
    widget->pref_size[UI_Axis_X] = UI_SIZE_PARENT(1.0f);
    widget->pref_size[UI_Axis_Y] = UI_SIZE_FIXED(ui_state.font_atlas.glyph_height);
    widget->child_layout_axis = UI_Axis_X;
    // UI_PushPrefSize(UI_Axis_X, UI_SIZE_TEXT(1.0f));
    // UI_PushPrefSize(UI_Axis_Y, UI_SIZE_TEXT(1.0f));
    ui_state.parent_stack.push(widget);
}

void UI_RowEnd() {
    ui_state.parent_stack.pop();
    UI_PopPrefSize(UI_Axis_X);
    UI_PopPrefSize(UI_Axis_Y);
}

bool UI_Button(char *label) {
    UI_Widget *widget = UI_FindWidget(label);
    bool hover = false;
    bool clicked = UI_ButtonBehavior(widget, &hover);

    UI_Widget *new_widget = UI_WidgetBuild(label, (UI_WidgetFlags)(UI_WidgetFlags_DrawText | UI_WidgetFlags_DrawBorder | UI_WidgetFlags_DrawBackground));
    new_widget->pref_size[UI_Axis_X] = UI_SIZE_TEXT(20.0f);
    new_widget->pref_size[UI_Axis_Y] = UI_SIZE_TEXT(0.0f);

    if (hover) {
        new_widget->flags = (UI_WidgetFlags)((int)new_widget->flags | UI_WidgetFlags_DrawHotEffects);
    }
    if (UI_IsActive(new_widget)) {
        new_widget->flags = (UI_WidgetFlags)((int)new_widget->flags | UI_WidgetFlags_DrawActiveEffects);
    }

    return clicked;
}

#if 0
void UI_Label(char *label) {
    float width = UI_GetTextWidth(label, &ui_state.font_atlas) + 10.0f;
    float height = 20.0f;
    UI_Rect rect = {position.x, position.y, width, height};

    // UI_DrawRectOutline(rect, WHITE);
    UI_DrawText(label, &ui_state.font_atlas, UI_Vec2(position.x + 4.0f, position.y));

    ui_state.next_position = UI_Vec2(NEXT_PX, rect.y + rect.height + 20.0f);
}

void UI_Labelf(char *fmt, ...) {
    char buffer[128] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 128, fmt, args);
    va_end(args);
    UI_Label(buffer);
}

void UI_Slider(char *label, float *f, float min, float max) {
    float d = max - min;
    *f = UI_CLAMP(*f, min, max);

    UI_Widget *widget = UI_FindWidget(label);
    bool just_created = widget == nullptr;
    widget = UI_WidgetCreate(label);
    UI_WidgetPush(widget);
    
    UI_Vec2 position = ui_state.next_position;
    float text_width = UI_GetTextWidth(label, &ui_state.font_atlas);
    UI_Rect bar_rect = {position.x, position.y, 200, 5};

    bool hover = UI_InRect(ui_state.mouse_x, ui_state.mouse_y, bar_rect);

    if (!UI_AnyActive()) {
        if (hover && ui_state.mouse_down) {
            UI_WidgetActivate(widget);
        }
    }

    float offset = ((*f - min) / d);
    UI_Vec2 slider_position = UI_Vec2(position.x, position.y);
    slider_position.x += offset * bar_rect.width;

    if (UI_IsActive(widget)) {
        if (ui_state.mouse_down) {
            slider_position.x = (float)ui_state.mouse_x;
            slider_position.x = UI_CLAMP(slider_position.x, bar_rect.x, bar_rect.x + bar_rect.width);
            float r = (slider_position.x - bar_rect.x) / (float)(bar_rect.width);
            float new_f =  min + d * r;
            *f = new_f;
        } else if (ui_state.mouse_pressed || !ui_state.mouse_down) {
            UI_WidgetDeactivate();
        }

    }

    UI_Vec4 color = (hover && !UI_AnyActive()) || UI_IsActive(widget) ? LIGHTGRAY : GRAY;

    UI_Rect slider_rect = {slider_position.x, slider_position.y - 5, 8, 20};

    UI_DrawRectOutline(bar_rect, color);
    UI_DrawRectOutline(slider_rect, GRAY);

    ui_state.next_position = UI_Vec2(NEXT_PX, slider_rect.y + slider_rect.height + 20.0f);
}

bool UI_Checkbox(char *label, bool *b) {
    bool result = false;
    float height = 12;
    float button_width = 12;
    float width = UI_GetTextWidth(label, &ui_state.font_atlas) + button_width;
    UI_Rect button_rect = {ui_state.next_position.x, ui_state.next_position.y, button_width, height};

    bool hover = UI_InRect(ui_state.mouse_x, ui_state.mouse_y, button_rect);

    UI_Widget *widget = UI_FindWidget(label);
    widget = UI_WidgetCreate(label);
    UI_WidgetPush(widget);

    if (UI_IsActive(widget)) {
        if (ui_state.mouse_pressed) {
            if (hover) {
                result = true;
            }
            UI_WidgetDeactivate();
        }
    } else if (hover) {
        if (ui_state.mouse_down) {
            UI_WidgetActivate(widget);
        }
    }

    if (result) {
        if (*b) {
            *b = false;
        } else {
            *b = true;
        }
    }

    UI_Vec4 color;
    if (*b) {
        color = LIGHTGRAY;
    } else {
        color = GRAY;
    }
    UI_DrawRectOutline(button_rect, color);

    if (*b) {
        UI_DrawCheckMark({(float)button_rect.x, (float)button_rect.y}, {button_rect.x + (float)button_rect.width, button_rect.y + (float)button_rect.height}, DARKGRAY);
    }

    UI_DrawText(label, &ui_state.font_atlas, {button_rect.x + (float)button_rect.width, (float)button_rect.y - 2});

    ui_state.next_position = UI_Vec2(NEXT_PX, button_rect.y + button_rect.height + 20.0f);

    return result;
}

bool UI_Field(char *label, char *input, int input_length) {
    bool result = false;
    
    UI_Rect rect = {ui_state.next_position.x, ui_state.next_position.y, 400, 20};

    UI_Widget *widget = UI_FindWidget(label);
    if (widget == nullptr) {
        widget = UI_WidgetCreate(label);
    } else {
        widget = UI_WidgetCopy(widget);
    }
    UI_WidgetPush(widget);
    
    bool hover = UI_InRect(ui_state.mouse_x, ui_state.mouse_y, rect);
    if (hover && ui_state.mouse_down) {
        UI_WidgetActivate(widget);
    }

    // NOTE: View offset needed to view current position in text and clip the rest

    if (UI_IsActive(widget)) {
        if (ui_state.key_down) {
            if (ui_state.key == VK_RETURN) {
                result = true; 
            } else if (ui_state.key == VK_BACK) {
                widget->cursor--;
                widget->cursor = UI_Clamp(widget->cursor, 0, input_length - 1);
            } else {
                strncat(input, &ui_state.key, input_length);
                widget->cursor++;
                widget->cursor = UI_Clamp(widget->cursor, 0, input_length - 1);
            }
        }
        if (ui_state.mouse_down && !UI_InRect(ui_state.mouse_x, ui_state.mouse_y, rect)) {
            UI_WidgetDeactivate();
        }
    }

    float c = UI_GetTextWidthRanged(input, 0, widget->cursor, &ui_state.font_atlas) - widget->text_offset;
    if (c > rect.width) {
        widget->text_offset += (c - rect.width);
    } else if (c < widget->text_offset) {
        widget->text_offset -= (widget->text_offset - c);
    }

    float margin = 2.0f;
    UI_DrawRectOutline(rect, WHITE);

    UI_DrawTextOffset(input, &ui_state.font_atlas, {(float)rect.x + margin, (float)rect.y}, widget->text_offset);
    // draw cursor
    if (UI_IsActive(widget)) {
        UI_Vec2 cursor_position = {(float)rect.x + margin, (float)rect.y};
        cursor_position.x += UI_GetTextWidthRanged(input, 0, widget->cursor, &ui_state.font_atlas);
        cursor_position.x -= widget->text_offset;
        UI_DrawSolidRect({cursor_position.x, cursor_position.y + 1, 1, rect.height - 2}, BLACK);
    }

    ui_state.next_position = UI_Vec2(NEXT_PX, rect.y + rect.height + 20.0f);

    return result;
}

bool UI_RadioButton(char *label, int *out, int value) {
    bool result = false;
    UI_Widget *widget = UI_FindWidget(label);
    if (widget == nullptr) {
        widget = UI_WidgetCreate(label);
    } else {
        widget = UI_WidgetCopy(widget);
    }
    UI_WidgetPush(widget);

    float radio_width = 20;
    float width = UI_GetTextWidth(label, &ui_state.font_atlas) + radio_width;
    float height = 20;
    UI_Rect rect = {ui_state.next_position.x, ui_state.next_position.y, width, height};

    bool hover = UI_InRect(ui_state.mouse_x, ui_state.mouse_y, rect);

    if (UI_IsActive(widget)) {
        if (ui_state.mouse_pressed) {
            if (hover) {
                *out = value;
            }
            UI_WidgetDeactivate();
        }
    } else if (hover) {
        if (ui_state.mouse_down) UI_WidgetActivate(widget);
    }

    UI_Vec4 button_color = LIGHTGRAY;
    if (hover) {
        button_color = GRAY;
    }
    if (*out == value) {
        button_color = DARKGRAY;
    }
    UI_DrawRectOutline({rect.x, rect.y, radio_width, height}, button_color);
    UI_DrawText(label, &ui_state.font_atlas, {rect.x + radio_width, rect.y});

    ui_state.next_position = UI_Vec2(NEXT_PX, rect.y + rect.height + 20.0f);
    return result;
}
#endif
