#include "common.hpp"

#define SOKOL_ASSERT(c) assert(c)
#define SOKOL_D3D11
#include <d3d11.h>
#include <d3dcompiler.h>

#define SOKOL_GFX_IMPL
#include "sokol_gfx.h"

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

struct V2 {
    f32 x = 0;
    f32 y = 0;
    V2() = default;
    V2(f32 x, f32 y) : x{x}, y{y} {}
    f32 dot(V2 rhs) {
        return x * rhs.x + y * rhs.y;
    }
    f32 magsq() {
        return x * x + y * y;
    }
    f32 mag() {
        return sqrtf(magsq());
    }
    void operator+=(V2 v) {
        x += v.x;
        y += v.y;
    }
    V2 operator+(V2 v) {
        return {x + v.x, y + v.y};
    }
    void operator-=(V2 v) {
        x -= v.x;
        y -= v.y;
    }
    V2 operator-(V2 v) {
        return {x - v.x, y - v.y};
    }
    void operator*=(f32 f) {
        x *= f;
        y *= f;
    }
    V2 operator*(f32 f) {
        return {x * f, y * f};
    }
    void operator/=(f32 f) {
        x /= f;
        y /= f;
    }
    V2 operator/(f32 f) {
        return {x / f, y / f};
    }
    V2 operator-() {
        return {-x, -y};
    }
    V2 hat() {
        V2 v = *this;
        f32 m = magsq();
        if (m) {
            m = 1 / sqrtf(m);
        }
        v.x *= m;
        v.y *= m;
        return v;
    }
};
V2 operator*(f32 f, V2 v) {
    return v * f;
}
#define v2(...) V2{__VA_ARGS__}

#define EntityTypeEnum(Name) EntityType_##Name,
#define EntityMember(Name) Name as_##Name;
#define EntityMethod(Name) \
Name &_##Name() { \
assert(type == EntityType_##Name); \
return *cast(Name *) this; \
} \
explicit operator Name &() { return _##Name(); } \
explicit operator Name *() { return &_##Name(); }
#define EntityCtor(Name) \
Entity(const Name &param_##Name) { \
assert(param_##Name.type == EntityType_##Name); \
type = EntityType_##Name; \
_##Name() = param_##Name; \
} \
Entity(Name &&param_##Name) { \
assert(param_##Name.type == EntityType_##Name); \
_##Name() = param_##Name; \
type = EntityType_##Name; \
}

#define EntitySub(Name) \
Name() { type = EntityType_##Name; } \
operator Entity *(); \
operator Entity &();

#define EntitySubImpl(Name) \
Name::operator Entity *() { return cast(Entity *) this; } \
Name::operator Entity &() { return *operator Entity*(); }

#define EntityList(X) \
X(Guy) \
X(Bullet) \
X(Enemy) \

enum EntityType : u8 {
    EntityType_Invalid,
    EntityList(EntityTypeEnum)
        
        EntityType_Count
};
struct EntityBase {
    EntityType type = EntityType_Invalid;
    bool scheduled_for_destruction = false;
    V2 pos = {};
    V2 vel = {};
};
struct Entity;
struct Guy : EntityBase {
    EntitySub(Guy);
    V2 aim_direction = {};
};
struct Enemy : EntityBase {
    EntitySub(Enemy);
};
struct Bullet : EntityBase {
    EntitySub(Bullet);
};
struct Entity : EntityBase {
    Entity() = default;
    EntityList(EntityMethod);
    EntityList(EntityCtor);
    union EntityUnion { EntityList(EntityMember); };
    char padding[sizeof(EntityUnion) - sizeof(EntityBase)];
};
EntityList(EntitySubImpl);

struct shd_Vs_Uniform {
    V2 pos;
    f32 theta;
    f32 scale;
    V2 camera_pos;
    f32 camera_scale;
};
shd_Vs_Uniform shd_vs_uniform;
#include "assets.h"

const f32 G = 6.67430e-11f;
const f32 c = 299792458.0f;
const f32 L = 100;
const f32 desired_schwarzschild_radius = 1000000000.0f;//149597900000.0f;
const f32 M = c * c * desired_schwarzschild_radius / 2 / G;
const f32 schwarzschild_radius = 2 * G * M / (c * c);

float inv_lorentz(V2 v) {
    return sqrtf(1 - v.magsq() / (c * c));
}
float lorentz(V2 v) {
    return 1 / inv_lorentz(v);
}
V2 add_vel(V2 u, V2 v) { // returns u'
    float first = 1 / (1 + v.dot(u) / c / c);
    float alpha_v = lorentz(v);
    //V2 u_along_v = v * v.dot(u) / v.magsq();
    V2 u_along_v = v.hat() * v.hat().dot(u);
    return first * (alpha_v * u + v + (1 - alpha_v) * u_along_v);
}

V2 accel_from_force(V2 F, V2 v) {
    return inv_lorentz(v) * (F - v.dot(F) * v / c / c);
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
        hwnd = CreateWindowExA(0, "wndclass", "Ringularity", WS_OVERLAPPEDWINDOW,
                               //CW_USEDEFAULT, SW_SHOW,
                               -1300, 300, // For stream!
                               640, 480, null, null, hinstance, null);
    }
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
    
    sg_pass_action clear_action = { 0 };
    clear_action.colors[0].action = SG_ACTION_CLEAR;
    clear_action.colors[0].value = {0,0,0,1};
    sg_pass_action load_action = { 0 };
    load_action.colors[0].action = SG_ACTION_LOAD;
    
    /* a vertex buffer with the triangle vertices */
    const f32 vertices[] = {
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
    
    shd_desc.attrs[0].sem_name = "POS";
    shd_desc.attrs[1].sem_name = "COLOR";
    shd_desc.vs.source = shd_h;
    shd_desc.vs.entry = "vsmain";
    shd_desc.vs.uniform_blocks[0].size = sizeof(shd_Vs_Uniform);
    shd_desc.fs.source = shd_h;
    shd_desc.fs.entry = "fsmain";
    sg_shader shd = sg_make_shader(shd_desc);
    
    /* a pipeline object */
    sg_pipeline_desc pip_desc = {};
    /* if the vertex layout doesn't have gaps, don't need to provide strides and offsets */
    pip_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.shader = shd;
    sg_pipeline pip = sg_make_pipeline(pip_desc);
    
#define key(vk) (GetFocus() == hwnd && cast(unsigned short) GetKeyState(vk) >= 0x8000)
#define keydown(vk) (GetFocus() == hwnd && keydown[vk])
    
    set_fullscreen(hwnd, true);
    
    log("%g", schwarzschild_radius);
    shd_vs_uniform.camera_scale = 0.5f / (schwarzschild_radius * 3);
    
    // Newtonian orbital velocity.
    auto get_orbital_velocity = [&](V2 pos) -> V2 {
        f32 r = pos.mag();
        return sqrtf(G * M / r) * v2(-pos.y, pos.x).hat();
    };
    restart:;
    Array<Entity> entities = {};
    defer {
        log("Releasing!");
        entities.release();
    };
    if (1) {
        Guy guy = {};
        guy.pos = {schwarzschild_radius * 4, 0};
        guy.vel = get_orbital_velocity(guy.pos);
        guy.aim_direction = {};
        entities.push(guy);
    }
    {
         Enemy e = {};
        e.pos = {schwarzschild_radius * 4 + 1000000, 0};
        e.vel = get_orbital_velocity(e.pos);
        entities.push(e);
    }
    {
        Bullet bullet = {};
        bullet.pos = {-schwarzschild_radius * 4, 0};
        bullet.vel = get_orbital_velocity(bullet.pos) * 0.75f;
        entities.push(bullet);
    }
    {
        Bullet bullet = {};
        bullet.pos = {-schwarzschild_radius * 4, 0};
        bullet.vel = get_orbital_velocity(bullet.pos);
        entities.push(bullet);
    }
    {
        Bullet bullet = {};
        bullet.pos = {0, -schwarzschild_radius * 4};
        bullet.vel = get_orbital_velocity(bullet.pos);
        entities.push(bullet);
    }
    {
        Bullet bullet = {};
        bullet.pos = {0, +schwarzschild_radius * 4};
        bullet.vel = get_orbital_velocity(bullet.pos);
        entities.push(bullet);
    }
    {
        Bullet bullet = {};
        bullet.pos = {-schwarzschild_radius * 4, schwarzschild_radius * 4};
        bullet.vel = get_orbital_velocity(bullet.pos);
        entities.push(bullet);
    }
    
    double last = get_time();
    const f32 dt = 1.0f / 120;
    const f32 timescale = 4.0f;
    while (true) {
        while (get_time() - last < dt);
        enum { VK_COUNT = 256 };
        bool keydown[VK_COUNT] = {};
        MSG msg = {};
        while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
                && (msg.lParam & 0xffff) == 1) {
                keydown[msg.wParam] = true;
            } else if (msg.message == WM_LBUTTONDOWN) {
                keydown[VK_LBUTTON] = true;
            } else if (msg.message == WM_RBUTTONDOWN) {
                keydown[VK_RBUTTON] = true;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (keydown('R')) {
            goto restart;
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
        
        V2 mouse_pos = {};
        {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
             mouse_pos = {cast(f32) p.x / window_w, cast(f32) -p.y / window_h};
            mouse_pos = mouse_pos*2 + v2(-1, +1);
            mouse_pos = mouse_pos / shd_vs_uniform.camera_scale + shd_vs_uniform.camera_pos;
        }
        
        auto dilation_general = [&](Entity &e) {
            f32 r = e.pos.mag();
            f32 factor = 1;
            if (r > schwarzschild_radius) {
                factor = sqrtf(1.00001f - 2 * G * M / r / c / c);
            } else {
                //factor = sqrtf(2 * G * M / r / c / c - 1.00f);
                factor = 0.01f;
            }
            return factor;
        };
        auto dilation_special = [&](Entity &e) {
            return inv_lorentz(e.vel);
        };
        f32 guy_general = 1;
        f32 guy_special = 1;
        V2 guy_pos = {};
        V2 guy_vel = {};
        {
            auto find_guy = [&]() -> Guy * {
                for (auto &e : entities) {
                    if (e.type == EntityType_Guy) {
                        return cast(Guy *) &e;
                    }
                }
                return null;
            };
            Guy *guy = find_guy();
            if (guy) { guy_general = dilation_general(*guy); guy_special = dilation_special(*guy); }
            if (guy) { guy_pos = guy->pos; guy_vel = guy->vel; }
        }
        //log("largest dt %g", largest_dt);
        for (s64 i = 0; i < entities.count; i += 1) {
            Entity &e = entities[i];
            
            f32 dt_ = dt * timescale;
            f32 general = dilation_general(e) / guy_general;
            f32 special = dilation_special(e) / guy_special;
            auto clamp_c = [&] {
                if (e.vel.mag() > c * 0.999f) {
                    e.vel *= c * 0.999f / e.vel.mag();
                    assert(e.vel.mag() <= c * 0.9991f);
                }
            };
            // @Temporary
            if (e.type == EntityType_Guy) {
                Guy &g = e._Guy();
                f32 speed = c / 100;
                V2 input = {
                    (f32)(key('D') - key('A') + key(VK_RIGHT) - key(VK_LEFT)),
                    (f32)(key('W') - key('S') + key(VK_UP) - key(VK_DOWN)),
                };
                V2 force = input.hat() * speed;
                V2 accel = accel_from_force(force, e.vel);
                e.vel = add_vel(e.vel, accel * dt_ * general * special);
                
                g.aim_direction = (mouse_pos - e.pos).hat();
            }
            clamp_c();
            
            if (e.pos.magsq() < sq(schwarzschild_radius * 0.01f)) { e.scheduled_for_destruction = true; continue; }
            {
                 f32 r2 = e.pos.magsq();
                f32 r3 = r2 * e.pos.mag();
                f32 r4 = r2 * r2;
                V2 force = (-e.pos.hat() * (G * M / r2 - L * L / r3 + 3 * G * M * L * L / (c * c * r4)));
                force *= lorentz(e.vel);
                V2 accel = accel_from_force(force, e.vel);
                e.vel = add_vel(e.vel, accel * dt_ * general * special);
            }
            clamp_c();
            if (e.pos.magsq() < schwarzschild_radius * schwarzschild_radius) {
                // Fudge velocities so they point down
                e.vel -= e.vel.hat() * max(0, e.pos.hat().dot(e.vel));
                e.pos *= min(1, (0.9999f * schwarzschild_radius) / e.pos.mag());
            }
            clamp_c();
            e.pos += e.vel * dt_ * general * special;
        }
        for (s64 i = 0; i < entities.count;) {
            if (entities[i].scheduled_for_destruction) {
                entities.remove(i);
            } else {
                i += 1;
            }
        }
        
        if (key('I')) {
            shd_vs_uniform.camera_scale *= powf(2.0f, dt);
        }
        if (key('K')) {
            shd_vs_uniform.camera_scale *= powf(0.5f, dt);
        }
        
        static int skipped = 0;
        skipped += 1;
        //if (skipped % 10 != 0) continue;
        //sg_begin_default_pass(key('R') ? clear_action : load_action, window_w, window_h);
        sg_begin_default_pass(clear_action, window_w, window_h);
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        
        bool draw_inside = false;
        if (guy_pos.mag() < schwarzschild_radius) draw_inside = true;
        shd_vs_uniform.camera_pos = {};
          shd_vs_uniform.camera_pos = guy_pos;
        //if (guy) shd_vs_uniform.camera_scale = 0.6f / clamp(guy_pos.mag(), 0, schwarzschild_radius * 2);
        for (auto &e : entities) {
            if (!draw_inside && e.pos.mag() < schwarzschild_radius) continue;
            shd_vs_uniform.pos = e.pos;
            shd_vs_uniform.theta = 0;
            shd_vs_uniform.scale = 0.02f / shd_vs_uniform.camera_scale;
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(shd_vs_uniform));
            sg_draw(0, 3, 1);
            if (e.type == EntityType_Guy) {
                shd_vs_uniform.pos = e.pos + e._Guy().aim_direction * 100000000;
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(shd_vs_uniform));
                sg_draw(0, 3, 1);
            }
        }
        {
            
            shd_vs_uniform.pos = {};
            shd_vs_uniform.scale = 0.02f / shd_vs_uniform.camera_scale;
            if (draw_inside) {
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(shd_vs_uniform));
                sg_draw(0, 3, 1);
            }
            
            const int N = 128;
            for (int i = 0; i < N; i += 1) {
                shd_vs_uniform.pos = v2(cosf(cast(f32) i / N * 3.14159f * 2), sinf(cast(f32) i / N * 3.14159f * 2)) * schwarzschild_radius;
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(shd_vs_uniform));
                sg_draw(0, 3, 1);
            }
        }
        
        {
            shd_vs_uniform.pos = mouse_pos;
            shd_vs_uniform.scale = 0.02f / shd_vs_uniform.camera_scale;
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(shd_vs_uniform));
            sg_draw(0, 3, 1);
        }
        
        sg_end_pass();
        sg_commit();
        
        ShowWindow(hwnd, true);
        if (_sapp_win32_update_dimensions(hwnd, window_w, window_h)) {
            _sapp_d3d11_resize_default_render_target();
        } else {
            _sapp_dxgi_swap_chain->Present(0, 0);
        }
        Sleep(cast(DWORD)(1000 * dt));
    }
    sg_shutdown();
    return 0;
}
