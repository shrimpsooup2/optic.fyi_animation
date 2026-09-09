/* Optic Eye -- a small always-on-top eye that watches the cursor.
   Layered window, click-through, tray icon to move or quit it. */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <math.h>
#include <stdlib.h>
#include "eye_render.h"

#define SIZE_PX   88            /* the widget is square */
#define MARGIN    28            /* gap from the screen edge */
#define FPS_MS    16
#define TRAY_MSG  (WM_APP + 1)
#define ID_CORNER 1001
#define ID_QUIT   1002
#ifndef PI
#define PI 3.14159265358979323846
#endif

static HWND      g_wnd;
static HBITMAP   g_dib;
static HDC       g_memdc;
static unsigned *g_px;
static int       g_corner = 0;      /* 0 TL, 1 TR, 2 BL, 3 BR */
static double    g_phi;             /* current gaze angle, smoothed */
static double    g_gaze = 1.0;      /* 1 pupil out at rest, 0 drawn inward */
static double    g_light = 0.0;     /* 0 dark surroundings, 1 light */
static DWORD     g_lit_at;          /* next backdrop sample */
static double    g_light_target;
static double    g_blink = 1.0;
static DWORD     g_blink_at;        /* tick when the next blink starts */
static DWORD     g_blink_start;
static NOTIFYICONDATAW g_nid;

static void schedule_blink(void)
{
    g_blink_at = GetTickCount() + 2600 + (DWORD)(rand() % 4200);
}

/* Average brightness of a ring just outside the widget. Sampling around
   ourselves rather than underneath avoids reading back our own pixels. */
static double sample_light(const RECT *wr)
{
    HDC dc = GetDC(NULL);
    double cx = (wr->left + wr->right) * 0.5;
    double cy = (wr->top + wr->bottom) * 0.5;
    double rad = SIZE_PX * 0.5 + 16.0, sum = 0.0;
    int i, n = 0;
    for (i = 0; i < 12; i++) {
        double a = i * (2.0 * PI / 12.0);
        COLORREF c = GetPixel(dc, (int)(cx + rad * cos(a)),
                                  (int)(cy + rad * sin(a)));
        if (c == CLR_INVALID) continue;          /* off-screen */
        sum += 0.299 * GetRValue(c) + 0.587 * GetGValue(c) + 0.114 * GetBValue(c);
        n++;
    }
    ReleaseDC(NULL, dc);
    if (!n) return -1.0;
    {   /* smoothstep across the middle of the range so mid greys settle */
        double t = (sum / n - 70.0) / 80.0;
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
        return t * t * (3.0 - 2.0 * t);
    }
}

static void place(void)
{
    RECT wa;
    int x, y;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    x = (g_corner & 1) ? wa.right - SIZE_PX - MARGIN : wa.left + MARGIN;
    y = (g_corner & 2) ? wa.bottom - SIZE_PX - MARGIN : wa.top + MARGIN;
    SetWindowPos(g_wnd, HWND_TOPMOST, x, y, SIZE_PX, SIZE_PX, SWP_NOACTIVATE);
}

static void paint(void)
{
    POINT cur, pos = {0, 0};
    RECT wr;
    SIZE sz = {SIZE_PX, SIZE_PX};
    BLENDFUNCTION bf;
    HDC screen;
    POINT src = {0, 0};
    double dx, dy, dist, target, delta, want;
    DWORD now = GetTickCount();

    GetWindowRect(g_wnd, &wr);
    GetCursorPos(&cur);
    dx = cur.x - (wr.left + SIZE_PX / 2.0);
    dy = cur.y - (wr.top + SIZE_PX / 2.0);
    dist = sqrt(dx * dx + dy * dy);

    /* Only the last few pixels are ignored, or the angle spins on top of us. */
    if (dist > 8.0) {
        target = eye_angle_to(dx, dy);
        delta = target - g_phi;
        while (delta >  PI) delta -= 2 * PI;      /* turn the short way round */
        while (delta < -PI) delta += 2 * PI;
        g_phi += delta * 0.16;
    }

    /* Close up, the pupil draws in towards the centre instead of staying out
       on its orbit -- an eye focusing on something right in front of it. */
    want = dist / (eye_radius(SIZE_PX) * 2.0);
    if (want > 1.0) want = 1.0;
    g_gaze += (want - g_gaze) * 0.15;

    if (now >= g_lit_at) {
        double l = sample_light(&wr);
        if (l >= 0.0) g_light_target = l;
        g_lit_at = now + 400;
    }
    g_light += (g_light_target - g_light) * 0.02;   /* seconds, not frames */

    eye_render(g_px, SIZE_PX, g_phi, g_blink, g_gaze, g_light);

    pos.x = wr.left; pos.y = wr.top;
    bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255; bf.AlphaFormat = AC_SRC_ALPHA;
    screen = GetDC(NULL);
    UpdateLayeredWindow(g_wnd, screen, &pos, &sz, g_memdc, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screen);
}

static void tick(void)
{
    DWORD now = GetTickCount();
    if (g_blink_start) {
        double t = (now - g_blink_start) / 170.0;      /* one blink, 170ms */
        if (t >= 1.0) { g_blink = 1.0; g_blink_start = 0; schedule_blink(); }
        else g_blink = 1.0 - 0.97 * sin(PI * t);       /* shut and open again */
    } else if (now >= g_blink_at) {
        g_blink_start = now;
    }
    paint();
}

/* Build the tray icon out of the same renderer. */
static HICON make_icon(void)
{
    const int N = 32;
    BITMAPINFO bi;
    void *bits;
    HBITMAP colour, mask;
    HDC dc = GetDC(NULL);
    ICONINFO ii;
    HICON icon;

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = N; bi.bmiHeader.biHeight = -N;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    colour = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    eye_render((unsigned *)bits, N, 0.0, 1.0, 1.0, 0.0);
    mask = CreateBitmap(N, N, 1, 1, NULL);

    ii.fIcon = TRUE; ii.xHotspot = 0; ii.yHotspot = 0;
    ii.hbmMask = mask; ii.hbmColor = colour;
    icon = CreateIconIndirect(&ii);
    DeleteObject(colour); DeleteObject(mask);
    ReleaseDC(NULL, dc);
    return icon;
}

static void menu(void)
{
    POINT p;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_CORNER, L"Move to next corner");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_QUIT, L"Quit");
    GetCursorPos(&p);
    SetForegroundWindow(g_wnd);          /* so the menu dismisses properly */
    TrackPopupMenu(m, TPM_RIGHTBUTTON, p.x, p.y, 0, g_wnd, NULL);
    DestroyMenu(m);
}

static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg) {
    case WM_TIMER:   tick(); return 0;
    case TRAY_MSG:
        if (l == WM_RBUTTONUP || l == WM_LBUTTONUP) menu();
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_QUIT) { PostQuitMessage(0); }
        else if (LOWORD(w) == ID_CORNER) { g_corner = (g_corner + 1) & 3; place(); }
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE: place(); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSEXW wc;
    MSG msg;
    BITMAPINFO bi;
    HDC screen;
    HANDLE once;

    (void)prev; (void)cmd; (void)show;

    /* One eye is enough. */
    once = CreateMutexW(NULL, TRUE, L"OpticEyeSingleInstance");
    if (once && GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    SetProcessDPIAware();
    srand((unsigned)GetTickCount());

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"OpticEyeClass";
    RegisterClassExW(&wc);

    g_wnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"OpticEyeClass", L"Optic Eye", WS_POPUP,
        0, 0, SIZE_PX, SIZE_PX, NULL, NULL, inst, NULL);
    if (!g_wnd) return 1;

    screen = GetDC(NULL);
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = SIZE_PX; bi.bmiHeader.biHeight = -SIZE_PX;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    g_dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&g_px, NULL, 0);
    g_memdc = CreateCompatibleDC(screen);
    SelectObject(g_memdc, g_dib);
    ReleaseDC(NULL, screen);

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_wnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = TRAY_MSG;
    g_nid.hIcon = make_icon();
    lstrcpyW(g_nid.szTip, L"Optic Eye");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    place();
    schedule_blink();
    {   /* start already adapted, so it does not fade in from the wrong shade */
        RECT wr; double l;
        GetWindowRect(g_wnd, &wr);
        l = sample_light(&wr);
        g_light = g_light_target = (l >= 0.0 ? l : 0.0);
    }
    ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
    paint();
    SetTimer(g_wnd, 1, FPS_MS, NULL);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    return 0;
}
