// dear imgui: Renderer Backend for SDL_Renderer - PS3 Compatible Version
// Uses software surface rendering for SDL < 2.0.17 compatibility

#include "imgui.h"
#include "imgui_impl_sdlrenderer.h"
#include <stdint.h>
#include <SDL.h>

// SDL_Renderer data
struct ImGui_ImplSDLRenderer_Data
{
    SDL_Renderer*   SDLRenderer;
    SDL_Texture*    FontTexture;
    SDL_Surface*    FontSurface;
    ImGui_ImplSDLRenderer_Data() { memset((void*)this, 0, sizeof(*this)); }
};

static ImGui_ImplSDLRenderer_Data* ImGui_ImplSDLRenderer_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplSDLRenderer_Data*)ImGui::GetIO().BackendRendererUserData : NULL;
}

// Helper: software rasterize a triangle
#define SWAP_VAL(a, b) do { int _tmp = a; a = b; b = _tmp; } while(0)
#define SWAP_FLT(a, b) do { float _tmp = a; a = b; b = _tmp; } while(0)
#define SWAP_U32(a, b) do { ImU32 _tmp = a; a = b; b = _tmp; } while(0)

static void ImGui_ImplSDLRenderer_RasterizeTri(SDL_Surface* surf, int x0, int y0, int x1, int y1, int x2, int y2,
    ImU32 col0, ImU32 col1, ImU32 col2,
    SDL_Surface* tex_surf, float u0, float v0, float u1, float v1, float u2, float v2)
{
    // Sort vertices by Y
    if (y0 > y1) { SWAP_VAL(x0, x1); SWAP_VAL(y0, y1); SWAP_U32(col0, col1); SWAP_FLT(u0, u1); SWAP_FLT(v0, v1); }
    if (y0 > y2) { SWAP_VAL(x0, x2); SWAP_VAL(y0, y2); SWAP_U32(col0, col2); SWAP_FLT(u0, u2); SWAP_FLT(v0, v2); }
    if (y1 > y2) { SWAP_VAL(x1, x2); SWAP_VAL(y1, y2); SWAP_U32(col1, col2); SWAP_FLT(u1, u2); SWAP_FLT(v1, v2); }

    int dy = y2 - y0;
    if (dy == 0) return;

    int w = surf->w, h = surf->h;
    bool lock = SDL_MUSTLOCK(surf);
    if (lock) SDL_LockSurface(surf);

    for (int y = y0; y <= y2; y++)
    {
        if (y < 0 || y >= h) continue;
        // Find x range on this scanline
        int x_left, x_right;
        ImU32 col_left, col_right;
        float u_left, v_left, u_right, v_right;

        if (y < y1)
        {
            int seg = y1 - y0;
            if (seg == 0) continue;
            x_left = x0 + (x1 - x0) * (y - y0) / seg;
            col_left = col0;
            float t = (float)(y - y0) / seg;
            u_left = u0 + (u1 - u0) * t;
            v_left = v0 + (v1 - v0) * t;
        }
        else
        {
            int seg = y2 - y1;
            if (seg == 0) continue;
            x_left = x1 + (x2 - x1) * (y - y1) / seg;
            col_left = col1;
            float t = (float)(y - y1) / seg;
            u_left = u1 + (u2 - u1) * t;
            v_left = v1 + (v2 - v1) * t;
        }

        {
            int seg = y2 - y0;
            x_right = x0 + (x2 - x0) * (y - y0) / seg;
            float t = (float)(y - y0) / seg;
            u_right = u0 + (u2 - u0) * t;
            v_right = v0 + (v2 - v0) * t;
            col_right = col0;
        }

        if (x_left > x_right) { SWAP_VAL(x_left, x_right); SWAP_U32(col_left, col_right); SWAP_FLT(u_left, u_right); SWAP_FLT(v_left, v_right); }
        if (x_left < 0) x_left = 0;
        if (x_right >= w) x_right = w - 1;

        uint32_t* pixels = (uint32_t*)surf->pixels + y * surf->pitch / 4;
        uint32_t* tex_pixels = tex_surf ? (uint32_t*)tex_surf->pixels : NULL;
        int tex_pitch = tex_surf ? tex_surf->pitch / 4 : 0;
        int tex_w = tex_surf ? tex_surf->w : 1;
        int tex_h = tex_surf ? tex_surf->h : 1;

        for (int x = x_left; x <= x_right; x++)
        {
            float t = (x_right > x_left) ? (float)(x - x_left) / (x_right - x_left) : 0;
            float u = u_left + (u_right - u_left) * t;
            float v = v_left + (v_right - v_left) * t;

            ImU32 color = col_left;
            if (tex_pixels)
            {
                int tx = (int)(u * tex_w) % tex_w;
                int ty = (int)(v * tex_h) % tex_h;
                if (tx < 0) tx = 0; if (ty < 0) ty = 0;
                ImU32 texel = tex_pixels[ty * tex_pitch + tx];
                // Alpha blend
                unsigned char sa = (texel >> IM_COL32_A_SHIFT) & 0xFF;
                unsigned char da = (color >> IM_COL32_A_SHIFT) & 0xFF;
                if (sa == 255) {
                    color = texel;
                } else if (sa > 0) {
                    unsigned char sr = (texel >> IM_COL32_R_SHIFT) & 0xFF;
                    unsigned char sg = (texel >> IM_COL32_G_SHIFT) & 0xFF;
                    unsigned char sb = (texel >> IM_COL32_B_SHIFT) & 0xFF;
                    unsigned char dr = (color >> IM_COL32_R_SHIFT) & 0xFF;
                    unsigned char dg = (color >> IM_COL32_G_SHIFT) & 0xFF;
                    unsigned char db = (color >> IM_COL32_B_SHIFT) & 0xFF;
                    unsigned char a = sa * da / 255;
                    unsigned char ia = 255 - sa;
                    color = IM_COL32(
                        (sr * sa + dr * ia) / 255,
                        (sg * sa + dg * ia) / 255,
                        (sb * sa + db * ia) / 255,
                        (sa * da + 255 - da) / 255
                    );
                }
            }

            unsigned char a = (color >> IM_COL32_A_SHIFT) & 0xFF;
            if (a > 0)
            {
                uint32_t src = color;
                uint32_t dst = pixels[x];
                unsigned char sr = (src >> IM_COL32_R_SHIFT) & 0xFF;
                unsigned char sg = (src >> IM_COL32_G_SHIFT) & 0xFF;
                unsigned char sb = (src >> IM_COL32_B_SHIFT) & 0xFF;
                unsigned char dr = (dst >> IM_COL32_R_SHIFT) & 0xFF;
                unsigned char dg = (dst >> IM_COL32_G_SHIFT) & 0xFF;
                unsigned char db = (dst >> IM_COL32_B_SHIFT) & 0xFF;
                unsigned char da2 = (dst >> IM_COL32_A_SHIFT) & 0xFF;
                unsigned char ia = 255 - a;
                pixels[x] = IM_COL32(
                    (sr * a + dr * ia) / 255,
                    (sg * a + dg * ia) / 255,
                    (sb * a + db * ia) / 255,
                    (a + da2 * ia / 255)
                );
            }
        }
    }
    if (lock) SDL_UnlockSurface(surf);
}

bool ImGui_ImplSDLRenderer_Init(SDL_Renderer* renderer)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");
    IM_ASSERT(renderer != NULL && "SDL_Renderer not initialized!");

    ImGui_ImplSDLRenderer_Data* bd = IM_NEW(ImGui_ImplSDLRenderer_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_sdlrenderer_ps3";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    bd->SDLRenderer = renderer;
    return true;
}

void ImGui_ImplSDLRenderer_Shutdown()
{
    ImGui_ImplSDLRenderer_Data* bd = ImGui_ImplSDLRenderer_GetBackendData();
    IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplSDLRenderer_DestroyDeviceObjects();
    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;
    IM_DELETE(bd);
}

void ImGui_ImplSDLRenderer_NewFrame()
{
    ImGui_ImplSDLRenderer_Data* bd = ImGui_ImplSDLRenderer_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplSDLRenderer_Init()?");
    if (!bd->FontTexture)
        ImGui_ImplSDLRenderer_CreateDeviceObjects();
}

void ImGui_ImplSDLRenderer_RenderDrawData(ImDrawData* draw_data)
{
    ImGui_ImplSDLRenderer_Data* bd = ImGui_ImplSDLRenderer_GetBackendData();

    float rsx = 1.0f, rsy = 1.0f;
    SDL_RenderGetScale(bd->SDLRenderer, &rsx, &rsy);
    ImVec2 render_scale;
    render_scale.x = (rsx == 1.0f) ? draw_data->FramebufferScale.x : 1.0f;
    render_scale.y = (rsy == 1.0f) ? draw_data->FramebufferScale.y : 1.0f;

    int fb_width = (int)(draw_data->DisplaySize.x * render_scale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * render_scale.y);
    if (fb_width <= 0 || fb_height <= 0) return;

    // Create a temporary surface for software rendering
    SDL_Surface* surf = SDL_CreateRGBSurface(0, fb_width, fb_height, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!surf) return;

    ImVec2 clip_off = draw_data->DisplayPos;
    ImVec2 clip_scale = render_scale;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    continue;
                else
                    pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }

            // Software rasterize each triangle
            SDL_Texture* tex = (SDL_Texture*)pcmd->GetTexID();
            SDL_Surface* tex_surf = NULL;
            if (tex)
            {
                // Get texture info - for font texture, use stored surface
                if (tex == bd->FontTexture)
                    tex_surf = bd->FontSurface;
            }

            int idx_count = pcmd->ElemCount;
            for (int i = 0; i + 3 <= idx_count; i += 3)
            {
                int idx0 = idx_buffer[pcmd->IdxOffset + i + 0];
                int idx1 = idx_buffer[pcmd->IdxOffset + i + 1];
                int idx2 = idx_buffer[pcmd->IdxOffset + i + 2];
                const ImDrawVert& v0 = vtx_buffer[pcmd->VtxOffset + idx0];
                const ImDrawVert& v1 = vtx_buffer[pcmd->VtxOffset + idx1];
                const ImDrawVert& v2 = vtx_buffer[pcmd->VtxOffset + idx2];

                ImGui_ImplSDLRenderer_RasterizeTri(surf,
                    (int)(v0.pos.x * render_scale.x), (int)(v0.pos.y * render_scale.y),
                    (int)(v1.pos.x * render_scale.x), (int)(v1.pos.y * render_scale.y),
                    (int)(v2.pos.x * render_scale.x), (int)(v2.pos.y * render_scale.y),
                    v0.col, v1.col, v2.col,
                    tex_surf, v0.uv.x, v0.uv.y, v1.uv.x, v1.uv.y, v2.uv.x, v2.uv.y);
            }
        }
    }

    // Blit surface to screen via texture
    SDL_Texture* fb_tex = SDL_CreateTextureFromSurface(bd->SDLRenderer, surf);
    if (fb_tex)
    {
        SDL_RenderCopy(bd->SDLRenderer, fb_tex, NULL, NULL);
        SDL_DestroyTexture(fb_tex);
    }
    SDL_FreeSurface(surf);
}

bool ImGui_ImplSDLRenderer_CreateFontsTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplSDLRenderer_Data* bd = ImGui_ImplSDLRenderer_GetBackendData();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    bd->FontTexture = SDL_CreateTexture(bd->SDLRenderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STATIC, width, height);
    if (!bd->FontTexture)
        return false;

    SDL_UpdateTexture(bd->FontTexture, NULL, pixels, 4 * width);
    SDL_SetTextureBlendMode(bd->FontTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(bd->FontTexture, SDL_ScaleModeNearest);

    // Keep a surface copy for software rendering fallback
    // Use format-based creation for endian-safe pixel layout (RGBA byte order)
    bd->FontSurface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, width, height, 32, 4 * width,
        SDL_PIXELFORMAT_RGBA32);
    if (!bd->FontSurface)
    {
        // Fallback: create an empty surface and copy pixel data manually
        bd->FontSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
        if (!bd->FontSurface)
            return false;
        SDL_LockSurface(bd->FontSurface);
        memcpy(bd->FontSurface->pixels, pixels, 4 * width * height);
        SDL_UnlockSurface(bd->FontSurface);
    }

    io.Fonts->SetTexID((ImTextureID)(intptr_t)bd->FontTexture);
    return true;
}

void ImGui_ImplSDLRenderer_DestroyFontsTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplSDLRenderer_Data* bd = ImGui_ImplSDLRenderer_GetBackendData();
    if (bd->FontTexture)
    {
        io.Fonts->SetTexID(0);
        SDL_DestroyTexture(bd->FontTexture);
        bd->FontTexture = NULL;
    }
    if (bd->FontSurface)
    {
        SDL_FreeSurface(bd->FontSurface);
        bd->FontSurface = NULL;
    }
}

bool ImGui_ImplSDLRenderer_CreateDeviceObjects()
{
    return ImGui_ImplSDLRenderer_CreateFontsTexture();
}

void ImGui_ImplSDLRenderer_DestroyDeviceObjects()
{
    ImGui_ImplSDLRenderer_DestroyFontsTexture();
}
