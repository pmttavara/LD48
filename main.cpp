#pragma warning(disable: 4800)
#pragma warning(disable: 4100)
#pragma warning(disable: 4505)
#pragma comment(lib, "user32.lib")
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <Windows.h>
#include <stdint.h>
#define SOKOL_ASSERT(c) assert(c)
#define SOKOL_D3D11

int assert_(const char *s) {
    int x = MessageBoxA(NULL, s, "Assert Fired", 0x2112);
    if (x == 3) ExitProcess(1);
    return x == 4;
}
#define assert_2(LINE) #LINE
#define assert_1(LINE) assert_2(LINE)
#define assert(e) ((e) || assert_("At " __FILE__ ":" assert_1(__LINE__) ":\n\n" #e "\n\nPress Retry to debug.") && (__debugbreak(), 0))
//#define D3D11_NO_HELPERS
//#define CINTERFACE
//#define COBJMACROS

#include <d3d11.h>
#include <d3dcompiler.h>

#define SOKOL_GFX_IMPL
#include "sokol_gfx.h"

#define null nullptr
#define cast(T) (T)
#define switch

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;

struct String {
    constexpr String(u8 *p, s64 n) : ptr{p}, len{n} {}
    u8 *ptr = null;
    s64 len = 0;
    void invariants() {
        assert(len >= 0);
        assert(!ptr || len);
    }
    explicit operator bool() { return ptr; }
    String &operator ++() {
        invariants();
        ++ptr;
        --len;
        return *this;
    }
    u8 &operator[](s64 i) {
        invariants();
        assert(i < len);
        return ptr[i];
    }
};
constexpr String operator "" _s(const char * p, size_t n) {
    return String{cast(u8 *) p, cast(s64) n};
}

struct Memory_Block {
    u8 *ptr = null;
    u64 len = 0;
};
enum struct Allocator_Mode {
    Allocate,
    Reallocate,
    Free,
    Free_All
    };
using Allocator_Proc = bool(void *allocator_data, Memory_Block *block, Allocator_Mode mode, u64 size);

struct Allocator {
    Allocator_Proc *proc = null;
    void *data = null;
    bool allocate(Memory_Block *block, u64 n) {
        assert(block);
        assert(proc);
        return proc(data, block, Allocator_Mode::Allocate, n);
    }
     bool reallocate(Memory_Block *block, u64 n) {
        assert(block);
        assert(proc);
        return proc(data, block, Allocator_Mode::Reallocate, n);
    }
    void free(Memory_Block *block) {
        assert(block);
        assert(proc);
        proc(data, block, Allocator_Mode::Free, 0);
    }
    void free_all() {
        assert(proc);
        proc(data, null, Allocator_Mode::Free_All, 0);
    }
};

#include <stdio.h>
auto log = [](auto &&... args) {
    printf(args...);
    puts("");
    fflush(stdout);
};

void quit() {
    ExitProcess(0);
}

intptr_t wnd_proc(HWND hwnd, unsigned int umsg, size_t wparam, intptr_t lparam) {
    if (umsg == WM_SYSCOMMAND) {
        int type = wparam & 0xfff0;
        if (type == SC_KEYMENU) {
            return 0;
        } else if (type == SC_SCREENSAVE) {
            return 0;
        } else if (type == SC_CLOSE) {
            // do nothing
        }
    } else if (umsg == WM_CLOSE) {
        quit();
    } else if (umsg == WM_DESTROY) {
        quit();
    }
    return DefWindowProcA(hwnd, umsg, wparam, lparam);
}

 double get_time(void) {
    static double invfreq;
    union _LARGE_INTEGER li;
    if (invfreq == 0) {
        QueryPerformanceFrequency(&li);
        invfreq = 1.0 / li.QuadPart;
    }
    QueryPerformanceCounter(&li);
    return li.QuadPart * invfreq;
}

static bool is_fullscreen(HWND hwnd) {
    LONG_PTR window_style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if ((window_style & WS_OVERLAPPEDWINDOW) == 0) {
        return true;
    }
    
    return false;
}

void set_fullscreen(HWND hwnd, bool fullscreen) {
    MONITORINFO mi = {sizeof(mi)};
    int gmi_result = GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    assert(gmi_result);
    
    LONG_PTR swlp_result = SetWindowLongPtrA(hwnd, GWL_STYLE, fullscreen ? 0 : WS_OVERLAPPEDWINDOW);
    assert(swlp_result);
    
    int newx = mi.rcMonitor.left;
    int newy = mi.rcMonitor.top;
    int neww = mi.rcMonitor.right - mi.rcMonitor.left;
    int newh = mi.rcMonitor.bottom - mi.rcMonitor.top;
    if (!fullscreen) {
        neww = 640;
        newh = 480;
        if (neww > (mi.rcMonitor.right - mi.rcMonitor.left)) {
            neww = (mi.rcMonitor.right - mi.rcMonitor.left);
        }
        if (newh > (mi.rcMonitor.bottom - mi.rcMonitor.top)) {
            newh = (mi.rcMonitor.bottom - mi.rcMonitor.top);
        }
        newx += (mi.rcMonitor.right - mi.rcMonitor.left)/2-neww/2;
        newy += (mi.rcMonitor.bottom - mi.rcMonitor.top)/2-newh/2;
    }
    
    BOOL swp_result = SetWindowPos(hwnd, HWND_TOP, newx, newy, neww, newh, SWP_SHOWWINDOW);
    assert(swp_result);
}


static ID3D11Device* _sapp_d3d11_device;
static ID3D11DeviceContext* _sapp_d3d11_device_context;
static DXGI_SWAP_CHAIN_DESC _sapp_dxgi_swap_chain_desc;
static IDXGISwapChain* _sapp_dxgi_swap_chain;
static ID3D11Texture2D* _sapp_d3d11_rt;
static ID3D11RenderTargetView* _sapp_d3d11_rtv;
static ID3D11Texture2D* _sapp_d3d11_ds;
static ID3D11DepthStencilView* _sapp_d3d11_dsv;
static int _sapp_sample_count;
static int _sapp_framebuffer_width;
static int _sapp_framebuffer_height;

static
int//HRESULT
//WINAPI
(*fp_D3D11CreateDeviceAndSwapChain)(IDXGIAdapter* pAdapter,
                                    D3D_DRIVER_TYPE DriverType,
                                    HMODULE Software,
                                    UINT Flags,
                                    CONST D3D_FEATURE_LEVEL* pFeatureLevels,
                                    UINT FeatureLevels,
                                    UINT SDKVersion,
                                    CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
                                    IDXGISwapChain** ppSwapChain,
                                    ID3D11Device** ppDevice,
                                    D3D_FEATURE_LEVEL* pFeatureLevel,
                                    ID3D11DeviceContext** ppImmediateContext);

static const void *d3d11_render_target_view_cb(void) {
    return _sapp_d3d11_rtv;
}
static const void *d3d11_depth_stencil_view_cb(void) {
    return _sapp_d3d11_dsv;
}
static bool _sapp_win32_update_dimensions(
                                          void *hWnd,
                                          int window_w, int window_h) {
    
    (void)hWnd;
    
    if (_sapp_framebuffer_width != window_w ||
        _sapp_framebuffer_height != window_h) {
        _sapp_framebuffer_width = window_w;
        _sapp_framebuffer_height = window_h;
        if (_sapp_framebuffer_width <= 0) {
            _sapp_framebuffer_width = 1;
        }
        if (_sapp_framebuffer_height <= 0) {
            _sapp_framebuffer_height = 1;
        }
        return true;
    }
    
    return false;
}
static void _sapp_d3d11_create_default_render_target(void) {
    HRESULT hr;
    
    hr = _sapp_dxgi_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&_sapp_d3d11_rt);
    
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp_d3d11_rt);
    hr = _sapp_d3d11_device->CreateRenderTargetView((ID3D11Resource*)_sapp_d3d11_rt, NULL, &_sapp_d3d11_rtv);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp_d3d11_rtv);
    D3D11_TEXTURE2D_DESC ds_desc;
    memset(&ds_desc, 0, sizeof(ds_desc));
    ds_desc.Width = (unsigned int)_sapp_framebuffer_width;
    ds_desc.Height = (unsigned int)_sapp_framebuffer_height;
    ds_desc.MipLevels = 1;
    ds_desc.ArraySize = 1;
    ds_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ds_desc.SampleDesc = _sapp_dxgi_swap_chain_desc.SampleDesc;
    ds_desc.Usage = D3D11_USAGE_DEFAULT;
    ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = _sapp_d3d11_device->CreateTexture2D(&ds_desc, NULL, &_sapp_d3d11_ds);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp_d3d11_ds);
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
    memset(&dsv_desc, 0, sizeof(dsv_desc));
    dsv_desc.Format = ds_desc.Format;
    dsv_desc.ViewDimension = _sapp_sample_count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = _sapp_d3d11_device->CreateDepthStencilView((ID3D11Resource*)_sapp_d3d11_ds, &dsv_desc, &_sapp_d3d11_dsv);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp_d3d11_dsv);
}

#define SAPP_SAFE_RELEASE(class, obj) do { if (obj) obj->Release(); } while (0)

static void _sapp_d3d11_destroy_default_render_target(void) {
    SAPP_SAFE_RELEASE(ID3D11Texture2D, _sapp_d3d11_rt);
    SAPP_SAFE_RELEASE(ID3D11RenderTargetView, _sapp_d3d11_rtv);
    SAPP_SAFE_RELEASE(ID3D11Texture2D, _sapp_d3d11_ds);
    SAPP_SAFE_RELEASE(ID3D11DepthStencilView, _sapp_d3d11_dsv);
}

static void _sapp_d3d11_resize_default_render_target(void) {
    if (_sapp_dxgi_swap_chain) {
        _sapp_d3d11_destroy_default_render_target();
        _sapp_dxgi_swap_chain->ResizeBuffers(1,
                                     (unsigned int)_sapp_framebuffer_width,
                                     (unsigned int)_sapp_framebuffer_height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        _sapp_d3d11_create_default_render_target();
    }
}

static void get_d3d11_device(sg_desc *desc, int framebuffer_width, int framebuffer_height, HWND hWnd) {
    
    auto d3d11_dll = (LoadLibraryA)("d3d11.dll");
    *(void **)&fp_D3D11CreateDeviceAndSwapChain = (void *)(GetProcAddress)(d3d11_dll, "D3D11CreateDeviceAndSwapChain");
    
    _sapp_framebuffer_width = framebuffer_width;
    _sapp_framebuffer_height = framebuffer_height;
    _sapp_sample_count = 4;
    
    { // @Hack @Temporary
        DXGI_SWAP_CHAIN_DESC* sc_desc = &_sapp_dxgi_swap_chain_desc;
        sc_desc->BufferDesc.Width = (unsigned int)framebuffer_width; // @Hack @Temporary
        sc_desc->BufferDesc.Height = (unsigned int)framebuffer_height; // @Hack @Temporary
        sc_desc->BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc_desc->BufferDesc.RefreshRate.Numerator = 60;
        sc_desc->BufferDesc.RefreshRate.Denominator = 1;
        sc_desc->OutputWindow = hWnd;
        sc_desc->Windowed = true;
        sc_desc->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sc_desc->BufferCount = 1;
        sc_desc->SampleDesc.Count = (unsigned int)_sapp_sample_count;
        sc_desc->SampleDesc.Quality = _sapp_sample_count > 1 ? (unsigned int)D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
        sc_desc->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc_desc->Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; 
        int create_flags = /*D3D11_CREATE_DEVICE_SINGLETHREADED | */D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(SOKOL_DEBUG)
        //create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = fp_D3D11CreateDeviceAndSwapChain(
                                                      NULL, /* pAdapter (use default) */
                                                      D3D_DRIVER_TYPE_HARDWARE,       /* DriverType */
                                                      NULL,                           /* Software */
                                                      (unsigned int)create_flags,                   /* Flags */
                                                      NULL,                           /* pFeatureLevels */
                                                      0,                              /* FeatureLevels */
                                                      D3D11_SDK_VERSION,              /* SDKVersion */
                                                      sc_desc,                        /* pSwapChainDesc */
                                                      &_sapp_dxgi_swap_chain,         /* ppSwapChain */
                                                      &_sapp_d3d11_device,            /* ppDevice */
                                                      &feature_level,                 /* pFeatureLevel */
                                                      &_sapp_d3d11_device_context);   /* ppImmediateContext */
        
        SOKOL_ASSERT(SUCCEEDED(hr) && _sapp_dxgi_swap_chain && _sapp_d3d11_device && _sapp_d3d11_device_context);
        
        IDXGIDevice * pDXGIDevice = 0;
        hr = _sapp_d3d11_device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
        SOKOL_ASSERT(SUCCEEDED(hr) && pDXGIDevice);
        //hr = IDXGIDevice1_SetMaximumFrameLatency(pDXGIDevice, 2);
        
        IDXGIAdapter * pDXGIAdapter = 0;
        hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
        SOKOL_ASSERT(SUCCEEDED(hr) && pDXGIAdapter);
        
        IDXGIFactory * pDXGIFactory = 0;
        hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pDXGIFactory);
        SOKOL_ASSERT(SUCCEEDED(hr) && pDXGIFactory);
        
        pDXGIFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
    }

    _sapp_d3d11_create_default_render_target();
    
    desc->context.d3d11.device = _sapp_d3d11_device;
    desc->context.d3d11.device_context = _sapp_d3d11_device_context;
    desc->context.d3d11.render_target_view_cb = d3d11_render_target_view_cb;
    desc->context.d3d11.depth_stencil_view_cb = d3d11_depth_stencil_view_cb;
}


int main() {
    log("Hello, world!");
    HWND hwnd = null;
    {
        auto hinstance = GetModuleHandleA(null);
        WNDCLASS wndclass = {};
        wndclass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc = &wnd_proc,
        wndclass.hInstance = hinstance,
        wndclass.lpszClassName = "wndclass",
        wndclass.hCursor = LoadCursorA(null, cast(LPCSTR) IDC_ARROW),
        RegisterClassA(&wndclass);
        //hwnd = CreateWindowExA(0, "wndclass", "LD48", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, SW_SHOW, 640, 480, null, null, hinstance, null);
        // For stream!
        hwnd = CreateWindowExA(0, "wndclass", "LD48", WS_OVERLAPPEDWINDOW | WS_VISIBLE, -1300, 300, 640, 480, null, null, hinstance, null);
    }
    set_fullscreen(hwnd, true);
    int window_w = 1;
    int window_h = 1;
    {
        RECT cr = {};
        GetClientRect(hwnd, &cr);
        window_w = cr.right - cr.left;
        window_h = cr.bottom - cr.top;
    }
    {
        sg_desc sokol_state = {};
        get_d3d11_device(&sokol_state, window_w, window_h, hwnd);
        sg_setup(&sokol_state);
    }
    
    /* default pass action (clear to grey) */
    sg_pass_action pass_action = { 0 };
    
    /* a vertex buffer with the triangle vertices */
    const float vertices[] = {
        /* positions            colors */
        0.0f, 0.5f, 0.5f,      1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };
    sg_buffer_desc vbuf_desc = {};
    vbuf_desc.data = SG_RANGE(vertices);
    sg_buffer vbuf = sg_make_buffer(vbuf_desc);
    
    /* define resource bindings */
    sg_bindings bind = {};
    bind.vertex_buffers[0] = vbuf;
    
    /* a shader to render the triangle */
    sg_shader_desc shd_desc = {};
    
    float pos[2] = {};
    shd_desc.attrs[0].sem_name = "POS";
    shd_desc.attrs[1].sem_name = "COLOR";
    shd_desc.vs.source =
        "cbuffer pos { float2 pos; }"
            "struct vs_in {\n"
            "  float4 pos: POS;\n"
            "  float4 color: COLOR;\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 color: COLOR0;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
        "  outp.pos = inp.pos;\n"
        "  outp.pos.xy += pos;\n"
            "  outp.color = inp.color;\n"
            "  return outp;\n"
        "}\n";
    shd_desc.vs.uniform_blocks[0].size = sizeof(pos);
    shd_desc.fs.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
        "}\n";
    sg_shader shd = sg_make_shader(shd_desc);
    
    /* a pipeline object */
    sg_pipeline_desc pip_desc = {};
        /* if the vertex layout doesn't have gaps, don't need to provide strides and offsets */
    pip_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
        pip_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.shader = shd;
    sg_pipeline pip = sg_make_pipeline(pip_desc);
    
#define key(vk) (cast(unsigned short) GetKeyState(vk) >= 0x8000)
#define keydown(vk) (keydown[vk])
    
    double last = get_time();
    while (true) {
        double next = get_time();
        float dt = cast(float) (next - last);
        last = next;
        enum { VK_COUNT = 256 };
        bool keydown[VK_COUNT] = {};
        MSG msg = {};
        while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
                    && (msg.lParam & 0xffff) == 1) {
                keydown[msg.wParam] = true;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        {
            RECT cr = {};
            GetClientRect(hwnd, &cr);
            window_w = cr.right - cr.left;
            window_h = cr.bottom - cr.top;
        }
        if (keydown(VK_RETURN) && key(VK_MENU)) {
            set_fullscreen(hwnd, !is_fullscreen(hwnd));
        }
        
        sg_begin_default_pass(&pass_action, window_w, window_h);
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        pos[0] += (key('D') - key('A')) * dt;
        pos[1] += (key('W') - key('S')) * dt;
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(pos));
        sg_draw(0, 3, 1);
        sg_end_pass();
        sg_commit();
        
        if (_sapp_win32_update_dimensions(hwnd, window_w, window_h)) {
            _sapp_d3d11_resize_default_render_target();
        } else {
            _sapp_dxgi_swap_chain->Present(0, 0);
        }
    }
    sg_shutdown();
    return 0;
}