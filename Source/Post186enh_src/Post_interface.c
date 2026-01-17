/*
 *  Source generated with GadToolsBox V1.4
 *  which is (c) Copyright 1991,92 Jaba Development
 *  Severely edited by Robert Poole
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <clib/utility_protos.h>
#include <string.h>

#include "Post_interface.h"

APTR                   ReqVisualInfo = NULL;
struct Window         *ReqWnd = NULL;
struct Gadget         *ReqGList = NULL;
struct Gadget         *ReqGadgets[22];
UWORD                  ReqLeft = 32;
UWORD                  ReqTop = 68;
UWORD                  ReqWidth = 577;
UWORD                  ReqHeight = 229;
UBYTE                 *ReqWdt = (UBYTE *)"Post V1.86enh";

UBYTE         *Output0Labels[] = {
    (UBYTE *)"Printer",
    (UBYTE *)"Screen",
    (UBYTE *)"IFF File",
    NULL };

UBYTE         *Colours0Labels[] = {
    (UBYTE *)"Black and White",
    (UBYTE *)"3 Colour (RGB)",
    (UBYTE *)"4 Colour (CMYK)",
    NULL };

UBYTE         *density0Labels[] = {
    (UBYTE *)"Density = 1",
    (UBYTE *)"Density = 2",
    (UBYTE *)"Density = 3",
    (UBYTE *)"Density = 4",
    (UBYTE *)"Density = 5",
    (UBYTE *)"Density = 6",
    (UBYTE *)"Density = 7",
    NULL };

struct IntuiText  ReqIText[] = {
    7, 0, JAM1,285, 13, &topaz11, (UBYTE *)"Output", &ReqIText[1],
    7, 0, JAM1,381, 13, &topaz11, (UBYTE *)"Colours", &ReqIText[2],
    7, 0, JAM1,340, 111, &topaz11, (UBYTE *)"Page Size", &ReqIText[3],
    7, 0, JAM1,502, 107, &topaz11, (UBYTE *)"Memory", NULL };

UWORD ReqGTypes[] = {
    STRING_KIND,
    STRING_KIND,
    STRING_KIND,
    STRING_KIND,
    STRING_KIND,
    STRING_KIND,
    BUTTON_KIND,
    BUTTON_KIND,
    MX_KIND,
    MX_KIND,
    CYCLE_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    CHECKBOX_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    INTEGER_KIND,
    CHECKBOX_KIND
};

struct NewGadget ReqNGad[] = {
    22, 22, 219, 21, (UBYTE *)"Startup file name(s)", NULL, GD_startup1, PLACETEXT_ABOVE ,NULL, NULL,
    22, 46, 219, 21, NULL, NULL, GD_startup2, 0 ,NULL, NULL,
    22, 70, 219, 21, NULL, NULL, GD_startup3, 0 ,NULL, NULL,
    22, 94, 219, 21, NULL, NULL, GD_startup4, 0 ,NULL, NULL,
    22, 119, 219, 21, NULL, NULL, GD_startup5, 0 ,NULL, NULL,
    26, 161, 220, 24, (UBYTE *)"IFF file name pattern", NULL, GD_iffname, PLACETEXT_ABOVE ,NULL, NULL,
    27, 199, 80, 22, (UBYTE *)"OK", NULL, GD_okgad, PLACETEXT_IN ,NULL, NULL,
    165, 199, 80, 22, (UBYTE *)"Cancel", NULL, GD_cancelgad, PLACETEXT_IN ,NULL, NULL,
    286, 25, 17, 9, NULL, NULL, GD_Output, PLACETEXT_RIGHT ,NULL, NULL,
    383, 25, 17, 9, NULL, NULL, GD_Colours, PLACETEXT_RIGHT ,NULL, NULL,
    320, 77, 125, 19, (UBYTE *)"Printer Density", NULL, GD_density, PLACETEXT_ABOVE ,NULL, NULL,
    312, 124, 61, 16, (UBYTE *)"X, Y", NULL, GD_xsize, PLACETEXT_LEFT ,NULL, NULL,
    377, 124, 61, 16, NULL, NULL, GD_ysize, 0 ,NULL, NULL,
    312, 144, 61, 16, (UBYTE *)"Offset", NULL, GD_offx, PLACETEXT_LEFT ,NULL, NULL,
    377, 144, 61, 16, NULL, NULL, GD_offy, 0 ,NULL, NULL,
    312, 165, 61, 16, (UBYTE *)"DPI", NULL, GD_xdpi, PLACETEXT_LEFT ,NULL, NULL,
    377, 165, 61, 16, NULL, NULL, GD_ydpi, 0 ,NULL, NULL,
    301, 193, 26, 11, (UBYTE *)"Close WBench", NULL, GD_wbclose, PLACETEXT_RIGHT ,NULL, NULL,
    494, 124, 67, 17, (UBYTE *)"Fonts", NULL, GD_fontmem, PLACETEXT_LEFT ,NULL, NULL,
    494, 145, 67, 17, (UBYTE *)"Htone", NULL, GD_htonemem, PLACETEXT_LEFT ,NULL, NULL,
    494, 165, 67, 17, (UBYTE *)"VM", NULL, GD_VMem, PLACETEXT_LEFT ,NULL, NULL,
    494, 186, 67, 17, (UBYTE *)"Paths", NULL, GD_pathmem, PLACETEXT_LEFT ,NULL, NULL,
    301, 209, 26, 11, (UBYTE *)"Sunmouse compat", NULL, GD_sunmouse, PLACETEXT_RIGHT ,NULL, NULL
};

ULONG ReqGTags[] = {
    (GTST_String), (ULONG)"init.ps", (GTST_MaxChars), 256, (TAG_DONE),
    (GTST_MaxChars), 256, (TAG_DONE),
    (GTST_MaxChars), 256, (TAG_DONE),
    (GTST_MaxChars), 256, (TAG_DONE),
    (GTST_MaxChars), 256, (TAG_DONE),
    (GTST_MaxChars), 256, (TAG_DONE),
    (TAG_DONE),
    (TAG_DONE),
    (GTMX_Labels), (ULONG)&Output0Labels[ 0 ], (TAG_DONE),
    (GTMX_Labels), (ULONG)&Colours0Labels[ 0 ], (TAG_DONE),
    (GTCY_Labels), (ULONG)&density0Labels[ 0 ], (TAG_DONE),
    (GTIN_Number), 618, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 876, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 0, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 0, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 75, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 75, (GTIN_MaxChars), 10, (TAG_DONE),
    (TAG_DONE),
    (GTIN_Number), 60000, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 20000, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 50000, (GTIN_MaxChars), 10, (TAG_DONE),
    (GTIN_Number), 10000, (GTIN_MaxChars), 10, (TAG_DONE),
    (TAG_DONE)
};

void ReqRender( void )
{
    UWORD        offx, offy;

    offx = ReqWnd->BorderLeft;
    offy = ReqWnd->BorderTop;

    PrintIText( ReqWnd->RPort, ReqIText, offx, offy );
}

int OpenReqWindow( struct Screen *scrn )
{
    struct NewGadget     ng;
    struct Gadget       *g;
    UWORD                lc, tc;
    UWORD                offx = scrn->WBorLeft,offy = scrn->WBorTop + scrn->RastPort.TxHeight + 1;

    ReqVisualInfo = GetVisualInfo(scrn, TAG_END);

    if ( ! ( g = CreateContext( &ReqGList )))
        return( 1L );

    for( lc = 0, tc = 0; lc < Req_CNT; lc++ ) {

        CopyMem((char * )&ReqNGad[ lc ], (char * )&ng, (long)sizeof( struct NewGadget ));

        ng.ng_VisualInfo = ReqVisualInfo;
        ng.ng_TextAttr   = &topaz11;
        ng.ng_LeftEdge  += offx;
        ng.ng_TopEdge   += offy;

        ReqGadgets[ lc ] = g = CreateGadgetA((ULONG)ReqGTypes[ lc ], g, &ng, ( struct TagItem * )&ReqGTags[ tc ] );

        while( ReqGTags[ tc ] ) tc += 2;
        tc++;

        if ( NOT g )
        return( 2L );
    }

    if ( ! ( ReqWnd = OpenWindowTags( NULL,
                    WA_Left,          ReqLeft,
                    WA_Top,           ReqTop,
                    WA_Width,         ReqWidth,
                    WA_Height,        ReqHeight + offy,
                    WA_IDCMP,         STRINGIDCMP|BUTTONIDCMP|MXIDCMP|CYCLEIDCMP|INTEGERIDCMP|CHECKBOXIDCMP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
                    WA_Flags,         WFLG_DRAGBAR|WFLG_SMART_REFRESH|WFLG_SIMPLE_REFRESH|WFLG_ACTIVATE,
                    WA_Gadgets,       ReqGList,
                    WA_Title,         ReqWdt,
                    WA_ScreenTitle,   "Post V1.86enh Copyright © 1989, 1992 Adrian Aylward",
                    WA_CustomScreen,  scrn,
                    TAG_DONE )))
        return( 4L );

    GT_RefreshWindow( ReqWnd, NULL );

    ReqRender();

    return( 0L );
}

void CloseReqWindow( void )
{
    if ( ReqWnd        ) {
        CloseWindow( ReqWnd );
        ReqWnd = NULL;
    }

    if ( ReqGList      ) {
        FreeGadgets( ReqGList );
        ReqGList = NULL;
    }

    FreeVisualInfo(ReqVisualInfo);
}

