/*
Mandelbrot explorer for native Windows (32 or 64-bit), not using any runtime libraries!

Copyright 2020 Dmitry Brant.
*/

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
// Windows Header Files
#include <windows.h>
#include <commctrl.h>

#include "resource.h"

extern "C"
{
    // required by the compiler for floating-point usage.
    int _fltused;
}

#define NUM_THREADS 12

#define WM_REPAINT_MAIN WM_USER+1

typedef struct ThreadParams {
    HWND hWnd;
    int id;
    int startRow;
    int rowCount;
} THREADPARAMS, * PTHREADPARAMS;


// Global Variables:
HINSTANCE hInst;
HWND mainWindow;

int bmpWidth;
int bmpHeight;

// Our bitmap's pixels will be addressable as ints, which is super efficient.
int* bmpBits = NULL;
int bmpSizeAllocated = 0;
BITMAPINFO bmpInfo;

int colorPaletteSize = 256;
int* colorPalette = NULL;

double xCenter = -0.5;
double yCenter = 0.0;
double xExtent = 3.0;
int numIterations = 256;

PTHREADPARAMS threadParams[NUM_THREADS];
HANDLE threads[NUM_THREADS];
bool threadTerminateFlag;

int prevMouseX, prevMouseY;
bool isMouseDown;


LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    MainDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ConfigDlgProc(HWND, UINT, WPARAM, LPARAM);

// Insert manifest for using themed Windows components.
#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")


// The actual entry point of the program, which will in turn call WinMain.
void WinMainCRTStartup()
{
    STARTUPINFO StartupInfo;
    LPWSTR lpszCommandLine = GetCommandLine();
    // TODO: do something with the command line.

    GetStartupInfo(&StartupInfo);

    int ret = WinMain(GetModuleHandle(NULL), 0, 0,
        StartupInfo.dwFlags & STARTF_USESHOWWINDOW ? StartupInfo.wShowWindow : SW_SHOWDEFAULT);
    ExitProcess(ret);
}

// Thread function that computes a chunk of the mandelbrot image.
DWORD WINAPI MandelbrotThreadProc(LPVOID lpParam) {
    int viewWidth = bmpWidth;
    int viewHeight = bmpHeight;
    int startX = 0, maxX = startX + bmpWidth;
    int startY, maxY;

    double xmin = xCenter - (xExtent / 2.0);
    double xmax = xmin + xExtent;
    double aspect = (double)viewWidth / (double)viewHeight;
    double ymin = yCenter - (xExtent / aspect / 2.0);
    double ymax = ymin + (xExtent / aspect);

    double x, y, x0, y0, x2, y2;
    double xscale = (xmax - xmin) / viewWidth;
    double yscale = (ymax - ymin) / viewHeight;
    int iteration;
    int iterScale = 1;
    int px, py, yptr;

    PTHREADPARAMS params = (PTHREADPARAMS)lpParam;
    startY = params->startRow;
    maxY = startY + params->rowCount;

    if (numIterations < colorPaletteSize) {
        iterScale = colorPaletteSize / numIterations;
    }

    for (py = startY; py < maxY; py++) {
        y0 = ymin + (double)py * yscale;
        yptr = py * viewWidth;

        for (px = startX; px < maxX; px++) {

            x = y = x2 = y2 = 0.0;
            iteration = 0;
            x0 = xmin + (double)px * xscale;
            while (x2 + y2 < 4.0) {
                y = 2.0 * x * y + y0;
                x = x2 - y2 + x0;
                x2 = x * x;
                y2 = y * y;
                if (++iteration > numIterations) {
                    break;
                }
            }

            bmpBits[yptr + px] = iteration >= numIterations ? 0 : colorPalette[(iteration * iterScale) % colorPaletteSize];
        }
        if (threadTerminateFlag) {
            break;
        }
    }
    PostMessage(params->hWnd, WM_REPAINT_MAIN, NULL, NULL);
    return 0;
}

// Function that triggers (re)creation of the bitmap and starts the threads that draw onto it.
void RecreateBitmap(HWND hwnd) {

    // Resize our bitmap to fit the size of the given window.
    RECT rect;
    GetClientRect(hwnd, &rect);
    bmpWidth = rect.right;
    bmpHeight = rect.bottom;

    if (threads[0] != INVALID_HANDLE_VALUE) {
        // terminate previous threads
        threadTerminateFlag = true;
        WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);

        for (int i = 0; i < NUM_THREADS; i++) {
            CloseHandle(threads[i]);
        }
    }

    // do we need to reallocate our buffer?
    int bmpSizeRequired = bmpWidth * bmpHeight * sizeof(int);
    if (bmpSizeRequired > bmpSizeAllocated) {
        if (bmpBits != NULL) {
            HeapFree(GetProcessHeap(), 0, bmpBits);
        }
        // allocate twice the amount of requested bytes, to reduce reallocations upon resize.
        bmpSizeAllocated = 2 * bmpSizeRequired;
        bmpBits = (int*)HeapAlloc(GetProcessHeap(), 0, bmpSizeAllocated);
    }

    // update our BITMAPINFO object, for painting the bitmap onto the window
    bmpInfo.bmiHeader.biXPelsPerMeter = 0;
    bmpInfo.bmiHeader.biYPelsPerMeter = 0;
    bmpInfo.bmiHeader.biClrUsed = 0;
    bmpInfo.bmiHeader.biClrImportant = 0;
    bmpInfo.bmiHeader.biBitCount = 32;
    bmpInfo.bmiHeader.biWidth = bmpWidth;
    bmpInfo.bmiHeader.biHeight = bmpHeight;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biSizeImage = bmpWidth * bmpHeight * 4;
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    // update our thread parameters, and let 'em rip

    threadTerminateFlag = false;

    int row = 0, rowInc = bmpHeight / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        threadParams[i]->hWnd = hwnd;
        threadParams[i]->id = i;
        threadParams[i]->startRow = row;
        threadParams[i]->rowCount = rowInc;
        row += rowInc;

        threads[i] = CreateThread(NULL, 0, MandelbrotThreadProc, threadParams[i], 0, NULL);
    }
}


int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    hInst = hInstance;

    // Initialize our color palette with some color gradients.
    colorPalette = (int*)HeapAlloc(GetProcessHeap(), 0, colorPaletteSize * sizeof(int));
    int colorLevel;
    for (int i = 0; i < 64; i++) {
        colorLevel = i * 4;
        colorPalette[i] = (colorLevel << 16) | (255 - colorLevel);
        colorPalette[i + 64] = ((255 - colorLevel) << 16) | (colorLevel << 8);
        colorPalette[i + 128] = 0xFF00 | colorLevel;
        colorPalette[i + 192] = ((255 - colorLevel) << 8) | 0xFF;
    }

    // Initialize thread params.
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = INVALID_HANDLE_VALUE;
        threadParams[i] = (PTHREADPARAMS)HeapAlloc(GetProcessHeap(), 0, sizeof(THREADPARAMS));
    }

    // Create our main dialog window and show it!
    mainWindow = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_MAIN), 0, MainDlgProc, 0);
    ShowWindow(mainWindow, nCmdShow);

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Free whatever we allocated.
    HeapFree(GetProcessHeap(), 0, bmpBits);
    HeapFree(GetProcessHeap(), 0, colorPalette);
    for (int i = 0; i < NUM_THREADS; i++) {
        HeapFree(GetProcessHeap(), 0, threadParams[i]);
    }

    return (int)msg.wParam;
}


INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    mainWindow = hDlg;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        HWND configWnd = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CONFIGBOX), hDlg, ConfigDlgProc);
        ShowWindow(configWnd, SW_SHOW);
    }
    case WM_SIZE:
        RecreateBitmap(hDlg);
        return TRUE;

    case WM_REPAINT_MAIN:
        InvalidateRect(hDlg, NULL, false);
        return TRUE;

    case WM_LBUTTONDOWN:
        isMouseDown = true;
        prevMouseX = LOWORD(lParam);
        prevMouseY = HIWORD(lParam);
        return TRUE;

    case WM_LBUTTONUP:
        isMouseDown = false;
        return TRUE;

    case WM_MOUSEMOVE:
        if (isMouseDown) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            xCenter += (xExtent / (double)bmpWidth * (double)(prevMouseX - x));
            double aspect = (double)bmpWidth / (double)bmpHeight;
            yCenter -= (xExtent / aspect / (double)bmpHeight * (double)(prevMouseY - y));
            prevMouseX = x;
            prevMouseY = y;

            InvalidateRect(hDlg, NULL, false);
            RecreateBitmap(hDlg);
        }
        return TRUE;

    case WM_MOUSEWHEEL:
    {
        POINT p;
        p.x = LOWORD(lParam);
        p.y = HIWORD(lParam);
        ScreenToClient(hDlg, &p);

        int delta = (short)HIWORD(wParam) / WHEEL_DELTA;
        double factor = delta > 0 ? 0.6666666 : 1.5;
        double aspect = (double)bmpWidth / (double)bmpHeight;

        double xmin = xCenter - xExtent / 2.0, xmax = xmin + xExtent;
        double xpos = xmin + ((double)p.x * xExtent / (double)bmpWidth);
        double ymin = yCenter - xExtent / aspect / 2.0, ymax = ymin + xExtent / aspect;
        double ypos = ymin + ((double)(bmpHeight - p.y) * xExtent / aspect / (double)bmpHeight);

        xmin = xpos - (xpos - xmin) * factor;
        xmax = xpos + (xmax - xpos) * factor;
        ymin = ypos - (ypos - ymin) * factor;
        ymax = ypos + (ymax - ypos) * factor;

        xExtent = xmax - xmin;
        xCenter = xmin + xExtent / 2.0;
        yCenter = ymin + xExtent / aspect / 2.0;

        InvalidateRect(hDlg, NULL, false);
        RecreateBitmap(hDlg);
    }
    return TRUE;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_EXIT:
        case IDOK:
        case IDCANCEL:
            DestroyWindow(hDlg);
            break;
        }
    }
    return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);

        // write the bitmap to the screen in the fastest way possible.
        SetDIBitsToDevice(hdc, 0, 0, bmpWidth, bmpHeight, 0, 0, 0, bmpHeight, bmpBits, &bmpInfo, DIB_RGB_COLORS);

        EndPaint(hDlg, &ps);
    }
    return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;

    }
    return FALSE;
}

INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // initialize slider
        HWND slider = GetDlgItem(hDlg, IDC_SLIDER_ITER);
        SendMessage(slider, TBM_SETRANGE, TRUE, MAKELONG(2, 2048));
        SendMessage(slider, TBM_SETPAGESIZE, 0, 32);
        SendMessage(slider, TBM_SETPOS, TRUE, 256);
    }
    return TRUE;

    case WM_HSCROLL:
    {
        LRESULT pos = SendMessageW(GetDlgItem(hDlg, IDC_SLIDER_ITER), TBM_GETPOS, 0, 0);
        numIterations = (int)pos;
        RecreateBitmap(mainWindow);
    }
    return TRUE;

    case WM_COMMAND:
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_EXIT:
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
    }

    return FALSE;
}
