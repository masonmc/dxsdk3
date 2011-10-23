/*==========================================================================
 *
 *  Copyright (C) 1995, 1996 Microsoft Corporation. All Rights Reserved.
 *
 *  File: viewer.cpp
 *
 ***************************************************************************/

#define INITGUID
#include <d3drmwin.h>
#include "viewer.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <direct.h>
#include "sel.h"

static char ViewerClass[32] = "ViewerClass";
static BOOL Render(void);
HRESULT loadTextures(char *name, void *arg, LPDIRECT3DRMTEXTURE *tex);

typedef struct _AppInfo
{
    LPDIRECT3DRMFRAME scene, camera;
    LPDIRECT3DRMDEVICE dev;
    LPDIRECT3DRMVIEWPORT view;
    D3DRMCOLORMODEL model;
    BOOL bMinimized;
} AppInfo;

AppInfo *active_window = NULL;

LPDIRECT3DRM lpD3DRM = 0;
LPDIRECTDRAWCLIPPER lpDDClipper = 0;
BOOL bQuit = FALSE;

static BOOL FirstInstance(HINSTANCE);
static HWND AnyInstance(HINSTANCE this_inst, int cmdshow);
long FAR PASCAL WindowProc(HWND, UINT, WPARAM, LPARAM);
static BOOL CreateDevice(HWND, AppInfo*);
static void Idle();
char* MyErrorToString(HRESULT error);

/* Msg
 * Message output for error notification.
 */
void __cdecl
Msg( LPSTR fmt, ... )
{
    char buff[256];

    wvsprintf(buff, fmt, (char *)(&fmt+1));
    lstrcat(buff, "\r\n");
    MessageBox( NULL, buff, "Viewer Message", MB_OK );
}


char* LSTRRCHR( const char* lpString, int bChar )
{
    if( lpString != NULL )
    {
        const char*     lpBegin;

        lpBegin = lpString;

        while( *lpString != 0 )
        {
            lpString++;
        }

        while( 1 )
        {
            if( *lpString == bChar )
            {
                return (char*)lpString;
            }
            
            if( lpString == lpBegin )
            {
                break;
            }

            lpString--;
        }
    }

    return NULL;
} /* LSTRRCHR */

/*
 * Initialization, message loop
 */
int PASCAL WinMain
    (HINSTANCE this_inst, HINSTANCE prev_inst, LPSTR cmdline, int cmdshow)
{
    MSG         msg;
    int         idle;
    int         done = FALSE;
    HACCEL      accel;
    HRESULT     rval;
    HWND        hwnd;

    prev_inst = prev_inst;
    cmdline = cmdline;

    rval = Direct3DRMCreate(&lpD3DRM);
    if (rval != D3DRM_OK) {
        Msg("Failed to create Direct3DRM.\n%s", MyErrorToString(rval));
        return 1;
    }
    if (!prev_inst)
        if (!FirstInstance(this_inst))
            return 1;

    if (!(hwnd = AnyInstance(this_inst, cmdshow)))
        return 1;
    accel = LoadAccelerators(this_inst, "ViewerAccel");

    while (!done) {
        idle = TRUE;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            idle = FALSE;
            if (msg.message == WM_QUIT || bQuit) {
                done = TRUE;
                break;
            }
            if (!TranslateAccelerator(msg.hwnd, accel, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        if (done) {
            break;
        } else if (!active_window->bMinimized && !bQuit) {
            if (idle) Idle();
            if (!Render()) {
                Msg("Rendering failed.\n");
                done = TRUE;
                break;
            }
        } else {
            WaitMessage();
        }
    }
    RELEASE(active_window->scene);
    RELEASE(active_window->camera);
    RELEASE(active_window->view);
    RELEASE(active_window->dev);
    ClearClipboard();
    RELEASE(lpD3DRM);
    RELEASE(lpDDClipper);
    DestroyWindow(hwnd);
    return msg.wParam;
}

/*
 * Register window class for the application, and do any other
 * application initialization
 */
static BOOL FirstInstance(HINSTANCE this_inst)
{
    WNDCLASS    wc;
    BOOL        rc;

    /*
     * set up and register window class
     */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(DWORD);
    wc.hInstance = this_inst;
    wc.hIcon = LoadIcon(this_inst, "ViewerIcon");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = "ViewerMenu";
    wc.lpszClassName = ViewerClass;
    rc = RegisterClass(&wc);

    return rc;
}

/*
 * Do work required for every instance of the application:
 * create the window, initialize data
 */
static HWND AnyInstance(HINSTANCE this_inst, int cmdshow)
{
    HWND win;
    AppInfo *info;

    /*
     * create main window
     */
    win =
        CreateWindow
        (   ViewerClass,                /* class */
            "Direct3D Object Viewer",/* caption */
            WS_OVERLAPPEDWINDOW,        /* style */
            CW_USEDEFAULT,              /* init. x pos */
            CW_USEDEFAULT,              /* init. y pos */
            400,                        /* init. x size */
            400,                        /* init. y size */
            NULL,                       /* parent window */
            NULL,                       /* menu handle */
            this_inst,                  /* program handle */
            NULL                        /* create parms */
        );


    if (!win) return FALSE;

    /*
     * Create a clipper for this window
     */
    if (FAILED(DirectDrawCreateClipper(0, &lpDDClipper, NULL))) {
        return FALSE;
    }
    if (FAILED(lpDDClipper->SetHWnd(0, win))) {
        RELEASE(lpDDClipper);
        return FALSE;
    }
    /*
     * set up data associated with this window
     */
    info = (AppInfo*) malloc(sizeof(AppInfo));
    SetWindowLong(win, 0, (long) info);
    info->model = D3DCOLOR_MONO;
    if (!CreateDevice(win, info)) {
        return FALSE;
    }

    /*
     * display window
     */
    ShowWindow(win, cmdshow);
    UpdateWindow(win);

    return win;
}

/*
 * Processes messages for the about dialog.
 */
BOOL FAR PASCAL AboutDlgProc
    (HWND win, unsigned msg, WORD wparam, LONG lparam)
{
    lparam = lparam;

    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (wparam == IDOK)
        {   EndDialog(win, TRUE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/*
 * Create a simple scene.
 */
static BOOL CreateScene(AppInfo* info)
{
    LPDIRECT3DRMFRAME frame = NULL;
    LPDIRECT3DRMFRAME light = NULL;
    LPDIRECT3DRMMESHBUILDER builder = NULL;
    LPDIRECT3DRMLIGHT light1 = NULL;
    LPDIRECT3DRMLIGHT light2 = NULL;
    LPDIRECT3DRMMATERIAL mat = NULL;
    HRESULT rval;

    if (FAILED(lpD3DRM->CreateFrame(NULL, &info->scene)))
        goto generic_error;
    if (FAILED(lpD3DRM->CreateLightRGB(D3DRMLIGHT_DIRECTIONAL, D3DVAL(1.0), D3DVAL(1.0), D3DVAL(1.0), &light1)))
        goto generic_error;
    if (FAILED(lpD3DRM->CreateLightRGB(D3DRMLIGHT_AMBIENT, D3DVAL(0.1), D3DVAL(0.1), D3DVAL(0.1), &light2)))
        goto generic_error;

    if (FAILED(lpD3DRM->CreateFrame(info->scene, &light)))
        goto generic_error;
    if (FAILED(light->SetPosition(info->scene, D3DVAL(2.0), D3DVAL(2.0), D3DVAL(5.0))))
        goto generic_error;
    if (FAILED(light->SetOrientation(info->scene, D3DVAL(-1.0), D3DVAL(-1.0), D3DVAL(1.0), D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0))))
        goto generic_error;
    if (FAILED(light->AddLight(light1)))
        goto generic_error;
    RELEASE(light1);
    if (FAILED(info->scene->AddLight(light2)))
        goto generic_error;
    RELEASE(light2);
    if (FAILED(lpD3DRM->CreateMeshBuilder(&builder)))
        goto generic_error;
    rval = builder->Load("camera.x", NULL, D3DRMLOAD_FROMFILE, NULL, NULL);
    if (rval != D3DRM_OK) {
        Msg("Failed to load camera.x.\n%s", MyErrorToString(rval));
        goto ret_with_error;
    }
    if (FAILED(builder->SetQuality(D3DRMRENDER_UNLITFLAT)))
        goto generic_error;
    if (FAILED(light->AddVisual(builder)))
        goto generic_error;
    RELEASE(builder);
    RELEASE(light);

    if (FAILED(lpD3DRM->CreateFrame(info->scene, &frame)))
        goto generic_error;
    if (FAILED(frame->SetRotation(info->scene, D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0), D3DVAL(-0.02))))
        goto generic_error;
    if (FAILED(frame->SetPosition(info->scene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(10.0))))
        goto generic_error;
    if (FAILED(lpD3DRM->CreateMeshBuilder(&builder)))
        goto generic_error;
    rval = builder->Load("mslogo.x", NULL, D3DRMLOAD_FROMFILE,
        loadTextures, NULL);
    if (rval != D3DRM_OK) {
        Msg("Failed to load mslogo.x.\n%s", MyErrorToString(rval));
        goto ret_with_error;
    }
    if (FAILED(builder->SetColorRGB(D3DVAL(0.8), D3DVAL(0.8), D3DVAL(0.8))))
        goto generic_error;
    if (FAILED(lpD3DRM->CreateMaterial(D3DVAL(10.0), &mat)))
        goto generic_error;
    if (FAILED(builder->SetMaterial(mat)))
        goto generic_error;

    RELEASE(mat);
    if (FAILED(frame->AddVisual(builder)))
        goto generic_error;
    RELEASE(builder);
    RELEASE(frame);

    if (FAILED(lpD3DRM->CreateFrame(info->scene, &info->camera)))
        goto generic_error;
    if (FAILED(info->camera->SetPosition(info->scene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(0.0))))
        goto generic_error;
    return TRUE;
generic_error:
    Msg("A failure occurred while creating the scene.\n");
ret_with_error:
    RELEASE(frame);
    RELEASE(light);
    RELEASE(builder);
    RELEASE(light1);
    RELEASE(light2);
    RELEASE(mat);
    return FALSE;
}

static DWORD bppToddbd(int bpp)
{
    switch(bpp) {
    case 1:
        return DDBD_1;
    case 2:
        return DDBD_2;
    case 4:
        return DDBD_4;
    case 8:
        return DDBD_8;
    case 16:
        return DDBD_16;
    case 24:
        return DDBD_24;
    case 32:
        return DDBD_32;
    }
    return 0;
}


/*
 * Find a device, preferably hardware, for a particular color model.
 */
LPGUID
FindDevice(D3DCOLORMODEL cm)
{
    LPDIRECTDRAW lpDD;
    LPDIRECT3D lpD3D;
    D3DFINDDEVICESEARCH search;
    static D3DFINDDEVICERESULT result;
    HRESULT error;
    HDC hdc;
    int bpp;

    hdc = GetDC(NULL);
    bpp = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(NULL, hdc);

    if (DirectDrawCreate(NULL, &lpDD, NULL))
        return NULL;

    if (lpDD->QueryInterface(IID_IDirect3D, (void**) &lpD3D)) {
        lpDD->Release();
        return NULL;
    }
    
    memset(&search, 0, sizeof search);
    search.dwSize = sizeof search;
    search.dwFlags = D3DFDS_COLORMODEL;
    search.dcmColorModel = (cm == D3DCOLOR_MONO) ? D3DCOLOR_MONO : D3DCOLOR_RGB;

    memset(&result, 0, sizeof result);
    result.dwSize = sizeof result;

    error = lpD3D->FindDevice(&search, &result);

    if (error == DD_OK) {
        /*
         * If the device found is hardware but cannot support the current
         * bit depth, then fall back to software rendering.
         */
        if (result.ddHwDesc.dwFlags
            && !(result.ddHwDesc.dwDeviceRenderBitDepth & bppToddbd(bpp))) {
            
            search.dwFlags |= D3DFDS_HARDWARE;
            search.bHardware = FALSE;
            memset(&result, 0, sizeof result);
            result.dwSize = sizeof result;
            error = lpD3D->FindDevice(&search, &result);
        }
    }

    lpD3D->Release();
    lpDD->Release();

    if (error)
        return NULL;
    else
        return &result.guid;
}

/*
 * Create the device and viewport.
 */
static BOOL CreateDevice(HWND win, AppInfo* info)
{
    RECT r;
    int bpp;
    HDC hdc;

    GetClientRect(win, &r);
    if (FAILED(lpD3DRM->CreateDeviceFromClipper(lpDDClipper, FindDevice(info->model),
                                r.right, r.bottom, &info->dev)))
                                goto generic_error;
    hdc = GetDC(win);
    bpp = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(win, hdc);
    switch (bpp)
    {
    case 1:
        if (FAILED(info->dev->SetShades(4)))
            goto generic_error;
        if (FAILED(lpD3DRM->SetDefaultTextureShades(4)))
            goto generic_error;
        break;
    case 16:
        if (FAILED(info->dev->SetShades(32)))
            goto generic_error;
        if (FAILED(lpD3DRM->SetDefaultTextureColors(64)))
            goto generic_error;
        if (FAILED(lpD3DRM->SetDefaultTextureShades(32)))
            goto generic_error;
        if (FAILED(info->dev->SetDither(FALSE)))
            goto generic_error;
        break;
    case 24:
    case 32:
        if (FAILED(info->dev->SetShades(256)))
            goto generic_error;
        if (FAILED(lpD3DRM->SetDefaultTextureColors(64)))
            goto generic_error;
        if (FAILED(lpD3DRM->SetDefaultTextureShades(256)))
            goto generic_error;
        if (FAILED(info->dev->SetDither(FALSE)))
            goto generic_error;
        break;
    default:
        if (FAILED(info->dev->SetDither(FALSE)))
            goto generic_error;
    }
    if (!CreateScene(info))
        goto ret_with_error;
    if (FAILED(lpD3DRM->CreateViewport(info->dev, info->camera, 0, 0,
        info->dev->GetWidth(),
        info->dev->GetHeight(), &info->view)))
        goto generic_error;
    if (FAILED(info->view->SetBack(D3DVAL(5000.0))))
        goto generic_error;

    return TRUE;
generic_error:
    Msg("An error occurred while creating the device.\n");
ret_with_error:
    return FALSE;
}

/*
 * Regenerate the device if the color model changes or the window size
 * changes.
 */
static BOOL RebuildDevice(HWND win, AppInfo* info, int width, int height)
{
    HRESULT rval;
    int old_dither = info->dev->GetDither();
    D3DRMRENDERQUALITY old_quality = info->dev->GetQuality();
    int old_shades = info->dev->GetShades();

    RELEASE(info->view);
    RELEASE(info->dev);
    rval = lpD3DRM->CreateDeviceFromClipper(lpDDClipper, FindDevice(info->model),
                                width, height, &info->dev);
    if (rval != D3DRM_OK) {
        Msg("Creating a device from HWND failed while rebuilding device.\n%s", MyErrorToString(rval));
        return FALSE;
    }

    if (FAILED(info->dev->SetDither(old_dither)))
        goto generic_error;
    if (FAILED(info->dev->SetQuality(old_quality)))
        goto generic_error;
    if (FAILED(info->dev->SetShades(old_shades)))
        goto generic_error;
    width = info->dev->GetWidth();
    height = info->dev->GetHeight();
    if (FAILED(lpD3DRM->CreateViewport(info->dev, info->camera,
                          0, 0, width, height, &info->view)))
                          goto generic_error;
    if (FAILED(info->view->SetBack(D3DVAL(400.0))))
        goto generic_error;
    return TRUE;
generic_error:
    Msg("A failure occurred while rebuilding the device.\n");
    return FALSE;
}

/*
 * Resize the viewport and device when the window size changes.
 */
static BOOL ResizeViewport(HWND win, AppInfo* info, int width, int height)
{
    int view_width = info->view->GetWidth();
    int view_height = info->view->GetHeight();
    int dev_width = info->dev->GetWidth();
    int dev_height = info->dev->GetHeight();

    if (view_width == width && view_height == height)
        return TRUE;
    
    if (width <= dev_width && height <= dev_height) {
        RELEASE(info->view);
        if (FAILED(lpD3DRM->CreateViewport(info->dev, info->camera, 0, 0, width, height, &info->view)))
            goto generic_error;
        if (FAILED(info->view->SetBack(D3DVAL(400.0))))
            goto generic_error;
    }

    if (!RebuildDevice(win, info, width, height))
        return FALSE;
    return TRUE;
generic_error:
    Msg("A failure occurred while resizing the viewport.\n");
    return FALSE;
}

/*
 * Place an object in front of the camera.
 */
static BOOL PlaceMesh(LPDIRECT3DRMMESHBUILDER mesh, AppInfo *info)
{
    LPDIRECT3DRMFRAME frame;

    if (FAILED(lpD3DRM->CreateFrame(info->scene, &frame)))
        return FALSE;
    if (FAILED(frame->AddVisual(mesh)))
        return FALSE;
    if (FAILED(frame->SetPosition(info->camera, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(15.0))))
        return FALSE;
    frame->Release();
    return TRUE;
}

static BOOL ScaleScene(LPDIRECT3DRMFRAME frame, AppInfo *info)
{
  /* Some trickery form the RL2 viewer to scale a scene down to
     managable proportions */
  LPDIRECT3DRMMESHBUILDER mbuilder;
  D3DRMBOX box;
  D3DVALUE maxDim;

  lpD3DRM->CreateMeshBuilder(&mbuilder);
  mbuilder->AddFrame(frame);
  mbuilder->GetBox(&box);
  mbuilder->Release();

  maxDim = box.max.x - box.min.x;
  if (box.max.y - box.min.y > maxDim) 
    maxDim = box.max.y - box.min.y;
  if (box.max.z - box.min.z > maxDim)
    maxDim = box.max.z - box.min.z;

  frame->AddScale(D3DRMCOMBINE_BEFORE, D3DDivide(D3DVAL(8.0), maxDim),
                  D3DDivide(D3DVAL(8.0), maxDim),
                  D3DDivide(D3DVAL(8.0), maxDim));

  frame->SetPosition(info->scene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(15.0));

  return TRUE;
}
                                                

typedef struct {
    LPDIRECT3DRMANIMATIONSET animset;
    D3DVALUE time;
} animationCallbackArgs;

static void CDECL animationCallback(LPDIRECT3DRMFRAME obj, void* arg, D3DVALUE delta)
{
    animationCallbackArgs* cb = (animationCallbackArgs *) arg;

    obj = obj;
    cb->animset->SetTime(cb->time);
    cb->time += delta;
}

static BOOL setAnimationCallback(LPDIRECT3DRMFRAME frame, 
                                 LPDIRECT3DRMANIMATIONSET animset)
{
    animationCallbackArgs *cb;

    cb = (animationCallbackArgs*)malloc(sizeof(animationCallbackArgs));
    if (!cb)
        return FALSE;

    cb->animset = animset;
    cb->time = D3DVAL(0);
    if (FAILED(frame->AddMoveCallback(animationCallback, (void *) cb)))
        return FALSE;
    return TRUE;
}

static BOOL LoadAnimationSet(const char *filename, AppInfo *info)
{
  LPDIRECT3DRMANIMATIONSET lpAnimSet;
  LPDIRECT3DRMFRAME lpFrame;

  /* Create a new parent frame for the animation, load it, and set up the
     callback */
  
  if (FAILED(lpD3DRM->CreateFrame(info->scene, &lpFrame))) 
    return FALSE;

  if (FAILED(lpD3DRM->CreateAnimationSet(&lpAnimSet))) 
    return FALSE;

  if (FAILED(lpAnimSet->Load((LPVOID)filename, NULL, 
                             D3DRMLOAD_FROMFILE, loadTextures,
                             NULL, lpFrame)))
    return FALSE;

  ScaleScene(lpFrame, info);

  setAnimationCallback(lpFrame, lpAnimSet);

  return TRUE;
}
  
static BOOL LoadFrameHierarchy(const char *filename, AppInfo *info)
{
  LPDIRECT3DRMFRAME lpFrame;
  
  if (FAILED(lpD3DRM->CreateFrame(info->scene, &lpFrame)))
    return FALSE;

  if (FAILED(lpFrame->Load((LPVOID)filename, NULL, D3DRMLOAD_FROMFILE,
                           loadTextures, NULL)))
    return FALSE;

  ScaleScene(lpFrame, info);

  return TRUE;
}


static int
    left_drag = FALSE, right_drag = FALSE,
    last_x, last_y;

/*
 * Render the scene into the viewport.
 */
static BOOL Render() {
    if (FAILED(active_window->scene->Move(D3DVAL(1.0))))
        return FALSE;
    if (FAILED(active_window->view->Clear()))
        return FALSE;
    if (FAILED(active_window->view->Render(active_window->scene)))
        return FALSE;
    if (FAILED(active_window->dev->Update()))
        return FALSE;
    return TRUE;
}

static void Idle()
{
    LPDIRECT3DRMFRAME selected = SelectedFrame();

    if (active_window == NULL) return;
    if (left_drag && selected)
        selected->SetRotation(active_window->scene, D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0), D3DVAL(0.0));
    if (right_drag && selected)
        selected->SetVelocity(active_window->scene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(0.0), FALSE);
}

static int FillModeToMenuItem(D3DRMFILLMODE fm)
{
    switch (fm) {
    case D3DRMFILL_POINTS:
        return 2;

    case D3DRMFILL_WIREFRAME:
        return 3;

    case D3DRMFILL_SOLID:
        return 4;
    }
    return -1;
}

static int ShadeModeToMenuItem(D3DRMSHADEMODE sm)
{
    switch (sm) {
    case D3DRMSHADE_FLAT:
        return 6;

    case D3DRMSHADE_GOURAUD:
        return 7;

    case D3DRMSHADE_PHONG:
        return 8;
    }
    return -1;
}

static BOOL ToggleLighting(HWND win, AppInfo* info)
{
    HMENU menu;
    D3DRMRENDERQUALITY quality = info->dev->GetQuality();
    D3DRMLIGHTMODE mode = (D3DRMLIGHTMODE)(quality & D3DRMLIGHT_MASK);
    HRESULT rval;

    if (mode == D3DRMLIGHT_ON)
        mode = D3DRMLIGHT_OFF;
    else
        mode = D3DRMLIGHT_ON;

    menu = GetMenu(win);
    menu = GetSubMenu(menu, 2);
    CheckMenuItem(menu, 0, (MF_BYPOSITION
                            | (mode == D3DRMLIGHT_ON
                               ? MF_CHECKED
                               : MF_UNCHECKED)));

    quality = (quality & ~D3DRMLIGHT_MASK) | mode;
    rval = info->dev->SetQuality(quality);
    if (rval != D3DRM_OK) {
        Msg("Setting the render quality while toggling lighting failed.\n%s", MyErrorToString(rval));
        return FALSE;
    }
    return TRUE;
}

static BOOL SetFillMode(HWND win, AppInfo* info, D3DRMFILLMODE fm)
{
    HMENU menu;
    D3DRMRENDERQUALITY quality = info->dev->GetQuality();
    D3DRMFILLMODE oldfm = (D3DRMFILLMODE)(quality & D3DRMFILL_MASK);
    HRESULT rval;

    menu = GetMenu(win);
    menu = GetSubMenu(menu, 2);
    CheckMenuItem(menu, FillModeToMenuItem(oldfm),
                  MF_BYPOSITION|MF_UNCHECKED);
    CheckMenuItem(menu, FillModeToMenuItem(fm),
                  MF_BYPOSITION|MF_CHECKED);

    quality = (quality & ~D3DRMFILL_MASK) | fm;
    rval = info->dev->SetQuality(quality);
    if (rval != D3DRM_OK) {
        Msg("Setting the render quality while changing the fill mode failed.\n%s", MyErrorToString(rval));
        return FALSE;
    }
    return TRUE;
}

static BOOL SetShadeMode(HWND win, AppInfo* info, D3DRMSHADEMODE sm)
{
    HMENU menu;
    D3DRMRENDERQUALITY quality = info->dev->GetQuality();
    D3DRMSHADEMODE oldsm = (D3DRMSHADEMODE)(quality & D3DRMSHADE_MASK);
    HRESULT rval;

    menu = GetMenu(win);
    menu = GetSubMenu(menu, 2);
    CheckMenuItem(menu, ShadeModeToMenuItem(oldsm),
                  MF_BYPOSITION|MF_UNCHECKED);
    CheckMenuItem(menu, ShadeModeToMenuItem(sm),
                  MF_BYPOSITION|MF_CHECKED);

    quality = (quality & ~D3DRMSHADE_MASK) | sm;
    rval = info->dev->SetQuality(quality);
    if (rval != D3DRM_OK) {
        Msg("Setting the render quality while changing the shade mode failed.\n%s", MyErrorToString(rval));
        return FALSE;
    }
    return TRUE;
}

static BOOL SetModel(HWND win, AppInfo* info, D3DRMCOLORMODEL model)
{
    HMENU menu;
    D3DRMCOLORMODEL oldModel = info->model;

    if (oldModel == model) return TRUE;

    info->model = model;
    if (!RebuildDevice(win, info, info->dev->GetWidth(), info->dev->GetHeight()))
        return FALSE;

    menu = GetMenu(win);
    menu = GetSubMenu(menu, 2);
    CheckMenuItem(menu, 9 + (int) oldModel, MF_BYPOSITION|MF_UNCHECKED);
    CheckMenuItem(menu, 9 + (int) model, MF_BYPOSITION|MF_CHECKED);
    return TRUE;
}

static BOOL ToggleDither(HWND win, AppInfo *info)
{
    HMENU menu;
    int dither = info->dev->GetDither();
    int checked;
    dither = !dither;
    if (FAILED(info->dev->SetDither(dither))) {
        Msg("Toggling dithering failed.\n");
        return FALSE;
    }
    menu = GetMenu(win);
    menu = GetSubMenu(menu, 2);

    if (dither) checked = MF_CHECKED;
    else checked = MF_UNCHECKED;

    //CheckMenuItem(menu, MENU_DITHER, MF_BYCOMMAND|checked);
    CheckMenuItem(menu, 13, MF_BYPOSITION|checked);
    return TRUE;
}

static BOOL ToggleTextureFiltering(HWND win, AppInfo *info)
{
    HMENU menu;
    D3DRMTEXTUREQUALITY tq = info->dev->GetTextureQuality();
    int checked;
    if (tq == D3DRMTEXTURE_NEAREST)
        tq = D3DRMTEXTURE_LINEAR;
    else
        tq = D3DRMTEXTURE_NEAREST;

    if (FAILED(info->dev->SetTextureQuality(tq))) {
        Msg("Setting texture quality failed.\n");
        return FALSE;
    }
    menu = GetMenu(win);
    menu = GetSubMenu(menu, 2);

    if (tq == D3DRMTEXTURE_LINEAR) checked = MF_CHECKED;
    else checked = MF_UNCHECKED;

    CheckMenuItem(menu, 14, MF_BYPOSITION|checked);
    return TRUE;
}

static BOOL
CreateLight(WPARAM wparam, AppInfo* info)
{
    LPDIRECT3DRMMESHBUILDER builder = NULL;
    LPDIRECT3DRMLIGHT light = NULL;
    LPDIRECT3DRMFRAME frame = NULL;
    HRESULT rval;

    if (FAILED(lpD3DRM->CreateMeshBuilder(&builder)))
        goto generic_error;

    if (wparam == MENU_LIGHT_DIRECTIONAL) {
        rval = builder->Load("camera.x", NULL, D3DRMLOAD_FROMFILE,
                             NULL, NULL);
        if (rval != D3DRM_OK) {
            Msg("Failed to load camera.x.\n%s", MyErrorToString(rval));
            goto ret_with_error;
        }
        if (FAILED(builder->SetQuality(D3DRMRENDER_UNLITFLAT)))
            goto generic_error;
        if (FAILED(lpD3DRM->CreateLightRGB
            (D3DRMLIGHT_DIRECTIONAL, D3DVAL(1.0), D3DVAL(1.0), D3DVAL(1.0), &light)))
            goto generic_error;
    } else if (wparam == MENU_LIGHT_PARALLEL_POINT) {
        rval = builder->Load("sphere2.x", NULL, D3DRMLOAD_FROMFILE,
                             NULL, NULL);
        if (rval != D3DRM_OK) {
            Msg("Failed to load sphere2.x.\n%s", MyErrorToString(rval));
            goto ret_with_error;
        }
        if (FAILED(builder->SetQuality(D3DRMRENDER_UNLITFLAT)))
            goto generic_error;
        if (FAILED(builder->Scale(D3DVAL(0.2), D3DVAL(0.2), D3DVAL(0.2))))
            goto generic_error;
        if (FAILED(lpD3DRM->CreateLightRGB
            (D3DRMLIGHT_PARALLELPOINT, D3DVAL(1.0), D3DVAL(1.0), D3DVAL(1.0), &light)))
            goto generic_error;
    } else if (wparam == MENU_LIGHT_POINT) {
        rval = builder->Load("sphere2.x", NULL, D3DRMLOAD_FROMFILE,
                             NULL, NULL);
        if (rval != D3DRM_OK) {
            Msg("Failed to load sphere2.x.\n%s", MyErrorToString(rval));
            goto ret_with_error;
        }
        if (FAILED(builder->SetQuality(D3DRMRENDER_UNLITFLAT)))
            goto generic_error;
        if (FAILED(builder->Scale(D3DVAL(0.2), D3DVAL(0.2), D3DVAL(0.2))))
            goto generic_error;
        if (FAILED(lpD3DRM->CreateLightRGB
            (D3DRMLIGHT_POINT, D3DVAL(1.0), D3DVAL(1.0), D3DVAL(1.0), &light)))
            goto generic_error;
    } else if (wparam == MENU_LIGHT_SPOT) {
        rval = builder->Load("camera.x", NULL, D3DRMLOAD_FROMFILE,
                             NULL, NULL);
        if (rval != D3DRM_OK) {
            Msg("Failed to load camera.x.\n%s", MyErrorToString(rval));
            goto ret_with_error;
        }
        if (FAILED(builder->SetQuality(D3DRMRENDER_UNLITFLAT)))
            goto generic_error;
        if (FAILED(lpD3DRM->CreateLightRGB(D3DRMLIGHT_SPOT, D3DVAL(1.0), D3DVAL(1.0), D3DVAL(1.0), &light)))
            goto generic_error;
    }
    if (FAILED(lpD3DRM->CreateFrame(info->scene, &frame)))
        goto generic_error;
    if (FAILED(frame->SetPosition(info->camera, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(10.0))))
        goto generic_error;
    if (FAILED(frame->AddVisual(builder)))
        goto generic_error;
    if (FAILED(frame->AddLight(light)))
        goto generic_error;

    builder->Release(), frame->Release(), light->Release();
    return TRUE;
generic_error:
    Msg("A failure occurred while creating a new light.\n");
ret_with_error:
    RELEASE(builder);
    RELEASE(light);
    RELEASE(frame);
    return FALSE;
}

HRESULT loadTextures(char *name, void *arg, LPDIRECT3DRMTEXTURE *tex)
{
    /* IDirect3DRM::LoadTexture checks whether the texture is a PPM or
    BMP which it knows how to load. If you want to load other formats 
    you could add code to the callback to load the code into an D3DRMIMAGE
    structure and call IDirect3DRM::CreateTexture */
    
    return lpD3DRM->LoadTexture(name, tex);
}

#define SIGN_EXTEND(w)    ((((int)(w)) << 16) >> 16)

/*
 * Handle messages for the main application window
 */
LONG FAR PASCAL WindowProc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static HCURSOR oldCursor = NULL;
    AppInfo *info;
    LPDIRECT3DRMFRAME sFrame = SelectedFrame();
    LPDIRECT3DRMMESHBUILDER sVisual = SelectedVisual();

    info = (AppInfo *) GetWindowLong(win, 0);
    active_window = info;

    switch (msg)
    {
    case WM_KEYDOWN:
        {   D3DVECTOR dir, up, right;

            info->camera->GetOrientation(info->scene, &dir, &up);
            D3DRMVectorCrossProduct(&right, &up, &dir);
            up.x /= D3DVAL(4.0);
            up.y /= D3DVAL(4.0);
            up.z /= D3DVAL(4.0);
            right.x /= D3DVAL(4.0);
            right.y /= D3DVAL(4.0);
            right.z /= D3DVAL(4.0);

            switch (wparam)
            {
            case 'T':
                info->camera->SetVelocity(info->scene, dir.x, dir.y, dir.z, FALSE);
                break;

            case 'Y':
                info->camera->SetVelocity(info->scene, D3DVAL(-100.0) * dir.x,
                                          D3DVAL(-100.0) * dir.y, 
                                          D3DVAL(-100.0) * dir.z, FALSE);

            case 'R':
                info->camera->SetVelocity(info->scene, -dir.x, -dir.y, -dir.z, FALSE);
                break;
                 
            case 'E':
                info->camera->SetVelocity(info->scene, D3DVAL(100.0) * dir.x,
                                          D3DVAL(100.0) * dir.y, 
                                          D3DVAL(100.0) * dir.z, FALSE);
            case VK_UP:
                info->camera->SetVelocity(info->scene, up.x, up.y, up.z, FALSE);
                break;

            case VK_DOWN:
                info->camera->SetVelocity(info->scene, -up.x, -up.y, -up.z, FALSE);
                break;

            case VK_RIGHT:
                info->camera->SetVelocity(info->scene, right.x, right.y, right.z, FALSE);
                break;

            case VK_LEFT:
                info->camera->SetVelocity(info->scene, -right.x, -right.y, -right.z, FALSE);
                break;

            case 'X':
                if (sFrame)
                    sFrame->SetVelocity(info->scene, dir.x, dir.y, dir.z, FALSE);
                break;

            case 'Z':
                if (sFrame)
                    sFrame->SetVelocity(info->scene, -dir.x, -dir.y, -dir.z, FALSE);
                break;

            case VK_SUBTRACT:
                if (sFrame)
                {   sVisual->Scale(D3DVAL(0.9), D3DVAL(0.9), D3DVAL(0.9));
                    SelectVisual(sVisual, sFrame);
                }
                break;

            case VK_ADD:
                if (sFrame)
                {   sVisual->Scale(D3DVAL(1.1), D3DVAL(1.1), D3DVAL(1.1));
                    SelectVisual(sVisual, sFrame);
                }
                break;
            }
        }
        break;

    case WM_KEYUP:
        switch (wparam)
        {
        case 'T':
        case 'R':
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
            info->camera->SetVelocity(info->scene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(0.0), FALSE);
            break;

        case 'Z':
        case 'X':
            if (sFrame)
                sFrame->SetVelocity(info->scene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(0.0), FALSE);
            break;
        }
        break;

    case WM_LBUTTONDOWN:
        {   HCURSOR hCur;
            int x = LOWORD(lparam);
            int y = HIWORD(lparam);
            last_x = x;
            last_y = y;
            FindAndSelectVisual(info->view, x, y);
            left_drag = TRUE;
            SetCapture(win);
            /* change to a groovy cursor */
            hCur = LoadCursor(NULL, IDC_ARROW);
            if (hCur) oldCursor = SetCursor(hCur);
            else oldCursor = NULL;
        }
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        left_drag = FALSE;
        if (oldCursor) SetCursor(oldCursor);
        break;

    case WM_RBUTTONDOWN:
        {
            HCURSOR hCur;
            int x = LOWORD(lparam);
            int y = HIWORD(lparam);
            last_x = x;
            last_y = y;
            FindAndSelectVisual(info->view, x, y);
            right_drag = TRUE;
            SetCapture(win);
            /* change to a groovy cursor */
            hCur = LoadCursor(NULL, IDC_ARROW);
            if (hCur) oldCursor = SetCursor(hCur);
            else oldCursor = NULL;
        }
        break;

    case WM_RBUTTONUP:
        right_drag = FALSE;
        ReleaseCapture();
        if (oldCursor) SetCursor(oldCursor);
        break;

    case WM_MOUSEMOVE:
        if ((wparam & MK_LBUTTON) && sFrame && left_drag)
        {   double delta_x, delta_y;
            delta_x = SIGN_EXTEND(LOWORD(lparam)) - last_x;
            delta_y = -SIGN_EXTEND((HIWORD(lparam)) - last_y);
            last_x = SIGN_EXTEND(LOWORD(lparam));
            last_y = SIGN_EXTEND(HIWORD(lparam));
            {
                double delta_r = sqrt(delta_x * delta_x + delta_y * delta_y);
                double radius = 50;
                double denom;

                denom = sqrt(radius * radius + delta_r * delta_r);

                if (delta_r == 0 || denom == 0) break;
                sFrame->SetRotation
                (   info->camera,
                    D3DDivide(D3DVAL((float) delta_y), D3DVAL((float) delta_r)),
                    D3DDivide(D3DVAL((float) -delta_x), D3DVAL((float) delta_r)),
                    D3DVAL(0.0),
                    D3DDivide(D3DVAL((float) delta_r), D3DVAL((float) denom))
                );
            }
        }
        else if ((wparam & MK_RBUTTON) && sFrame && right_drag)
        {   double delta_x, delta_y;
            D3DVECTOR p1;
            D3DRMVECTOR4D p2;

            delta_x = SIGN_EXTEND(LOWORD(lparam)) - last_x;
            delta_y = SIGN_EXTEND(HIWORD(lparam)) - last_y;
            last_x = SIGN_EXTEND(LOWORD(lparam));
            last_y = SIGN_EXTEND(HIWORD(lparam));
            sFrame->GetPosition(info->scene, &p1);
            info->view->Transform(&p2, &p1);
            p2.x += D3DMultiply(D3DVAL((float)delta_x), p2.w);
            p2.y += D3DMultiply(D3DVAL((float)delta_y), p2.w);
            info->view->InverseTransform(&p1, &p2);
            sFrame->SetPosition(info->scene, p1.x, p1.y, p1.z);
        }
        break;

    case WM_COMMAND:
        switch( wparam & 0xffff )
        {
        case MENU_FILE_ABOUT:
            DialogBox((HINSTANCE) GetWindowLong(win, GWL_HINSTANCE),"AboutBox", win, (DLGPROC)AboutDlgProc);
            break;

        case MENU_FILE_OPEN:
          {  LPDIRECT3DRMMESHBUILDER builder;
             HRESULT rval;
             char *file = OpenNewFile(win, "Open a Mesh file");
             if (file)
               {
                 if (FAILED(lpD3DRM->CreateMeshBuilder(&builder))) {
                   Msg("Failed the create a builder for the new mesh.\n");
                   break;
                 }
                 rval = builder->Load(file, NULL, D3DRMLOAD_FROMFILE,
                                      loadTextures, NULL);
                 if (rval != D3DRM_OK) {
                   Msg("Loading %s failed.\n%s", file, MyErrorToString(rval));
                   builder->Release();
                   break;
                 }
                 if (!PlaceMesh(builder, info)) {
                   Msg("Placing the mesh in the scene failed.\n");
                   builder->Release();
                   break;
                 }
                 builder->Release();
               }

             /*LoadAnimationSet(file, info);*/
             break;
          }

        case MENU_FILE_OPEN_ANIMSET:
          {
            char *file = OpenNewFile(win, "Open Animation ...");
            if (file) {
              if (LoadAnimationSet(file, info) == FALSE) {
                Msg("Loading and placing of %s failed.\n", file);
              }
            }
            break;
          }

        case MENU_FILE_OPEN_FRAME:
            {
              char *file = OpenNewFile(win, "Open Frame ...");
              if (file) {
                if (LoadFrameHierarchy(file, info) == FALSE) {
                  Msg("Loading and placing of %s failed.\n", file);
                }
              }
              break;
            }

        case MENU_FILE_EXIT:
            PostQuitMessage(0);
            break;

        case MENU_EDIT_CUT:
            CutVisual();
            break;

        case MENU_EDIT_COPY:
            CopyVisual();
            break;

        case MENU_EDIT_PASTE:
            PasteVisual(info->scene);
            break;

        case MENU_EDIT_DELETE:
            DeleteVisual();
            break;

        case MENU_EDIT_COLOR:
            if (sFrame)
            {
                LPDIRECT3DRMMESHBUILDER mesh;
                HRESULT rval;

                if (FAILED(sVisual->QueryInterface(IID_IDirect3DRMMeshBuilder,
                                                   (void**) &mesh)))
                    break;

                if (SelectedLight())
                {
                    D3DCOLOR c = SelectedLight()->GetColor();

                    if (ChooseNewColor(win, &c)) {
                        mesh->SetColor(c);
                        SelectedLight()->SetColor(c);
                    }
                } else {
                    D3DCOLOR c;

                    if (mesh->GetFaceCount()) {
                        LPDIRECT3DRMFACEARRAY faces;
                        LPDIRECT3DRMFACE face;
                        mesh->GetFaces(&faces);
                        faces->GetElement(0, &face);
                        c = face->GetColor();
                        RELEASE(face);
                        RELEASE(faces);
                    } else
                        c = D3DRMCreateColorRGB(D3DVAL(0.0), D3DVAL(0.0), D3DVAL(0.0));

                    if (ChooseNewColor(win, &c)) {
                        rval = mesh->SetColor(c);
                        if (rval != D3DRM_OK)
                            Msg("Failed to set the mesh's color.\n%s", MyErrorToString(rval));
                    }
                }

                RELEASE(mesh);
            }
            break;

        case MENU_EDIT_BOXES:
            {
                HMENU menu;
                int checked = ToggleBoxes() ? MF_CHECKED : MF_UNCHECKED;
                menu = GetMenu(win);
                menu = GetSubMenu(menu, 1);
                CheckMenuItem(menu, MENU_EDIT_BOXES, MF_BYCOMMAND|checked);
            }
            break;

        case MENU_QUALITY_LIGHTING:
            ToggleLighting(win, info);
            break;

        case MENU_QUALITY_POINTS:
            SetFillMode(win, info, D3DRMFILL_POINTS);
            break;

        case MENU_QUALITY_WIREFRAME:
            SetFillMode(win, info, D3DRMFILL_WIREFRAME);
            break;

        case MENU_QUALITY_SOLID:
            SetFillMode(win, info, D3DRMFILL_SOLID);
            break;

        case MENU_QUALITY_FLAT:
            SetShadeMode(win, info, D3DRMSHADE_FLAT);
            break;

        case MENU_QUALITY_GOURAUD:
            SetShadeMode(win, info, D3DRMSHADE_GOURAUD);
            break;

        case MENU_QUALITY_PHONG:
            SetShadeMode(win, info, D3DRMSHADE_PHONG);
            break;

        case MENU_MODEL_MONO:
            if (!SetModel(win, info, D3DCOLOR_MONO))
                bQuit = TRUE;
            break;

        case MENU_MODEL_RGB:
            if (!SetModel(win, info, D3DCOLOR_RGB))
                bQuit = TRUE;
            break;

        case MENU_DITHER:
            ToggleDither(win, info);
            break;

        case MENU_TEXTURE_FILTERING:
            ToggleTextureFiltering(win, info);
            break;

        case MENU_LIGHT_DIRECTIONAL:
        case MENU_LIGHT_PARALLEL_POINT:
        case MENU_LIGHT_POINT:
        case MENU_LIGHT_SPOT:
            {
                CreateLight(wparam, info);
            }
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        bQuit = TRUE;
        break;

    case WM_SIZE:
        {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
                if (width && height) {
                        if (!ResizeViewport(win, info, width, height)) {
                            bQuit = TRUE;
                            break;
                        }   
                        active_window->bMinimized = FALSE;
                } else {
                        active_window->bMinimized = TRUE;
                }
        }
        break;

    case WM_ACTIVATE:
        {   LPDIRECT3DRMWINDEVICE windev;
            if (!info || !info->dev) break;
            if (SUCCEEDED(info->dev->QueryInterface(IID_IDirect3DRMWinDevice, (void **) &windev)))
            {   if (FAILED(windev->HandleActivate(wparam)))
                    Msg("Failed to handle WM_ACTIVATE.\n");
                windev->Release();
            } else {
                Msg("Failed to create Windows device to handle WM_ACTIVATE.\n");
            }
        }
        break;

    case WM_PAINT:
        if (info)
        {   RECT r;
            PAINTSTRUCT ps;
            LPDIRECT3DRMWINDEVICE windev;

            if (!info || !info->dev) break;
            if (GetUpdateRect(win, &r, FALSE))
            {   BeginPaint(win, &ps);
                if (SUCCEEDED(info->dev->QueryInterface(IID_IDirect3DRMWinDevice, (void **) &windev)))
                {   if (FAILED(windev->HandlePaint(ps.hdc)))
                        Msg("Failed to handle WM_PAINT.\n");
                    windev->Release();
                } else {
                    Msg("Failed to create Windows device to handle WM_PAINT.\n");
                }
                EndPaint(win, &ps);
            }
        }

        else return DefWindowProc(win, msg, wparam, lparam);

    default:
        return DefWindowProc(win, msg, wparam, lparam);
    }
    return 0L;
}

/*
 * MyErrorToString
 * Returns a pointer to a string describing the given error code.
 */
char*
MyErrorToString(HRESULT error)
{
    switch(error) {
        case DD_OK:
            /* Also includes D3D_OK and D3DRM_OK */
            return "No error.\0";
        case DDERR_ALREADYINITIALIZED:
            return "This object is already initialized.\0";
        case DDERR_BLTFASTCANTCLIP:
            return "Return if a clipper object is attached to the source surface passed into a BltFast call.\0";
        case DDERR_CANNOTATTACHSURFACE:
            return "This surface can not be attached to the requested surface.\0";
        case DDERR_CANNOTDETACHSURFACE:
            return "This surface can not be detached from the requested surface.\0";
        case DDERR_CANTCREATEDC:
            return "Windows can not create any more DCs.\0";
        case DDERR_CANTDUPLICATE:
            return "Can't duplicate primary & 3D surfaces, or surfaces that are implicitly created.\0";
        case DDERR_CLIPPERISUSINGHWND:
            return "An attempt was made to set a cliplist for a clipper object that is already monitoring an hwnd.\0";
        case DDERR_COLORKEYNOTSET:
            return "No src color key specified for this operation.\0";
        case DDERR_CURRENTLYNOTAVAIL:
            return "Support is currently not available.\0";
        case DDERR_DIRECTDRAWALREADYCREATED:
            return "A DirectDraw object representing this driver has already been created for this process.\0";
        case DDERR_EXCEPTION:
            return "An exception was encountered while performing the requested operation.\0";
        case DDERR_EXCLUSIVEMODEALREADYSET:
            return "An attempt was made to set the cooperative level when it was already set to exclusive.\0";
        case DDERR_GENERIC:
            return "Generic failure.\0";
        case DDERR_HEIGHTALIGN:
            return "Height of rectangle provided is not a multiple of reqd alignment.\0";
        case DDERR_HWNDALREADYSET:
            return "The CooperativeLevel HWND has already been set. It can not be reset while the process has surfaces or palettes created.\0";
        case DDERR_HWNDSUBCLASSED:
            return "HWND used by DirectDraw CooperativeLevel has been subclassed, this prevents DirectDraw from restoring state.\0";
        case DDERR_IMPLICITLYCREATED:
            return "This surface can not be restored because it is an implicitly created surface.\0";
        case DDERR_INCOMPATIBLEPRIMARY:
            return "Unable to match primary surface creation request with existing primary surface.\0";
        case DDERR_INVALIDCAPS:
            return "One or more of the caps bits passed to the callback are incorrect.\0";
        case DDERR_INVALIDCLIPLIST:
            return "DirectDraw does not support the provided cliplist.\0";
        case DDERR_INVALIDDIRECTDRAWGUID:
            return "The GUID passed to DirectDrawCreate is not a valid DirectDraw driver identifier.\0";
        case DDERR_INVALIDMODE:
            return "DirectDraw does not support the requested mode.\0";
        case DDERR_INVALIDOBJECT:
            return "DirectDraw received a pointer that was an invalid DIRECTDRAW object.\0";
        case DDERR_INVALIDPARAMS:
            return "One or more of the parameters passed to the function are incorrect.\0";
        case DDERR_INVALIDPIXELFORMAT:
            return "The pixel format was invalid as specified.\0";
        case DDERR_INVALIDPOSITION:
            return "Returned when the position of the overlay on the destination is no longer legal for that destination.\0";
        case DDERR_INVALIDRECT:
            return "Rectangle provided was invalid.\0";
        case DDERR_LOCKEDSURFACES:
            return "Operation could not be carried out because one or more surfaces are locked.\0";
        case DDERR_NO3D:
            return "There is no 3D present.\0";
        case DDERR_NOALPHAHW:
            return "Operation could not be carried out because there is no alpha accleration hardware present or available.\0";
        case DDERR_NOBLTHW:
            return "No blitter hardware present.\0";
        case DDERR_NOCLIPLIST:
            return "No cliplist available.\0";
        case DDERR_NOCLIPPERATTACHED:
            return "No clipper object attached to surface object.\0";
        case DDERR_NOCOLORCONVHW:
            return "Operation could not be carried out because there is no color conversion hardware present or available.\0";
        case DDERR_NOCOLORKEY:
            return "Surface doesn't currently have a color key\0";
        case DDERR_NOCOLORKEYHW:
            return "Operation could not be carried out because there is no hardware support of the destination color key.\0";
        case DDERR_NOCOOPERATIVELEVELSET:
            return "Create function called without DirectDraw object method SetCooperativeLevel being called.\0";
        case DDERR_NODC:
            return "No DC was ever created for this surface.\0";
        case DDERR_NODDROPSHW:
            return "No DirectDraw ROP hardware.\0";
        case DDERR_NODIRECTDRAWHW:
            return "A hardware-only DirectDraw object creation was attempted but the driver did not support any hardware.\0";
        case DDERR_NOEMULATION:
            return "Software emulation not available.\0";
        case DDERR_NOEXCLUSIVEMODE:
            return "Operation requires the application to have exclusive mode but the application does not have exclusive mode.\0";
        case DDERR_NOFLIPHW:
            return "Flipping visible surfaces is not supported.\0";
        case DDERR_NOGDI:
            return "There is no GDI present.\0";
        case DDERR_NOHWND:
            return "Clipper notification requires an HWND or no HWND has previously been set as the CooperativeLevel HWND.\0";
        case DDERR_NOMIRRORHW:
            return "Operation could not be carried out because there is no hardware present or available.\0";
        case DDERR_NOOVERLAYDEST:
            return "Returned when GetOverlayPosition is called on an overlay that UpdateOverlay has never been called on to establish a destination.\0";
        case DDERR_NOOVERLAYHW:
            return "Operation could not be carried out because there is no overlay hardware present or available.\0";
        case DDERR_NOPALETTEATTACHED:
            return "No palette object attached to this surface.\0";
        case DDERR_NOPALETTEHW:
            return "No hardware support for 16 or 256 color palettes.\0";
        case DDERR_NORASTEROPHW:
            return "Operation could not be carried out because there is no appropriate raster op hardware present or available.\0";
        case DDERR_NOROTATIONHW:
            return "Operation could not be carried out because there is no rotation hardware present or available.\0";
        case DDERR_NOSTRETCHHW:
            return "Operation could not be carried out because there is no hardware support for stretching.\0";
        case DDERR_NOT4BITCOLOR:
            return "DirectDrawSurface is not in 4 bit color palette and the requested operation requires 4 bit color palette.\0";
        case DDERR_NOT4BITCOLORINDEX:
            return "DirectDrawSurface is not in 4 bit color index palette and the requested operation requires 4 bit color index palette.\0";
        case DDERR_NOT8BITCOLOR:
            return "DirectDrawSurface is not in 8 bit color mode and the requested operation requires 8 bit color.\0";
        case DDERR_NOTAOVERLAYSURFACE:
            return "Returned when an overlay member is called for a non-overlay surface.\0";
        case DDERR_NOTEXTUREHW:
            return "Operation could not be carried out because there is no texture mapping hardware present or available.\0";
        case DDERR_NOTFLIPPABLE:
            return "An attempt has been made to flip a surface that is not flippable.\0";
        case DDERR_NOTFOUND:
            return "Requested item was not found.\0";
        case DDERR_NOTLOCKED:
            return "Surface was not locked.  An attempt to unlock a surface that was not locked at all, or by this process, has been attempted.\0";
        case DDERR_NOTPALETTIZED:
            return "The surface being used is not a palette-based surface.\0";
        case DDERR_NOVSYNCHW:
            return "Operation could not be carried out because there is no hardware support for vertical blank synchronized operations.\0";
        case DDERR_NOZBUFFERHW:
            return "Operation could not be carried out because there is no hardware support for zbuffer blitting.\0";
        case DDERR_NOZOVERLAYHW:
            return "Overlay surfaces could not be z layered based on their BltOrder because the hardware does not support z layering of overlays.\0";
        case DDERR_OUTOFCAPS:
            return "The hardware needed for the requested operation has already been allocated.\0";
        case DDERR_OUTOFMEMORY:
            return "DirectDraw does not have enough memory to perform the operation.\0";
        case DDERR_OUTOFVIDEOMEMORY:
            return "DirectDraw does not have enough memory to perform the operation.\0";
        case DDERR_OVERLAYCANTCLIP:
            return "The hardware does not support clipped overlays.\0";
        case DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
            return "Can only have ony color key active at one time for overlays.\0";
        case DDERR_OVERLAYNOTVISIBLE:
            return "Returned when GetOverlayPosition is called on a hidden overlay.\0";
        case DDERR_PALETTEBUSY:
            return "Access to this palette is being refused because the palette is already locked by another thread.\0";
        case DDERR_PRIMARYSURFACEALREADYEXISTS:
            return "This process already has created a primary surface.\0";
        case DDERR_REGIONTOOSMALL:
            return "Region passed to Clipper::GetClipList is too small.\0";
        case DDERR_SURFACEALREADYATTACHED:
            return "This surface is already attached to the surface it is being attached to.\0";
        case DDERR_SURFACEALREADYDEPENDENT:
            return "This surface is already a dependency of the surface it is being made a dependency of.\0";
        case DDERR_SURFACEBUSY:
            return "Access to this surface is being refused because the surface is already locked by another thread.\0";
        case DDERR_SURFACEISOBSCURED:
            return "Access to surface refused because the surface is obscured.\0";
        case DDERR_SURFACELOST:
            return "Access to this surface is being refused because the surface memory is gone. The DirectDrawSurface object representing this surface should have Restore called on it.\0";
        case DDERR_SURFACENOTATTACHED:
            return "The requested surface is not attached.\0";
        case DDERR_TOOBIGHEIGHT:
            return "Height requested by DirectDraw is too large.\0";
        case DDERR_TOOBIGSIZE:
            return "Size requested by DirectDraw is too large, but the individual height and width are OK.\0";
        case DDERR_TOOBIGWIDTH:
            return "Width requested by DirectDraw is too large.\0";
        case DDERR_UNSUPPORTED:
            return "Action not supported.\0";
        case DDERR_UNSUPPORTEDFORMAT:
            return "FOURCC format requested is unsupported by DirectDraw.\0";
        case DDERR_UNSUPPORTEDMASK:
            return "Bitmask in the pixel format requested is unsupported by DirectDraw.\0";
        case DDERR_VERTICALBLANKINPROGRESS:
            return "Vertical blank is in progress.\0";
        case DDERR_WASSTILLDRAWING:
            return "Informs DirectDraw that the previous Blt which is transfering information to or from this Surface is incomplete.\0";
        case DDERR_WRONGMODE:
            return "This surface can not be restored because it was created in a different mode.\0";
        case DDERR_XALIGN:
            return "Rectangle provided was not horizontally aligned on required boundary.\0";
        case D3DERR_BADMAJORVERSION:
            return "D3DERR_BADMAJORVERSION\0";
        case D3DERR_BADMINORVERSION:
            return "D3DERR_BADMINORVERSION\0";
        case D3DERR_EXECUTE_LOCKED:
            return "D3DERR_EXECUTE_LOCKED\0";
        case D3DERR_EXECUTE_NOT_LOCKED:
            return "D3DERR_EXECUTE_NOT_LOCKED\0";
        case D3DERR_EXECUTE_CREATE_FAILED:
            return "D3DERR_EXECUTE_CREATE_FAILED\0";
        case D3DERR_EXECUTE_DESTROY_FAILED:
            return "D3DERR_EXECUTE_DESTROY_FAILED\0";
        case D3DERR_EXECUTE_LOCK_FAILED:
            return "D3DERR_EXECUTE_LOCK_FAILED\0";
        case D3DERR_EXECUTE_UNLOCK_FAILED:
            return "D3DERR_EXECUTE_UNLOCK_FAILED\0";
        case D3DERR_EXECUTE_FAILED:
            return "D3DERR_EXECUTE_FAILED\0";
        case D3DERR_EXECUTE_CLIPPED_FAILED:
            return "D3DERR_EXECUTE_CLIPPED_FAILED\0";
        case D3DERR_TEXTURE_NO_SUPPORT:
            return "D3DERR_TEXTURE_NO_SUPPORT\0";
        case D3DERR_TEXTURE_NOT_LOCKED:
            return "D3DERR_TEXTURE_NOT_LOCKED\0";
        case D3DERR_TEXTURE_LOCKED:
            return "D3DERR_TEXTURELOCKED\0";
        case D3DERR_TEXTURE_CREATE_FAILED:
            return "D3DERR_TEXTURE_CREATE_FAILED\0";
        case D3DERR_TEXTURE_DESTROY_FAILED:
            return "D3DERR_TEXTURE_DESTROY_FAILED\0";
        case D3DERR_TEXTURE_LOCK_FAILED:
            return "D3DERR_TEXTURE_LOCK_FAILED\0";
        case D3DERR_TEXTURE_UNLOCK_FAILED:
            return "D3DERR_TEXTURE_UNLOCK_FAILED\0";
        case D3DERR_TEXTURE_LOAD_FAILED:
            return "D3DERR_TEXTURE_LOAD_FAILED\0";
        case D3DERR_MATRIX_CREATE_FAILED:
            return "D3DERR_MATRIX_CREATE_FAILED\0";
        case D3DERR_MATRIX_DESTROY_FAILED:
            return "D3DERR_MATRIX_DESTROY_FAILED\0";
        case D3DERR_MATRIX_SETDATA_FAILED:
            return "D3DERR_MATRIX_SETDATA_FAILED\0";
        case D3DERR_SETVIEWPORTDATA_FAILED:
            return "D3DERR_SETVIEWPORTDATA_FAILED\0";
        case D3DERR_MATERIAL_CREATE_FAILED:
            return "D3DERR_MATERIAL_CREATE_FAILED\0";
        case D3DERR_MATERIAL_DESTROY_FAILED:
            return "D3DERR_MATERIAL_DESTROY_FAILED\0";
        case D3DERR_MATERIAL_SETDATA_FAILED:
            return "D3DERR_MATERIAL_SETDATA_FAILED\0";
        case D3DERR_LIGHT_SET_FAILED:
            return "D3DERR_LIGHT_SET_FAILED\0";
        case D3DRMERR_BADOBJECT:
            return "D3DRMERR_BADOBJECT\0";
        case D3DRMERR_BADTYPE:
            return "D3DRMERR_BADTYPE\0";
        case D3DRMERR_BADALLOC:
            return "D3DRMERR_BADALLOC\0";
        case D3DRMERR_FACEUSED:
            return "D3DRMERR_FACEUSED\0";
        case D3DRMERR_NOTFOUND:
            return "D3DRMERR_NOTFOUND\0";
        case D3DRMERR_NOTDONEYET:
            return "D3DRMERR_NOTDONEYET\0";
        case D3DRMERR_FILENOTFOUND:
            return "The file was not found.\0";
        case D3DRMERR_BADFILE:
            return "D3DRMERR_BADFILE\0";
        case D3DRMERR_BADDEVICE:
            return "D3DRMERR_BADDEVICE\0";
        case D3DRMERR_BADVALUE:
            return "D3DRMERR_BADVALUE\0";
        case D3DRMERR_BADMAJORVERSION:
            return "D3DRMERR_BADMAJORVERSION\0";
        case D3DRMERR_BADMINORVERSION:
            return "D3DRMERR_BADMINORVERSION\0";
        case D3DRMERR_UNABLETOEXECUTE:
            return "D3DRMERR_UNABLETOEXECUTE\0";
        default:
            return "Unrecognized error value.\0";
    }
}
