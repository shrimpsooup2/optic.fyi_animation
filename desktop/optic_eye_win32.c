/* Optic Eye -- a small always-on-top eye that watches the cursor.
   Layered window, click-through, tray icon to move or quit it. */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <math.h>
#include <stdlib.h>
#include "eye_render.h"

#define SIZE_PX   64            /* the widget is square */
#define MARGIN    28            /* gap from the screen edge */
#define FPS_MS    16
#define TRAY_MSG  (WM_APP + 1)
#define ID_QUIT     1002
#define ID_CORNER0  1010          /* +0 TL, +1 TR, +2 BL, +3 BR */
#define TP_OUT      150           /* shrink away, ms */
#define TP_IN       320           /* pop back in, ms */
#define HK_QUIT   1
#ifndef PI
#define PI 3.14159265358979323846
#endif

static HWND      g_wnd;
static HBITMAP   g_dib;
static HDC       g_memdc;
static unsigned *g_px;
static int       g_corner = 0;      /* 0 TL, 1 TR, 2 BL, 3 BR */
static double    g_gx, g_gy;        /* pupil position, in eyeball radii */
static double    g_light = 0.0;     /* 0 dark surroundings, 1 light */
static DWORD     g_lit_at;          /* next backdrop sample */
static double    g_light_target;
static double    g_blink = 1.0;
static double    g_scale = 1.0;     /* teleport shrink / pop */
static int       g_tp_active, g_tp_moved;
static DWORD     g_tp_start;
static DWORD     g_blink_at;        /* tick when the next blink starts */
static DWORD     g_blink_start;
static NOTIFYICONDATAW g_nid;
static UINT      g_taskbar_msg;     /* explorer restarted; re-add the icon */
static int       g_hotkey;          /* 1 if Ctrl+Alt+Q is registered */

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

/* Shrink out where it is, jump, then pop back in at the new corner. */
static void goto_corner(int c)
{
    g_corner = c & 3;
    g_tp_active = 1;
    g_tp_moved = 0;
    g_tp_start = GetTickCount();
}

static double ease_out_back(double u)
{
    const double c1 = 1.70158, c3 = c1 + 1.0, k = u - 1.0;
    return 1.0 + c3 * k * k * k + c1 * k * k;   /* overshoots to about 1.10 */
}

static void paint(void)
{
    POINT cur, pos = {0, 0};
    RECT wr;
    SIZE sz = {SIZE_PX, SIZE_PX};
    BLENDFUNCTION bf;
    HDC screen;
    POINT src = {0, 0};
    double dx, dy, tgx, tgy;
    DWORD now = GetTickCount();

    GetWindowRect(g_wnd, &wr);
    GetCursorPos(&cur);
    dx = cur.x - (wr.left + SIZE_PX / 2.0);
    dy = cur.y - (wr.top + SIZE_PX / 2.0);

    /* Straight 2D: the iris and pupil slide together to wherever it is
       looking. Easing the position itself, rather than an angle and a
       distance separately, is what keeps it smooth over the centre -- in
       polar terms the angle goes wild there for no real movement. */
    eye_look(dx, dy, eye_radius(SIZE_PX), &tgx, &tgy);
    g_gx += (tgx - g_gx) * 0.18;
    g_gy += (tgy - g_gy) * 0.18;

    if (now >= g_lit_at) {
        double l = sample_light(&wr);
        if (l >= 0.0) g_light_target = l;
        g_lit_at = now + 400;
    }
    g_light += (g_light_target - g_light) * 0.02;   /* seconds, not frames */

    eye_render(g_px, SIZE_PX, g_gx, g_gy, g_blink, g_light, g_scale);

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

    if (g_tp_active) {
        DWORD el = now - g_tp_start;
        if (el < TP_OUT) {
            double t = el / (double)TP_OUT;
            g_scale = 1.0 - t * t;                 /* gathers speed on the way out */
        } else if (!g_tp_moved) {
            g_scale = 0.0;
            place();                               /* jump while nothing is drawn */
            g_tp_moved = 1;
        } else if (el < TP_OUT + TP_IN) {
            g_scale = ease_out_back((el - TP_OUT) / (double)TP_IN);
        } else {
            g_scale = 1.0;
            g_tp_active = 0;
        }
    }

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
    unsigned *p;
    unsigned char *maskbits;
    HBITMAP colour, mask;
    HDC dc = GetDC(NULL);
    ICONINFO ii;
    HICON icon;
    int i;

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = N; bi.bmiHeader.biHeight = -N;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    colour = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    {   /* the icon shows it at rest, looking up and to the right */
        double gx, gy;
        eye_look(100.0, -100.0, 1.0, &gx, &gy);
        eye_render((unsigned *)bits, N, gx, gy, 1.0, 0.0, 1.0);
    }

    /* The renderer premultiplies; CreateIconIndirect wants straight alpha. */
    p = (unsigned *)bits;
    for (i = 0; i < N * N; i++) {
        unsigned a = p[i] >> 24, ch[3], k;
        if (!a || a == 255) continue;
        for (k = 0; k < 3; k++) {
            ch[k] = ((p[i] >> (k * 8)) & 0xFF) * 255 / a;
            if (ch[k] > 255) ch[k] = 255;
        }
        p[i] = (a << 24) | (ch[2] << 16) | (ch[1] << 8) | ch[0];
    }

    /* CreateBitmap with NULL bits leaves the mask UNDEFINED. Left to chance it
       can come back all ones, which makes the icon wholly transparent -- an
       invisible tray icon, and with no taskbar button and a click-through
       window that leaves no way to quit. Hand it zeroed bits. */
    maskbits = (unsigned char *)calloc((size_t)N * N / 8, 1);
    mask = CreateBitmap(N, N, 1, 1, maskbits);
    free(maskbits);

    ii.fIcon = TRUE; ii.xHotspot = 0; ii.yHotspot = 0;
    ii.hbmMask = mask; ii.hbmColor = colour;
    icon = CreateIconIndirect(&ii);
    DeleteObject(colour); DeleteObject(mask);
    ReleaseDC(NULL, dc);
    /* Better a stock icon than none: an icon-less tray entry cannot be right-clicked. */
    return icon ? icon : LoadIcon(NULL, IDI_APPLICATION);
}

static void menu(void)
{
    POINT p;
    HMENU m = CreatePopupMenu();
    static const WCHAR *names[4] = {L"Top left", L"Top right",
                                    L"Bottom left", L"Bottom right"};
    int i;
    for (i = 0; i < 4; i++)
        AppendMenuW(m, MF_STRING | (i == g_corner ? MF_CHECKED : 0),
                    ID_CORNER0 + i, names[i]);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_QUIT,
                g_hotkey ? L"Quit\tCtrl+Alt+Q" : L"Quit");
    GetCursorPos(&p);
    SetForegroundWindow(g_wnd);          /* or the menu will not take clicks */
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_LEFTALIGN, p.x, p.y, 0, g_wnd, NULL);
    PostMessage(g_wnd, WM_NULL, 0, 0);   /* lets it dismiss on the next click */
    DestroyMenu(m);
}

static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == g_taskbar_msg && g_taskbar_msg) {   /* explorer came back */
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        return 0;
    }
    switch (msg) {
    case WM_HOTKEY:  if (w == HK_QUIT) PostQuitMessage(0); return 0;
    case WM_CLOSE:
    case WM_ENDSESSION: PostQuitMessage(0); return 0;
    case WM_TIMER:   tick(); return 0;

    /* Only the circle takes the mouse; the corners of the box stay see-through
       so clicking near it still reaches whatever is underneath. */
    case WM_NCHITTEST: {
        POINT q; double ex, ey;
        q.x = GET_X_LPARAM(l); q.y = GET_Y_LPARAM(l);
        ScreenToClient(h, &q);
        ex = q.x - SIZE_PX / 2.0; ey = q.y - SIZE_PX / 2.0;
        return (ex * ex + ey * ey <= (SIZE_PX * 0.42) * (SIZE_PX * 0.42))
               ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_RBUTTONUP: menu(); return 0;
    case WM_LBUTTONUP:                       /* a poke makes it blink */
        if (!g_blink_start) g_blink_start = GetTickCount();
        return 0;
    case TRAY_MSG:
        if (l == WM_RBUTTONUP || l == WM_LBUTTONUP) menu();
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_QUIT) PostQuitMessage(0);
        else if (LOWORD(w) >= ID_CORNER0 && LOWORD(w) < ID_CORNER0 + 4)
            goto_corner(LOWORD(w) - ID_CORNER0);
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
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
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

    /* A guaranteed way out that does not depend on the tray icon rendering. */
    g_hotkey = RegisterHotKey(g_wnd, HK_QUIT, MOD_CONTROL | MOD_ALT, 'Q') ? 1 : 0;
    if (!g_hotkey)
        g_hotkey = RegisterHotKey(g_wnd, HK_QUIT,
                                  MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'Q') ? 1 : 0;

    g_taskbar_msg = RegisterWindowMessageW(L"TaskbarCreated");

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_wnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = TRAY_MSG;
    g_nid.hIcon = make_icon();
    lstrcpyW(g_nid.szTip, L"Optic Eye");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    place();
    eye_look(100.0, -100.0, 1.0, &g_gx, &g_gy);   /* start at rest, not centred */
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
    if (g_hotkey) UnregisterHotKey(g_wnd, HK_QUIT);
    if (g_nid.hIcon) DestroyIcon(g_nid.hIcon);
    return 0;
}
