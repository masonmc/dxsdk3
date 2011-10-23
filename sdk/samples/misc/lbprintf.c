/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation. All Rights Reserved.
 *
 *  File:       lbprintf.c
 *  Content:    Routines to display text in a listbox
 *
 ***************************************************************************/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include "lbprintf.h"

static HWND     hWndListBox;
static DWORD    dwPos;

/*
 * LBCreate
 *
 * Create a list box on a given parent window
 */
void LBCreate( HWND hWnd, DWORD pos )
{
    hWndListBox = CreateWindow(
        "listbox",              // class
        NULL,                   // caption
        WS_BORDER | WS_CHILDWINDOW | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, // style
        0,                      // x pos
        0,                      // y pos
        0,                      // width
        0,                      // height
        hWnd,                   // parent window
        NULL,                   // menu 
        GetWindowInstance( hWnd ), // instance
        NULL                    // parms
        );

    ShowWindow( hWndListBox, SW_NORMAL );
    UpdateWindow( hWndListBox );

    dwPos = pos;
    if( dwPos > 100 )
    {
        dwPos = 100;
    }

} /* LBCreate */

/*
 * LBSize
 *
 * New size for the listbox.   Should be called whenever the parent window
 * is resized
 */
void LBSize( DWORD width, DWORD height )
{
    DWORD       y;

    y = (height*(100-dwPos))/100;
    MoveWindow( hWndListBox, 0, y, width-1, height-y-1, TRUE );

} /* LBSize */

/*
 * LBClear
 *
 * Clear the listbox
 */
void LBClear( void )
{
    SendMessage( hWndListBox, LB_RESETCONTENT, 0, 0L );

} /* LBClear */

/*
 * LBPrintf
 */
void __cdecl LBPrintf( LPSTR fmt, ... )
{
    char        buff[512];
    UINT        sel;

    wvsprintf( buff, fmt, (LPVOID)(&fmt+1) );
    SendMessage( hWndListBox, LB_ADDSTRING, 0, (LONG) (LPSTR) buff );
    sel = (UINT) SendMessage( hWndListBox, LB_GETCOUNT, 0, 0L );
    if( sel != LB_ERR )
    {
        SendMessage( hWndListBox, LB_SETCURSEL, sel-1, 0L );
    }

} /* LBPrintf */

typedef struct
{
    HRESULT     rval;
    LPSTR       str;
} ERRLIST;

static ERRLIST elErrors[] =
{
    { DD_OK, "DD_OK" },
    { DDERR_ALREADYINITIALIZED, "DDERR_ALREADYINITIALIZED" },
    { DDERR_CANNOTATTACHSURFACE, "DDERR_CANNOTATTACHSURFACE" },
    { DDERR_CANNOTDETACHSURFACE, "DDERR_CANNOTDETACHSURFACE" },
    { DDERR_CURRENTLYNOTAVAIL, "DDERR_CURRENTLYNOTAVAIL" },
    { DDERR_EXCEPTION, "DDERR_EXCEPTION" },
    { DDERR_GENERIC, "DDERR_GENERIC" },
    { DDERR_HEIGHTALIGN, "DDERR_HEIGHTALIGN" },
    { DDERR_INCOMPATIBLEPRIMARY, "DDERR_INCOMPATIBLEPRIMARY" },
    { DDERR_INVALIDCAPS, "DDERR_INVALIDCAPS" },
    { DDERR_INVALIDCLIPLIST, "DDERR_INVALIDCLIPLIST" },
    { DDERR_INVALIDMODE, "DDERR_INVALIDMODE" },
    { DDERR_INVALIDOBJECT, "DDERR_INVALIDOBJECT" },
    { DDERR_INVALIDPARAMS, "DDERR_INVALIDPARAMS" },
    { DDERR_INVALIDPIXELFORMAT, "DDERR_INVALIDPIXELFORMAT" },
    { DDERR_INVALIDRECT, "DDERR_INVALIDRECT" },
    { DDERR_LOCKEDSURFACES, "DDERR_LOCKEDSURFACES" },
    { DDERR_NO3D, "DDERR_NO3D" },
    { DDERR_NOALPHAHW, "DDERR_NOALPHAHW" },
    { DDERR_NOCLIPLIST, "DDERR_NOCLIPLIST" },
    { DDERR_NOCOLORCONVHW, "DDERR_NOCOLORCONVHW" },
    { DDERR_NOCOOPERATIVELEVELSET, "DDERR_NOCOOPERATIVELEVELSET" },
    { DDERR_NOCOLORKEY, "DDERR_NOCOLORKEY" },
    { DDERR_NOCOLORKEYHW, "DDERR_NOCOLORKEYHW" },
    { DDERR_NOEXCLUSIVEMODE, "DDERR_NOEXCLUSIVEMODE" },
    { DDERR_NOFLIPHW, "DDERR_NOFLIPHW" },
    { DDERR_NOGDI, "DDERR_NOGDI" },
    { DDERR_NOMIRRORHW, "DDERR_NOMIRRORHW" },
    { DDERR_NOTFOUND, "DDERR_NOTFOUND" },
    { DDERR_NOOVERLAYHW, "DDERR_NOOVERLAYHW" },
    { DDERR_NORASTEROPHW, "DDERR_NORASTEROPHW" },
    { DDERR_NOROTATIONHW, "DDERR_NOROTATIONHW" },
    { DDERR_NOSTRETCHHW, "DDERR_NOSTRETCHHW" },
    { DDERR_NOT4BITCOLOR, "DDERR_NOT4BITCOLOR" },
    { DDERR_NOT4BITCOLORINDEX, "DDERR_NOT4BITCOLORINDEX" },
    { DDERR_NOT8BITCOLOR, "DDERR_NOT8BITCOLOR" },
    { DDERR_NOTEXTUREHW, "DDERR_NOTEXTUREHW" },
    { DDERR_NOVSYNCHW, "DDERR_NOVSYNCHW" },
    { DDERR_NOZBUFFERHW, "DDERR_NOZBUFFERHW" },
    { DDERR_NOZOVERLAYHW, "DDERR_NOZOVERLAYHW" },
    { DDERR_OUTOFCAPS, "DDERR_OUTOFCAPS" },
    { DDERR_OUTOFMEMORY, "DDERR_OUTOFMEMORY" },
    { DDERR_OUTOFVIDEOMEMORY, "DDERR_OUTOFVIDEOMEMORY" },
    { DDERR_OVERLAYCANTCLIP, "DDERR_OVERLAYCANTCLIP" },
    { DDERR_OVERLAYCOLORKEYONLYONEACTIVE, "DDERR_OVERLAYCOLORKEYONLYONEACTIVE" },
    { DDERR_PALETTEBUSY, "DDERR_PALETTEBUSY" },
    { DDERR_COLORKEYNOTSET, "DDERR_COLORKEYNOTSET" },
    { DDERR_SURFACEALREADYATTACHED, "DDERR_SURFACEALREADYATTACHED" },
    { DDERR_SURFACEALREADYDEPENDENT, "DDERR_SURFACEALREADYDEPENDENT" },
    { DDERR_SURFACEBUSY, "DDERR_SURFACEBUSY" },
    { DDERR_SURFACEISOBSCURED, "DDERR_SURFACEISOBSCURED" },
    { DDERR_SURFACELOST, "DDERR_SURFACELOST" },
    { DDERR_SURFACENOTATTACHED, "DDERR_SURFACENOTATTACHED" },
    { DDERR_TOOBIGHEIGHT, "DDERR_TOOBIGHEIGHT" },
    { DDERR_TOOBIGSIZE, "DDERR_TOOBIGSIZE" },
    { DDERR_TOOBIGWIDTH, "DDERR_TOOBIGWIDTH" },
    { DDERR_UNSUPPORTED, "DDERR_UNSUPPORTED" },
    { DDERR_UNSUPPORTEDFORMAT, "DDERR_UNSUPPORTEDFORMAT" },
    { DDERR_UNSUPPORTEDMASK, "DDERR_UNSUPPORTEDMASK" },
    { DDERR_VERTICALBLANKINPROGRESS, "DDERR_VERTICALBLANKINPROGRESS" },
    { DDERR_WASSTILLDRAWING, "DDERR_WASSTILLDRAWING" },
    { DDERR_XALIGN, "DDERR_XALIGN" },
    { DDERR_INVALIDDIRECTDRAWGUID, "DDERR_INVALIDDIRECTDRAWGUID" },
    { DDERR_DIRECTDRAWALREADYCREATED, "DDERR_DIRECTDRAWALREADYCREATED" },
    { DDERR_NODIRECTDRAWHW, "DDERR_NODIRECTDRAWHW" },
    { DDERR_PRIMARYSURFACEALREADYEXISTS, "DDERR_PRIMARYSURFACEALREADYEXISTS" },
    { DDERR_NOEMULATION, "DDERR_NOEMULATION" },
    { DDERR_REGIONTOOSMALL, "DDERR_REGIONTOOSMALL" },
    { DDERR_CLIPPERISUSINGHWND, "DDERR_CLIPPERISUSINGHWND" },
    { DDERR_NOCLIPPERATTACHED, "DDERR_NOCLIPPERATTACHED" },
    { DDERR_NOHWND, "DDERR_NOHWND" },
    { DDERR_HWNDSUBCLASSED, "DDERR_HWNDSUBCLASSED" },
    { DDERR_HWNDALREADYSET, "DDERR_HWNDALREADYSET" },
    { DDERR_NOPALETTEATTACHED, "DDERR_NOPALETTEATTACHED" },
    { DDERR_NOPALETTEHW, "DDERR_NOPALETTEHW" },
    { DDERR_BLTFASTCANTCLIP, "DDERR_BLTFASTCANTCLIP" },
    { DDERR_NOBLTHW, "DDERR_NOBLTHW" },
    { DDERR_NODDROPSHW, "DDERR_NODDROPSHW" },
    { DDERR_OVERLAYNOTVISIBLE, "DDERR_OVERLAYNOTVISIBLE" },
    { DDERR_NOOVERLAYDEST, "DDERR_NOOVERLAYDEST" },
    { DDERR_INVALIDPOSITION, "DDERR_INVALIDPOSITION" },
    { DDERR_NOTAOVERLAYSURFACE, "DDERR_NOTAOVERLAYSURFACE" },
    { DDERR_EXCLUSIVEMODEALREADYSET, "DDERR_EXCLUSIVEMODEALREADYSET" },
    { DDERR_NOTFLIPPABLE, "DDERR_NOTFLIPPABLE" },
    { DDERR_CANTDUPLICATE, "DDERR_CANTDUPLICATE" },
    { DDERR_NOTLOCKED, "DDERR_NOTLOCKED" },
    { DDERR_CANTCREATEDC, "DDERR_CANTCREATEDC" },
    { DDERR_NODC, "DDERR_NODC" },
    { DDERR_WRONGMODE, "DDERR_WRONGMODE" },
    { DDERR_IMPLICITLYCREATED, "DDERR_IMPLICITLYCREATED" },
};

/*
 * getErrorString
 */
static LPSTR getErrorString( HRESULT ddrval )
{
    int i;

    for( i=0;i<sizeof( elErrors )/sizeof( elErrors[0] );i++ )
    {
        if( ddrval == elErrors[i].rval )
        {
            return elErrors[i].str;
        }
    }
    return "Unknown Error Code";


} /* getErrorString */

/*
 * LBPrintfDDRC
 *
 * Display a DirectDraw error code in human readable form
 */
void __cdecl LBPrintfDDRC( HRESULT ddrval, LPSTR fmt, ... )
{
    char        buff[512];

    wvsprintf( buff, fmt, (LPVOID)(&fmt+1) );
    LBPrintf( "%s, rc=%ld (0x%08lx:%s)", buff, LOWORD( ddrval ),
                                ddrval, getErrorString( ddrval ) );

} /* LBPrintfDDRC */
