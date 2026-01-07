#include "Renderer.h"
#include <windows.h>
#include <sstream>
#include <cmath>
#include <algorithm>
#include "GameLoop.h"

static const wchar_t WINDOW_CLASS_NAME[] = L"AI_Viewer_Class";
static const UINT WM_REFRESH_ENTITIES = WM_USER + 1;

// Helper to convert void* to HWND safely within this translation unit
inline HWND ToHWND(void* p) { return reinterpret_cast<HWND>(p); }

Renderer::Renderer(int width, int height)
    : width_(width), height_(height), running_(false), hwnd_(nullptr)
{
    keyState_.fill(0);
}

Renderer::~Renderer()
{
    Stop();
}

void Renderer::Start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return; // already running

    thread_ = std::thread(&Renderer::ThreadMain, this);

    // Wait until the window is created (or thread stops)
    std::unique_lock<std::mutex> lk(cv_mtx_);
    cv_.wait(lk, [this]() { return hwnd_ != nullptr || !running_.load(); });
}

void Renderer::Stop()
{
    // Idempotent stop
    bool wasRunning = running_.exchange(false);
    if (!wasRunning)
        return;

    // Post WM_QUIT to the window thread if window exists
    if (hwnd_)
    {
        PostMessage(ToHWND(hwnd_), WM_QUIT, 0, 0);
    }

    // join the thread if we started one
    if (thread_.joinable())
        thread_.join();

    // ensure hwnd_ is cleared
    hwnd_ = nullptr;
}

void Renderer::SetEntities(const std::vector<Entity>& entities)
{
    if (!running_.load())
        return;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        entities_ = entities;
    }
    if (hwnd_)
    {
        PostMessage(ToHWND(hwnd_), WM_REFRESH_ENTITIES, 0, 0);
    }
}

void Renderer::SetOverlayLines(const std::vector<std::string>& lines)
{
    if (!running_.load())
        return;
    {
        std::lock_guard<std::mutex> lk(overlayMtx_);
        overlayLines_ = lines;
    }
    if (hwnd_)
    {
        PostMessage(ToHWND(hwnd_), WM_REFRESH_ENTITIES, 0, 0);
    }
}

void Renderer::ClearOverlayLines()
{
    if (!running_.load())
        return;
    {
        std::lock_guard<std::mutex> lk(overlayMtx_);
        overlayLines_.clear();
    }
    if (hwnd_)
    {
        PostMessage(ToHWND(hwnd_), WM_REFRESH_ENTITIES, 0, 0);
    }
}

void Renderer::SetKeyState(unsigned int vk, bool down)
{
    if (vk >= keyState_.size()) return;
    std::lock_guard<std::mutex> lk(inputMtx_);
    keyState_[vk] = down ? 1 : 0;
}

bool Renderer::IsKeyDown(unsigned int vk) const
{
    if (vk >= keyState_.size()) return false;
    std::lock_guard<std::mutex> lk(inputMtx_);
    return keyState_[vk] != 0;
}

void Renderer::OnRMouseClickScreen(int sx, int sy)
{
    // Convert client px -> world coords using same scale as OnPaint
    RECT rc;
    HWND h = ToHWND(hwnd_);
    if (!h) return;
    GetClientRect(h, &rc);
    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    const float worldW = WORLD_WIDTH;
    const float worldH = WORLD_HEIGHT;
    const float scaleX = (float)clientW / worldW;
    const float scaleY = (float)clientH / worldH;
    const float scale = std::min<float>(scaleX, scaleY);

    // screen -> world
    float wx = sx / scale;
    float wy = sy / scale;

    // clamp to world bounds (safer than wrapping here)
    if (wx < 0.0f) wx = 0.0f;
    if (wx >= worldW) wx = worldW - 1.0f;
    if (wy < 0.0f) wy = 0.0f;
    if (wy >= worldH) wy = worldH - 1.0f;

    {
        std::lock_guard<std::mutex> lk(clickRMtx_);
        clickRWorldX_ = wx;
        clickRWorldY_ = wy;
        hasRClick_ = true;
    }
}

void Renderer::OnLMouseClickScreen(int sx, int sy)
{
    // Convert client px -> world coords using same scale as OnPaint
    RECT rc;
    HWND h = ToHWND(hwnd_);
    if (!h) return;
    GetClientRect(h, &rc);
    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    const float worldW = WORLD_WIDTH;
    const float worldH = WORLD_HEIGHT;
    const float scaleX = (float)clientW / worldW;
    const float scaleY = (float)clientH / worldH;
    const float scale = std::min<float>(scaleX, scaleY);

    // screen -> world
    float wx = sx / scale;
    float wy = sy / scale;

    // clamp to world bounds (safer than wrapping here)
    if (wx < 0.0f) wx = 0.0f;
    if (wx >= worldW) wx = worldW - 1.0f;
    if (wy < 0.0f) wy = 0.0f;
    if (wy >= worldH) wy = worldH - 1.0f;

    {
        std::lock_guard<std::mutex> lk(clickLMtx_);
        clickLWorldX_ = wx;
        clickLWorldY_ = wy;
        hasLClick_ = true;
    }
}

bool Renderer::FetchLClick(Vec2& out)
{
    std::lock_guard<std::mutex> lk(clickLMtx_);
    if (!hasLClick_) return false;
    out.x = clickLWorldX_;
    out.y = clickLWorldY_;
    hasLClick_ = false;
    return true;
}

bool Renderer::FetchRClick(Vec2& out)
{
    std::lock_guard<std::mutex> lk(clickRMtx_);
    if (!hasRClick_) return false;
    out.x = clickRWorldX_;
    out.y = clickRWorldY_;
    hasRClick_ = false;
    return true;
}

LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CREATE)
    {
        // store pointer to Renderer instance in GWLP_USERDATA
        auto create = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }

    if (msg == WM_REFRESH_ENTITIES)
    {
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    // retrieve renderer pointer
    Renderer* renderer = reinterpret_cast<Renderer*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!renderer)
        return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        renderer->SetKeyState(static_cast<unsigned int>(wp), true);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        renderer->SetKeyState(static_cast<unsigned int>(wp), false);
        return 0;

    case WM_LBUTTONDOWN:
    {
        int x = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int y = static_cast<int>(static_cast<short>(HIWORD(lp)));
        renderer->OnLMouseClickScreen(x, y);
        return 0;
    }
    case WM_RBUTTONDOWN:
    {
        int x = static_cast<int>(static_cast<short>(LOWORD(lp)));
        int y = static_cast<int>(static_cast<short>(HIWORD(lp)));
        renderer->OnRMouseClickScreen(x, y);
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        renderer->OnPaint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void Renderer::RegisterClassAndCreateWindow()
{
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GlobalWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // we'll paint white
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wc);

    RECT wr = { 0, 0, width_, height_ };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    // create window and pass 'this' as lpParam so WM_CREATE can store pointer
    HWND hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"AI Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    // store hwnd_ while under cv mutex then notify Start()
    {
        std::lock_guard<std::mutex> lk(cv_mtx_);
        hwnd_ = hwnd;
    }
    cv_.notify_one();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void Renderer::ThreadMain()
{
    RegisterClassAndCreateWindow();
    HWND hwnd = ToHWND(hwnd_);
    if (!hwnd)
    {
        running_ = false;
        return;
    }

    // Message loop (runs on this thread)
    MSG msg;
    while (running_ && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindowInternal();
    running_.store(false);

    // ensure Start() isn't left waiting
    cv_.notify_one();
}

void Renderer::DestroyWindowInternal()
{
    if (hwnd_)
    {
        DestroyWindow(ToHWND(hwnd_));
        hwnd_ = nullptr;
    }
}

void Renderer::OnPaint(void* hdcPtr)
{
    HDC hdc = reinterpret_cast<HDC>(hdcPtr);

    RECT rc;
    GetClientRect(ToHWND(hwnd_), &rc);
    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    // Create a compatible memory DC (offscreen buffer)
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP backBuffer = CreateCompatibleBitmap(hdc, clientW, clientH);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, backBuffer);

    // Fill white background
    HBRUSH white = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(memDC, &rc, white);

    // --- Draw everything to memDC instead of hdc ---
    std::vector<Entity> localEntities;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        localEntities = entities_;
    }

    const float worldW = WORLD_WIDTH;
    const float worldH = WORLD_HEIGHT;
    const float scaleX = (float)clientW / worldW;
    const float scaleY = (float)clientH / worldH;
    const float scale = std::min<float>(scaleX, scaleY);

    for (const Entity& ent : localEntities)
    {
        if (ent.type == Entity::Type::Line)
        {
            float wx1 = ent.x;
            float wy1 = ent.y;
            float wx2 = ent.x2;
            float wy2 = ent.y2;

            int px1 = int(wx1 * scale);
            int py1 = int(wy1 * scale);
            int px2 = int(wx2 * scale);
            int py2 = int(wy2 * scale);

            BYTE r = (ent.color >> 16) & 0xFF;
            BYTE g = (ent.color >> 8) & 0xFF;
            BYTE b = (ent.color) & 0xFF;

            HPEN pen = CreatePen(PS_SOLID, (int)ent.thickness, RGB(r, g, b));
            HPEN old = (HPEN)SelectObject(memDC, pen);

            MoveToEx(memDC, px1, py1, nullptr);
            LineTo(memDC, px2, py2);

            SelectObject(memDC, old);
            DeleteObject(pen);

            continue;
        }
        if (ent.type == Entity::Type::Circle)
        {
            float wx = ent.x;
            float wy = ent.y;
            int px = static_cast<int>(wx * scale + 0.5f);
            int py = static_cast<int>(wy * scale + 0.5f);


            uint32_t col = ent.color;
            BYTE r = (col >> 16) & 0xFF;
            BYTE g = (col >> 8) & 0xFF;
            BYTE b = col & 0xFF;

            int radius = std::max<int>(1, static_cast<int>(ent.radius * scale));

            HPEN pen;
            if (!ent.filled)
                pen = CreatePen(PS_SOLID, max(1, int(ent.thickness)), RGB(r, g, b));
            else
                pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

            HPEN oldPen = (HPEN)SelectObject(memDC, pen);


            HBRUSH brush;
            if (ent.filled)
                brush = CreateSolidBrush(RGB(r, g, b));
            else
                brush = (HBRUSH)GetStockObject(NULL_BRUSH);

            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, brush);

            Ellipse(memDC, px - radius, py - radius, px + radius, py + radius);

            SelectObject(memDC, oldPen);
            SelectObject(memDC, oldBrush);

            DeleteObject(pen);
            if (ent.filled) DeleteObject(brush);


            // draw name if present
            if (!ent.name.empty() && shouldDrawNames_)
            {
                std::wstring wname;
                std::ostringstream oss;
                oss << ent.name;
                std::string s = oss.str();
                int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
                if (len > 0)
                {
                    wname.resize(len - 1);
                    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wname[0], len);
                    TextOutW(memDC, px + radius + 2, py - radius, wname.c_str(), (int)wname.size());
                }
            }

            // draw a small direction line if direction is non-zero
            {
                float dx = ent.dirX;
                float dy = ent.dirY;
                const float eps = 1e-6f;
                if (std::fabs(dx) > eps || std::fabs(dy) > eps)
                {
                    // normalize direction
                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len > eps)
                    {
                        float nx = dx / len;
                        float ny = dy / len;

                        // length of the indicator in world units (tweakable)
                        const float indicatorWorldLen = 28.0f;
                        int lineLenPx = static_cast<int>(indicatorWorldLen * scale + 0.5f);

                        int px2 = px + static_cast<int>(nx * lineLenPx + 0.5f);
                        int py2 = py + static_cast<int>(ny * lineLenPx + 0.5f);

                        // Create a black pen
                        HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
                        HPEN oldPen = (HPEN)SelectObject(memDC, pen);
                        MoveToEx(memDC, px, py, nullptr);
                        LineTo(memDC, px2, py2);
                        SelectObject(memDC, oldPen);
                        DeleteObject(pen);
                    }
                }
            }
        }
        if (ent.type == Entity::Type::Rectangle)
        {
            float left = ent.x;
            float bottom = ent.y;
            float right = ent.x2;
            float top = ent.y2;

            int pleft = int(left * scale);
            int pbottom = int(bottom * scale);
            int pright = int(right * scale);
            int ptop = int(top * scale);


            uint32_t col = ent.color;
            BYTE r = (col >> 16) & 0xFF;
            BYTE g = (col >> 8) & 0xFF;
            BYTE b = col & 0xFF;

            HPEN pen;
            if (!ent.filled)
                pen = CreatePen(PS_SOLID, max(1, int(ent.thickness)), RGB(r, g, b));
            else
                pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));

            HPEN oldPen = (HPEN)SelectObject(memDC, pen);

            HBRUSH brush;
            if (ent.filled)
                brush = CreateSolidBrush(RGB(r, g, b));
            else
                brush = (HBRUSH)GetStockObject(NULL_BRUSH);

            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, brush);

            Rectangle(memDC, pleft, ptop, pright, pbottom);

            SelectObject(memDC, oldPen);
            SelectObject(memDC, oldBrush);

            DeleteObject(pen);
            if (ent.filled) DeleteObject(brush);

            if (!ent.name.empty())
            {
                // set transparent background for text
                int oldBkMode = SetBkMode(memDC, TRANSPARENT);
                COLORREF oldColor = SetTextColor(memDC, RGB(0,0,0));

                const std::string& s = ent.name;

                int len = MultiByteToWideChar(CP_UTF8,0, s.c_str(), -1, nullptr,0);
                if (len <=0) continue;

                std::wstring w;
                w.resize(len -1);
                MultiByteToWideChar(CP_UTF8,0, s.c_str(), -1, &w[0], len);

                // compute rectangle center in pixel coordinates
                float centerX = (left + right) *0.5f;
                float centerY = (top + bottom) *0.5f;
                int centerPx = static_cast<int>(centerX * scale +0.5f);
                int centerPy = static_cast<int>(centerY * scale +0.5f);

                // measure text in pixels to center it
                SIZE ext;
                GetTextExtentPoint32W(memDC, w.c_str(), (int)w.size(), &ext);

                int textX = centerPx - (ext.cx /2);
                int textY = centerPy - (ext.cy /2);

                TextOutW(memDC, textX, textY, w.c_str(), (int)w.size());

                SetTextColor(memDC, oldColor);
                SetBkMode(memDC, oldBkMode);
            }
        }
    }

    {
        std::vector<std::string> overlay;
        {
            std::lock_guard<std::mutex> lk(overlayMtx_);
            overlay = overlayLines_;
        }

        if (!overlay.empty())
        {
            // set transparent background for text
            int oldBkMode = SetBkMode(memDC, TRANSPARENT);
            COLORREF oldColor = SetTextColor(memDC, RGB(0, 0, 0));

            // measure font height
            TEXTMETRICW tm;
            GetTextMetricsW(memDC, &tm);
            int lineHeight = tm.tmHeight;
            const int padding = 8;
            const int lineSpacing = 2;

            for (size_t i = 0; i < overlay.size(); ++i)
            {
                const std::string& s = overlay[i];
                if (s.empty()) continue;

                int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
                if (len <= 0) continue;
                std::wstring w;
                w.resize(len - 1);
                MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);

                SIZE ext;
                GetTextExtentPoint32W(memDC, w.c_str(), (int)w.size(), &ext);

                int x = clientW - padding - ext.cx;
                int y = padding + static_cast<int>(i) * (lineHeight + lineSpacing);

                TextOutW(memDC, x, y, w.c_str(), (int)w.size());
            }

            SetTextColor(memDC, oldColor);
            SetBkMode(memDC, oldBkMode);
        }
    }

    // --- Copy finished frame to window ---
    BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(backBuffer);
    DeleteDC(memDC);
}

bool Renderer::IsRunning() const
{
    return running_.load();
}