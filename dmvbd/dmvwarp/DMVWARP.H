/* DMVWARP.H - Schnittstelle eines After-Dark-1.x-Moduls (Windows 3.0):
   Meldungen, Rueckgabewerte und Parameterbloecke, wie sie After Dark und
   DMVBD erwarten. Eigene Beschreibung der Schnittstelle.
   Code: Claude, Prompts: rfka01 - MIT-Lizenz */

typedef struct {
    int     iOperatingMode;     /* 0 Real, 1 Standard, 2 Enhanced */
    int     iProcessorType;     /* 0 8086 ... 4 80486 */
    BOOL    bCoProcessor;
    POINT   ptScreenSize;
    int     iBitsPerPixel;
    POINT   ptAspect;
    POINT   ptDotsPerInch;
    int     iADVersion;
    RECT    rDemoRect;
    int     iSaverMode;         /* 0 Schoner, 1 Vorschau */
    HANDLE  hModuleInfo;        /* -> AD_MODULE */
    LPSTR   lpszErrorMessage;
    HWND    hADWnd;
    int     iRunner;
} AD_SYSTEM;

typedef struct {
    HRGN         hDrawRgn;
    POINT        ptRgnSize;
    int          iControlValue[4];
    int          iControlID[4];
    HANDLE       hModule;
    HPALETTE     hPalette;
    LPLOGPALETTE lpLogPalette;
} AD_MODULE;

typedef AD_SYSTEM FAR *LPSYSTEM;
typedef AD_MODULE FAR *LPMODULE;

#define INITIALIZE   0
#define BLANK        1
#define DRAWFRAME    2
#define CLOSE        3

#define NOERROR      0
#define MEM_ERROR    1
#define WAKE_UP      5

int FAR PASCAL Module(int msg, HDC hdc, HANDLE hSystem);
