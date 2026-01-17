/*
 *  Source generated with GadToolsBox V1.4
 *  which is (c) Copyright 1991,92 Jaba Development
 *  heavily modified by Robert Poole
 */

#define GD_startup1                            0
#define GD_startup2                            1
#define GD_startup3                            2
#define GD_startup4                            3
#define GD_startup5                            4
#define GD_iffname                             5
#define GD_okgad                               6
#define GD_cancelgad                           7
#define GD_Output                              8
#define GD_Colours                             9
#define GD_density                             10
#define GD_xsize                               11
#define GD_ysize                               12
#define GD_offx                                13
#define GD_offy                                14
#define GD_xdpi                                15
#define GD_ydpi                                16
#define GD_wbclose                             17
#define GD_fontmem                             18
#define GD_htonemem                            19
#define GD_VMem                                20
#define GD_pathmem                             21
#define GD_sunmouse                            22
#define GD_restart                             23

#define GDX_startup1                           0
#define GDX_startup2                           1
#define GDX_startup3                           2
#define GDX_startup4                           3
#define GDX_startup5                           4
#define GDX_iffname                            5
#define GDX_okgad                              6
#define GDX_cancelgad                          7
#define GDX_Output                             8
#define GDX_Colours                            9
#define GDX_density                            10
#define GDX_xsize                              11
#define GDX_ysize                              12
#define GDX_offx                               13
#define GDX_offy                               14
#define GDX_xdpi                               15
#define GDX_ydpi                               16
#define GDX_wbclose                            17
#define GDX_fontmem                            18
#define GDX_htonemem                           19
#define GDX_VMem                               20
#define GDX_pathmem                            21
#define GDX_sunmouse                           22

#define Req_CNT 23

extern APTR                  ReqVisualInfo;
extern struct Window        *ReqWnd;
extern struct Gadget        *ReqGList;
extern struct Gadget        *ReqGadgets[22];
extern UWORD                 ReqLeft;
extern UWORD                 ReqTop;
extern UWORD                 ReqWidth;
extern UWORD                 ReqHeight;
extern UBYTE                *ReqWdt;
extern UBYTE                *Output0Labels[];
extern UBYTE                *Colours0Labels[];
extern UBYTE                *density0Labels[];
extern struct TextAttr       topaz11;
extern struct IntuiText      ReqIText[];
extern UWORD                 ReqGTypes[];
extern struct NewGadget      ReqNGad[];
extern ULONG                 ReqGTags[];
extern UWORD                 DriPens[];

extern void ReqRender( void );
extern int OpenReqWindow( struct Screen * );
extern void CloseReqWindow( void );
