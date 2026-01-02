// Copyright © 2014 Brook Hong. All Rights Reserved.
//

// msbuild /p:platform=win32 /p:Configuration=Release
// msbuild keycastow.vcxproj /t:Clean
// rc keycastow.rc && cl -DUNICODE -D_UNICODE keycast.cpp keylog.cpp
// keycastow.res user32.lib shell32.lib gdi32.lib Comdlg32.lib comctl32.lib

#include <stdio.h>
#include <windows.h>

#include <Commctrl.h>

#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

#include <windowsx.h>

#include <gdiplus.h>

using namespace Gdiplus;

#include "locale_manager.h"
#include "resource.h"
#include "timer.h"
CTimer showTimer;
CTimer previewTimer;

WCHAR iniFile[MAX_PATH];

#define MAXCHARS 4096
WCHAR textBuffer[MAXCHARS];
LPCWSTR textBufferEnd = textBuffer + MAXCHARS;

struct KeyLabel {
  RectF rect;
  WCHAR *text;
  DWORD length;
  int time;
  BOOL fade;
  KeyLabel() {
    text = textBuffer;
    length = 0;
  }
};

struct LabelSettings {
  DWORD keyStrokeDelay;
  DWORD lingerTime;
  DWORD fadeDuration;
  LOGFONT font;
  COLORREF bgColor, textColor, borderColor;
  DWORD bgOpacity, textOpacity, borderOpacity;
  int borderSize;
  int cornerSize;
};
LabelSettings labelSettings, previewLabelSettings;
DWORD labelSpacing;
BOOL visibleShift = FALSE;
BOOL visibleModifier = TRUE;
BOOL mouseCapturing = TRUE;
BOOL mouseCapturingMod = FALSE;
BOOL keyAutoRepeat = TRUE;
BOOL mergeMouseActions = TRUE;
int alignment = 1;
BOOL onlyCommandKeys = FALSE;
BOOL positioning = FALSE;
BOOL draggableLabel = FALSE;
UINT tcModifiers = MOD_ALT;
UINT tcKey = 0x42; // 0x42 is 'b'
Color clearColor(0, 127, 127, 127);
#define BRANDINGMAX 256
WCHAR branding[BRANDINGMAX];
WCHAR comboChars[4];
POINT deskOrigin;

#define MAXLABELS 60
KeyLabel keyLabels[MAXLABELS];
DWORD maximumLines = 10;
DWORD labelCount = 0;
RECT desktopRect;
SIZE canvasSize;
POINT canvasOrigin;

#include "keycast.h"
#include "keylog.h"

WCHAR *szWinName = L"KeyCastOW";
HWND hMainWnd;
HWND hDlgSettings;
RECT settingsDlgRect;
HWND hWndStamp;
HINSTANCE hInstance;
Graphics *gCanvas = NULL;
Font *fontPlus = NULL;

#define IDI_TRAY 100
#define WM_TRAYMSG 101
#define MENU_CONFIG 32
#define MENU_EXIT 33
#define MENU_RESTORE 34

void showText(LPCWSTR text, int behavior);

#ifdef _DEBUG
WCHAR capFile[MAX_PATH];
FILE *capStream = NULL;
WCHAR recordFN[MAX_PATH];
int replayStatus = 0;
#define MENU_REPLAY 35
struct Displayed {
  DWORD tm;
  int behavior;
  DWORD len;
  Displayed(DWORD t, int b, DWORD l) {
    tm = t;
    behavior = b;
    len = l;
  }
};
DWORD WINAPI replay(LPVOID ptr) {
  replayStatus = 1;
  FILE *stream;
  WCHAR tmp[256];
  errno_t err = _wfopen_s(&stream, (LPCWSTR)ptr, L"rb");
  Displayed dp(0, 0, 0);
  fread(&dp, sizeof(Displayed), 1, stream);
  fread(tmp, sizeof(WCHAR), dp.len, stream);
  showText(tmp, dp.behavior);
  DWORD lastTm = dp.tm;
  while (replayStatus == 1 && fread(&dp, sizeof(Displayed), 1, stream) == 1) {
    Sleep(dp.tm - lastTm);
    lastTm = dp.tm;
    fread(tmp, sizeof(WCHAR), dp.len, stream);
    tmp[dp.len] = '\0';
    showText(tmp, dp.behavior);
  }
  fclose(stream);
  replayStatus = 0;
  return 0;
}
#include <sstream>
WCHAR logFile[MAX_PATH];
FILE *logStream;
void log(const std::stringstream &line) {
  fprintf(logStream, "%s", line.str().c_str());
}
#endif
void stamp(HWND hwnd, LPCWSTR text) {
  RECT rt;
  GetWindowRect(hwnd, &rt);
  HDC hdc = GetDC(hwnd);
  HDC memDC = ::CreateCompatibleDC(hdc);
  HBITMAP memBitmap =
      ::CreateCompatibleBitmap(hdc, desktopRect.right - desktopRect.left,
                               desktopRect.bottom - desktopRect.top);
  HBITMAP hOldBitmap = (HBITMAP)::SelectObject(memDC, memBitmap);
  Graphics g(memDC);
  g.SetSmoothingMode(SmoothingModeAntiAlias);
  g.SetTextRenderingHint(TextRenderingHintAntiAlias);
  g.Clear(clearColor);

  RectF rc((REAL)labelSettings.borderSize, (REAL)labelSettings.borderSize, 0.0,
           0.0);
  SizeF stringSize, layoutSize((REAL)desktopRect.right - desktopRect.left -
                                   2 * labelSettings.borderSize,
                               (REAL)desktopRect.bottom - desktopRect.top -
                                   2 * labelSettings.borderSize);
  StringFormat format;
  format.SetAlignment(StringAlignmentCenter);
  g.MeasureString(text, wcslen(text), fontPlus, layoutSize, &format,
                  &stringSize);
  rc.Width = stringSize.Width;
  rc.Height = stringSize.Height;
  SIZE wndSize = {2 * labelSettings.borderSize + (LONG)rc.Width,
                  2 * labelSettings.borderSize + (LONG)rc.Height};
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, wndSize.cx, wndSize.cy,
               SWP_NOMOVE | SWP_NOACTIVATE);

  SolidBrush bgBrush(Color::Color(0xaf007cfe));
  g.FillRectangle(&bgBrush, rc);
  SolidBrush textBrushPlus(Color(0xaf303030));
  g.DrawString(text, wcslen(text), fontPlus, rc, &format, &textBrushPlus);
  SolidBrush brushPlus(Color::Color(0xaffefefe));
  rc.X += 2;
  rc.Y += 2;
  g.DrawString(text, wcslen(text), fontPlus, rc, &format, &brushPlus);

  POINT ptSrc = {0, 0};
  POINT ptDst = {rt.left, rt.top};
  BLENDFUNCTION blendFunction;
  blendFunction.AlphaFormat = AC_SRC_ALPHA;
  blendFunction.BlendFlags = 0;
  blendFunction.BlendOp = AC_SRC_OVER;
  blendFunction.SourceConstantAlpha = 255;
  ::UpdateLayeredWindow(hwnd, hdc, &ptDst, &wndSize, memDC, &ptSrc, 0,
                        &blendFunction, 2);
  ::SelectObject(memDC, hOldBitmap);
  ::DeleteObject(memBitmap);
  ::DeleteDC(memDC);
  ReleaseDC(hwnd, hdc);
}
void updateLayeredWindow(HWND hwnd) {
  POINT ptSrc = {0, 0};
  BLENDFUNCTION blendFunction;
  blendFunction.AlphaFormat = AC_SRC_ALPHA;
  blendFunction.BlendFlags = 0;
  blendFunction.BlendOp = AC_SRC_OVER;
  blendFunction.SourceConstantAlpha = 255;
  HDC hdcBuf = gCanvas->GetHDC();
  HDC hdc = GetDC(hwnd);
  ::UpdateLayeredWindow(hwnd, hdc, &canvasOrigin, &canvasSize, hdcBuf, &ptSrc,
                        0, &blendFunction, 2);
  ReleaseDC(hwnd, hdc);
  gCanvas->ReleaseHDC(hdcBuf);
}
void eraseLabel(int i) {
  RectF &rt = keyLabels[i].rect;
  RectF rc(rt.X - labelSettings.borderSize, rt.Y - labelSettings.borderSize,
           rt.Width + 2 * labelSettings.borderSize + 1,
           rt.Height + 2 * labelSettings.borderSize + 1);
  gCanvas->SetClip(rc);
  gCanvas->Clear(clearColor);
  gCanvas->ResetClip();
}
void drawLabelFrame(Graphics *g, const Pen *pen, const Brush *brush, RectF &rc,
                    REAL cornerSize) {
  if (cornerSize > 0) {
    GraphicsPath path;
    REAL dx = rc.Width - cornerSize, dy = rc.Height - cornerSize;
    path.AddArc(rc.X, rc.Y, cornerSize, cornerSize, 170, 90);
    path.AddArc(rc.X + dx, rc.Y, cornerSize, cornerSize, 270, 90);
    path.AddArc(rc.X + dx, rc.Y + dy, cornerSize, cornerSize, 0, 90);
    path.AddArc(rc.X, rc.Y + dy, cornerSize, cornerSize, 90, 90);
    path.CloseFigure();

    g->DrawPath(pen, &path);
    g->FillPath(brush, &path);
  } else {
    g->DrawRectangle(pen, rc.X, rc.Y, rc.Width, rc.Height);
    g->FillRectangle(brush, rc.X, rc.Y, rc.Width, rc.Height);
  }
}
#define BR(alpha, bgr)                                                         \
  (alpha << 24 | bgr >> 16 | (bgr & 0x0000ff00) | (bgr & 0x000000ff) << 16)
void updateLabel(int i) {
  eraseLabel(i);

  if (keyLabels[i].length > 0) {
    RectF &rc = keyLabels[i].rect;
    REAL r = 1.0f * keyLabels[i].time / labelSettings.fadeDuration;
    r = (r > 1.0f) ? 1.0f : r;
    PointF origin(rc.X, rc.Y);
    gCanvas->MeasureString(keyLabels[i].text, keyLabels[i].length, fontPlus,
                           origin, &rc);
    rc.Width = (rc.Width < labelSettings.cornerSize) ? labelSettings.cornerSize
                                                     : rc.Width;
    if (alignment) {
      rc.X = canvasSize.cx - rc.Width - labelSettings.borderSize;
    } else {
      rc.X = (REAL)labelSettings.borderSize;
    }
    rc.Height = (rc.Height < labelSettings.cornerSize)
                    ? labelSettings.cornerSize
                    : rc.Height;
    int bgAlpha = (int)(r * labelSettings.bgOpacity),
        textAlpha = (int)(r * labelSettings.textOpacity),
        borderAlpha = (int)(r * labelSettings.borderOpacity);
    Pen penPlus(Color::Color(BR(borderAlpha, labelSettings.borderColor)),
                labelSettings.borderSize + 0.0f);
    SolidBrush brushPlus(Color::Color(BR(bgAlpha, labelSettings.bgColor)));
    drawLabelFrame(gCanvas, &penPlus, &brushPlus, rc,
                   (REAL)labelSettings.cornerSize);
    SolidBrush textBrushPlus(Color(BR(textAlpha, labelSettings.textColor)));
    gCanvas->DrawString(keyLabels[i].text, keyLabels[i].length, fontPlus,
                        PointF(rc.X, rc.Y), &textBrushPlus);
  }
}

void fadeLastLabel(BOOL whether) { keyLabels[labelCount - 1].fade = whether; }

static int newStrokeCount = 0;
#define SHOWTIMER_INTERVAL 40
static int deferredTime;
WCHAR deferredLabel[64];

static void startFade() {
  if (newStrokeCount > 0) {
    newStrokeCount -= SHOWTIMER_INTERVAL;
  }
  DWORD i = 0;
  BOOL dirty = FALSE;

  if (wcslen(deferredLabel) > 0) {
    // update deferred label if it exists
    if (deferredTime > 0) {
      deferredTime -= SHOWTIMER_INTERVAL;
    } else {
      showText(deferredLabel, 1);
      fadeLastLabel(FALSE);
      deferredLabel[0] = '\0';
    }
  }

  for (i = 0; i < labelCount; i++) {
    RectF &rt = keyLabels[i].rect;
    if ((unsigned long)keyLabels[i].time > labelSettings.fadeDuration) {
      if (keyLabels[i].fade) {
        keyLabels[i].time -= SHOWTIMER_INTERVAL;
      }
    } else if (keyLabels[i].time >= SHOWTIMER_INTERVAL) {
      if (keyLabels[i].fade) {
        keyLabels[i].time -= SHOWTIMER_INTERVAL;
      }
      updateLabel(i);
      dirty = TRUE;
    } else {
      keyLabels[i].time = 0;
      if (keyLabels[i].length) {
        eraseLabel(i);
        // erase keyLabels[i].length times to avoid remaining shadow
        keyLabels[i].length--;
        dirty = TRUE;
      }
    }
  }
  if (dirty) {
    updateLayeredWindow(hMainWnd);
  }
}

void ensureSpace(WCHAR *dest, size_t len) {
  for (DWORD i = 0; i < labelCount - 1; i++) {
    if (keyLabels[i].length > 0) {
      WCHAR *start = keyLabels[i].text;
      WCHAR *end = start + keyLabels[i].length;
      if (start < dest + len && end > dest) {
        keyLabels[i].length = 0;
        keyLabels[i].time = 0;
        eraseLabel(i);
      }
    }
  }
}

bool outOfLine(LPCWSTR text) {
  size_t newLen = wcslen(text);
  if (keyLabels[labelCount - 1].text + keyLabels[labelCount - 1].length +
          newLen >=
      textBufferEnd) {
    ensureSpace(textBuffer, keyLabels[labelCount - 1].length);
    wcscpy_s(textBuffer, MAXCHARS, keyLabels[labelCount - 1].text);
    keyLabels[labelCount - 1].text = textBuffer;
  }
  LPWSTR tmp =
      keyLabels[labelCount - 1].text + keyLabels[labelCount - 1].length;
  ensureSpace(tmp, newLen);
  wcscpy_s(tmp, (textBufferEnd - tmp), text);
  RectF box;
  PointF origin(0, 0);
  gCanvas->MeasureString(keyLabels[labelCount - 1].text,
                         keyLabels[labelCount - 1].length, fontPlus, origin,
                         &box);
  int cx = (int)box.Width + 2 * labelSettings.cornerSize +
           labelSettings.borderSize * 2;
  bool out = cx >= canvasSize.cx;
  return out;
}
/*
 * behavior 0: append text to last label
 * behavior 1: create a new label with text
 * behavior 2: replace last label with text
 */
void showText(LPCWSTR text, int behavior = 0) {
  SetWindowPos(hMainWnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
  size_t newLen = wcslen(text);

#ifdef _DEBUG
  if (replayStatus == 0 && capStream) {
    Displayed dp(GetTickCount(), behavior, (DWORD)newLen);
    fwrite(&dp, sizeof(Displayed), 1, capStream);
    fwrite(text, sizeof(WCHAR), newLen, capStream);
    fflush(capStream);
  }
#endif

  DWORD i;
  if (behavior == 2) {
    ensureSpace(keyLabels[labelCount - 1].text, newLen);
    wcscpy_s(keyLabels[labelCount - 1].text,
             textBufferEnd - keyLabels[labelCount - 1].text, text);
    keyLabels[labelCount - 1].length = (DWORD)newLen;
  } else if (behavior == 3) {
    wcscpy_s(deferredLabel, 64, text);
    deferredTime = 120;
  } else if (behavior == 1 || (newStrokeCount <= 0) || outOfLine(text)) {
    for (i = 1; i < labelCount; i++) {
      if (keyLabels[i].time > 0) {
        break;
      }
    }
    for (; i < labelCount; i++) {
      eraseLabel(i - 1);
      keyLabels[i - 1].text = keyLabels[i].text;
      keyLabels[i - 1].length = keyLabels[i].length;
      keyLabels[i - 1].time = keyLabels[i].time;
      keyLabels[i - 1].rect.X = keyLabels[i].rect.X;
      keyLabels[i - 1].fade = TRUE;
      updateLabel(i - 1);
      eraseLabel(i);
    }
    if (labelCount > 1) {
      keyLabels[labelCount - 1].text =
          keyLabels[labelCount - 2].text + keyLabels[labelCount - 2].length;
    }
    if (keyLabels[labelCount - 1].text + newLen >= textBufferEnd) {
      keyLabels[labelCount - 1].text = textBuffer;
    }
    ensureSpace(keyLabels[labelCount - 1].text, newLen);
    wcscpy_s(keyLabels[labelCount - 1].text,
             textBufferEnd - keyLabels[labelCount - 1].text, text);
    keyLabels[labelCount - 1].length = (DWORD)newLen;
  } else {
    LPWSTR tmp =
        keyLabels[labelCount - 1].text + keyLabels[labelCount - 1].length;
    if (tmp + newLen >= textBufferEnd) {
      tmp = textBuffer;
      keyLabels[labelCount - 1].text = tmp;
      keyLabels[labelCount - 1].length = (DWORD)newLen;
    } else {
      keyLabels[labelCount - 1].length += (DWORD)newLen;
    }
    ensureSpace(tmp, newLen);
    wcscpy_s(tmp, (textBufferEnd - tmp), text);
  }
  keyLabels[labelCount - 1].time =
      labelSettings.lingerTime + labelSettings.fadeDuration;
  keyLabels[labelCount - 1].fade = TRUE;
  updateLabel(labelCount - 1);
  newStrokeCount = labelSettings.keyStrokeDelay;
  updateLayeredWindow(hMainWnd);
}

void updateCanvasSize(const POINT &pt) {
  for (DWORD i = 0; i < labelCount; i++) {
    if (keyLabels[i].time > 0) {
      eraseLabel(i);
      keyLabels[i].time = 0;
    }
  }
  canvasSize.cy = desktopRect.bottom - desktopRect.top;
  canvasOrigin.y = pt.y - desktopRect.bottom + desktopRect.top;
  canvasSize.cx = pt.x - desktopRect.left;
  canvasOrigin.x = desktopRect.left;

#ifdef _DEBUG
  std::stringstream line;
  line << "desktopRect: {left: " << desktopRect.left
       << ", top: " << desktopRect.top << ", right: " << desktopRect.right
       << ", bottom: " << desktopRect.bottom << "};\n";
  line << "canvasSize: {" << canvasSize.cx << "," << canvasSize.cy << "};\n";
  line << "canvasOrigin: {" << canvasOrigin.x << "," << canvasOrigin.y
       << "};\n";
  line << "pt: {" << pt.x << "," << pt.y << "};\n";
  log(line);
#endif
}

HDC g_hdcBuffer = NULL;
HBITMAP g_hbitmap = NULL;
HBITMAP g_hBitmapOld = NULL;
HFONT g_hlabelFont = NULL;

void createCanvas() {
  HDC hdc = GetDC(hMainWnd);
  if (gCanvas) {
    delete gCanvas;
    gCanvas = NULL;
  }
  if (g_hdcBuffer) {
    SelectObject(g_hdcBuffer, g_hBitmapOld);
    DeleteObject(g_hbitmap);
    DeleteDC(g_hdcBuffer);
    g_hdcBuffer = NULL;
    g_hbitmap = NULL;
  }

  g_hdcBuffer = CreateCompatibleDC(hdc);
  g_hbitmap = CreateCompatibleBitmap(hdc, desktopRect.right - desktopRect.left,
                                     desktopRect.bottom - desktopRect.top);
  g_hBitmapOld = (HBITMAP)SelectObject(g_hdcBuffer, (HGDIOBJ)g_hbitmap);
  ReleaseDC(hMainWnd, hdc);

  gCanvas = new Graphics(g_hdcBuffer);
  gCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
  gCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
}
void prepareLabels() {
  HDC hdc = GetDC(hMainWnd);
  if (fontPlus) {
    delete fontPlus;
    fontPlus = NULL;
  }
  if (g_hlabelFont) {
    DeleteObject(g_hlabelFont);
    g_hlabelFont = NULL;
  }

  g_hlabelFont = CreateFontIndirect(&labelSettings.font);
  HFONT hFontOld = (HFONT)SelectObject(hdc, g_hlabelFont);

  fontPlus = new Font(hdc, g_hlabelFont);
  SelectObject(hdc, hFontOld);
  ReleaseDC(hMainWnd, hdc);

  RectF box;
  PointF origin(0, 0);
  gCanvas->MeasureString(L"\u263b - KeyCastOW OFF", 16, fontPlus, origin, &box);
  REAL unitH = box.Height + 2 * labelSettings.borderSize + labelSpacing;
  labelCount = (desktopRect.bottom - desktopRect.top) / (int)unitH;
  REAL paddingH = (desktopRect.bottom - desktopRect.top) - unitH * labelCount;

  DWORD offset = 0;
  if (labelCount > maximumLines) {
    offset = labelCount - maximumLines;
    labelCount = maximumLines;
  } else if (labelCount == 0) {
    offset = labelCount - 1;
    labelCount = 1;
  }

  gCanvas->Clear(clearColor);
  for (DWORD i = 0; i < labelCount; i++) {
    keyLabels[i].rect.X = (REAL)labelSettings.borderSize;
    keyLabels[i].rect.Y = paddingH + unitH * (i + offset) + labelSpacing +
                          labelSettings.borderSize;
    if ((DWORD)keyLabels[i].time >
        labelSettings.lingerTime + labelSettings.fadeDuration) {
      keyLabels[i].time = labelSettings.lingerTime + labelSettings.fadeDuration;
    }
    if (keyLabels[i].time > 0) {
      updateLabel(i);
    }
  }

  stamp(hWndStamp, branding);
}

void GetWorkAreaByOrigin(const POINT &pt, MONITORINFO &mi) {
  RECT rc = {pt.x - 1, pt.y - 1, pt.x + 1, pt.y + 1};
  HMONITOR hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hMonitor, &mi);
}

void positionOrigin(int action, POINT &pt) {
  if (action == 0) {
    updateCanvasSize(pt);

    MONITORINFO mi;
    GetWorkAreaByOrigin(pt, mi);
    if (mi.rcWork.left != desktopRect.left ||
        mi.rcWork.top != desktopRect.top) {
      CopyMemory(&desktopRect, &mi.rcWork, sizeof(RECT));
      MoveWindow(hMainWnd, desktopRect.left, desktopRect.top, 1, 1, TRUE);
      createCanvas();
      prepareLabels();
    }
#ifdef _DEBUG
    std::stringstream line;
    line << "rcWork: {" << mi.rcWork.left << "," << mi.rcWork.top << ","
         << mi.rcWork.right << "," << mi.rcWork.bottom << "};\n";
    line << "desktopRect: {" << desktopRect.left << "," << desktopRect.top
         << "," << desktopRect.right << "," << desktopRect.bottom << "};\n";
    line << "canvasSize: {" << canvasSize.cx << "," << canvasSize.cy << "};\n";
    line << "canvasOrigin: {" << canvasOrigin.x << "," << canvasOrigin.y
         << "};\n";
    line << "labelCount: " << labelCount << "\n";
    log(line);
#endif
    WCHAR tmp[256];
    swprintf(tmp, 256, L"%d, %d", pt.x, pt.y);
    showText(tmp, 2);
  } else {
    positioning = FALSE;
    deskOrigin.x = pt.x;
    deskOrigin.y = pt.y;
    updateCanvasSize(pt);
    clearColor.SetValue(0x007f7f7f);
    gCanvas->Clear(clearColor);
  }
}
BOOL ColorDialog(HWND hWnd, COLORREF &clr) {
  DWORD dwCustClrs[16] = {
      RGB(0, 0, 0),       RGB(0, 0, 255),     RGB(0, 255, 0),
      RGB(128, 255, 255), RGB(255, 0, 0),     RGB(255, 0, 255),
      RGB(255, 255, 0),   RGB(192, 192, 192), RGB(127, 127, 127),
      RGB(0, 0, 128),     RGB(0, 128, 0),     RGB(0, 255, 255),
      RGB(128, 0, 0),     RGB(255, 0, 128),   RGB(128, 128, 64),
      RGB(255, 255, 255)};
  CHOOSECOLOR dlgColor;
  dlgColor.lStructSize = sizeof(CHOOSECOLOR);
  dlgColor.hwndOwner = hWnd;
  dlgColor.hInstance = NULL;
  dlgColor.lpTemplateName = NULL;
  dlgColor.rgbResult = clr;
  dlgColor.lpCustColors = dwCustClrs;
  dlgColor.Flags = CC_ANYCOLOR | CC_RGBINIT;
  dlgColor.lCustData = 0;
  dlgColor.lpfnHook = NULL;

  if (ChooseColor(&dlgColor)) {
    clr = dlgColor.rgbResult;
  }
  return TRUE;
}
HWND CreateToolTip(HWND hDlg, int toolID, LPCWSTR pszText) {
  // Get the window of the tool.
  HWND hwndTool = GetDlgItem(hDlg, toolID);

  // Create the tooltip. g_hInst is the global instance handle.
  HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
                                WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, hDlg, NULL, hInstance, NULL);

  if (!hwndTool || !hwndTip) {
    return (HWND)NULL;
  }

  // Associate the tooltip with the tool.
  TOOLINFO toolInfo = {0};
  toolInfo.cbSize = sizeof(toolInfo);
  toolInfo.hwnd = hDlg;
  toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  toolInfo.uId = (UINT_PTR)hwndTool;
  toolInfo.lpszText = (LPWSTR)pszText;
  SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

  return hwndTip;
}
void writeSettingInt(LPCTSTR lpKeyName, DWORD dw) {
  WCHAR tmp[256];
  swprintf(tmp, 256, L"%d", dw);
  WritePrivateProfileString(L"KeyCastOW", lpKeyName, tmp, iniFile);
}
void saveSettings() {
  writeSettingInt(L"keyStrokeDelay", labelSettings.keyStrokeDelay);
  writeSettingInt(L"lingerTime", labelSettings.lingerTime);
  writeSettingInt(L"fadeDuration", labelSettings.fadeDuration);
  writeSettingInt(L"bgColor", labelSettings.bgColor);
  writeSettingInt(L"textColor", labelSettings.textColor);
  WritePrivateProfileStruct(L"KeyCastOW", L"labelFont",
                            (LPVOID)&labelSettings.font,
                            sizeof(labelSettings.font), iniFile);
  writeSettingInt(L"bgOpacity", labelSettings.bgOpacity);
  writeSettingInt(L"textOpacity", labelSettings.textOpacity);
  writeSettingInt(L"borderOpacity", labelSettings.borderOpacity);
  writeSettingInt(L"borderColor", labelSettings.borderColor);
  writeSettingInt(L"borderSize", labelSettings.borderSize);
  writeSettingInt(L"cornerSize", labelSettings.cornerSize);
  writeSettingInt(L"labelSpacing", labelSpacing);
  writeSettingInt(L"maximumLines", maximumLines);
  writeSettingInt(L"offsetX", deskOrigin.x);
  writeSettingInt(L"offsetY", deskOrigin.y);
  writeSettingInt(L"visibleShift", visibleShift);
  writeSettingInt(L"visibleModifier", visibleModifier);
  writeSettingInt(L"mouseCapturing", mouseCapturing);
  writeSettingInt(L"mouseCapturingMod", mouseCapturingMod);
  writeSettingInt(L"keyAutoRepeat", keyAutoRepeat);
  writeSettingInt(L"mergeMouseActions", mergeMouseActions);
  writeSettingInt(L"alignment", alignment);
  writeSettingInt(L"onlyCommandKeys", onlyCommandKeys);
  writeSettingInt(L"draggableLabel", draggableLabel);
  if (draggableLabel) {
    SetWindowLong(hMainWnd, GWL_EXSTYLE,
                  GetWindowLong(hMainWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
  } else {
    SetWindowLong(hMainWnd, GWL_EXSTYLE,
                  GetWindowLong(hMainWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
  }
  writeSettingInt(L"tcModifiers", tcModifiers);
  writeSettingInt(L"tcKey", tcKey);
  WritePrivateProfileString(L"KeyCastOW", L"branding", branding, iniFile);
  WritePrivateProfileString(L"KeyCastOW", L"comboChars", comboChars, iniFile);
}
void fixDeskOrigin() {
  if (deskOrigin.x > desktopRect.right ||
      deskOrigin.x < desktopRect.left + labelSettings.borderSize) {
    deskOrigin.x = desktopRect.right - labelSettings.borderSize;
  }
  if (deskOrigin.y > desktopRect.bottom ||
      deskOrigin.y < desktopRect.top + labelSettings.borderSize) {
    deskOrigin.y = desktopRect.bottom;
  }
}
void loadSettings() {
  labelSettings.keyStrokeDelay =
      GetPrivateProfileInt(L"KeyCastOW", L"keyStrokeDelay", 500, iniFile);
  labelSettings.lingerTime =
      GetPrivateProfileInt(L"KeyCastOW", L"lingerTime", 1200, iniFile);
  labelSettings.fadeDuration =
      GetPrivateProfileInt(L"KeyCastOW", L"fadeDuration", 310, iniFile);
  labelSettings.bgColor =
      GetPrivateProfileInt(L"KeyCastOW", L"bgColor", RGB(75, 75, 75), iniFile);
  labelSettings.textColor = GetPrivateProfileInt(L"KeyCastOW", L"textColor",
                                                 RGB(255, 255, 255), iniFile);
  labelSettings.bgOpacity =
      GetPrivateProfileInt(L"KeyCastOW", L"bgOpacity", 200, iniFile);
  labelSettings.textOpacity =
      GetPrivateProfileInt(L"KeyCastOW", L"textOpacity", 255, iniFile);
  labelSettings.borderOpacity =
      GetPrivateProfileInt(L"KeyCastOW", L"borderOpacity", 200, iniFile);
  labelSettings.borderColor = GetPrivateProfileInt(L"KeyCastOW", L"borderColor",
                                                   RGB(0, 128, 255), iniFile);
  labelSettings.borderSize =
      GetPrivateProfileInt(L"KeyCastOW", L"borderSize", 8, iniFile);
  labelSettings.cornerSize =
      GetPrivateProfileInt(L"KeyCastOW", L"cornerSize", 2, iniFile);
  labelSpacing =
      GetPrivateProfileInt(L"KeyCastOW", L"labelSpacing", 1, iniFile);
  maximumLines =
      GetPrivateProfileInt(L"KeyCastOW", L"maximumLines", 10, iniFile);
  if (maximumLines == 0) {
    maximumLines = 1;
  }
  deskOrigin.x = GetPrivateProfileInt(L"KeyCastOW", L"offsetX", 2, iniFile);
  deskOrigin.y = GetPrivateProfileInt(L"KeyCastOW", L"offsetY", 2, iniFile);
  MONITORINFO mi;
  GetWorkAreaByOrigin(deskOrigin, mi);
  CopyMemory(&desktopRect, &mi.rcWork, sizeof(RECT));
  MoveWindow(hMainWnd, desktopRect.left, desktopRect.top, 1, 1, TRUE);
  fixDeskOrigin();
  visibleShift =
      GetPrivateProfileInt(L"KeyCastOW", L"visibleShift", 0, iniFile);
  visibleModifier =
      GetPrivateProfileInt(L"KeyCastOW", L"visibleModifier", 1, iniFile);
  mouseCapturing =
      GetPrivateProfileInt(L"KeyCastOW", L"mouseCapturing", 1, iniFile);
  mouseCapturingMod =
      GetPrivateProfileInt(L"KeyCastOW", L"mouseCapturingMod", 0, iniFile);
  keyAutoRepeat =
      GetPrivateProfileInt(L"KeyCastOW", L"keyAutoRepeat", 1, iniFile);
  mergeMouseActions =
      GetPrivateProfileInt(L"KeyCastOW", L"mergeMouseActions", 1, iniFile);
  alignment = GetPrivateProfileInt(L"KeyCastOW", L"alignment", 1, iniFile);
  onlyCommandKeys =
      GetPrivateProfileInt(L"KeyCastOW", L"onlyCommandKeys", 0, iniFile);
  draggableLabel =
      GetPrivateProfileInt(L"KeyCastOW", L"draggableLabel", 0, iniFile);
  if (draggableLabel) {
    SetWindowLong(hMainWnd, GWL_EXSTYLE,
                  GetWindowLong(hMainWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
  } else {
    SetWindowLong(hMainWnd, GWL_EXSTYLE,
                  GetWindowLong(hMainWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
  }
  tcModifiers =
      GetPrivateProfileInt(L"KeyCastOW", L"tcModifiers", MOD_ALT, iniFile);
  tcKey = GetPrivateProfileInt(L"KeyCastOW", L"tcKey", 0x42, iniFile);
  GetPrivateProfileString(
      L"KeyCastOW", L"branding",
      L"Hi there, press any key to try, double click to configure.", branding,
      BRANDINGMAX, iniFile);
  GetPrivateProfileString(L"KeyCastOW", L"comboChars", L"<->", comboChars, 4,
                          iniFile);
  memset(&labelSettings.font, 0, sizeof(labelSettings.font));
  labelSettings.font.lfCharSet = DEFAULT_CHARSET;
  labelSettings.font.lfHeight = -37;
  labelSettings.font.lfPitchAndFamily = DEFAULT_PITCH;
  labelSettings.font.lfWeight = FW_BLACK;
  labelSettings.font.lfOutPrecision = OUT_DEFAULT_PRECIS;
  labelSettings.font.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  labelSettings.font.lfQuality = ANTIALIASED_QUALITY;
  wcscpy_s(labelSettings.font.lfFaceName, LF_FACESIZE, TEXT("Arial Black"));
  GetPrivateProfileStruct(L"KeyCastOW", L"labelFont", &labelSettings.font,
                          sizeof(labelSettings.font), iniFile);
}
void renderSettingsData(HWND hwndDlg) {
  WCHAR tmp[256];
  swprintf(tmp, 256, L"%d", previewLabelSettings.keyStrokeDelay);
  SetDlgItemText(hwndDlg, IDC_KEYSTROKEDELAY, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.lingerTime);
  SetDlgItemText(hwndDlg, IDC_LINGERTIME, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.fadeDuration);
  SetDlgItemText(hwndDlg, IDC_FADEDURATION, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.bgOpacity);
  SetDlgItemText(hwndDlg, IDC_BGOPACITY, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.textOpacity);
  SetDlgItemText(hwndDlg, IDC_TEXTOPACITY, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.borderOpacity);
  SetDlgItemText(hwndDlg, IDC_BORDEROPACITY, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.borderSize);
  SetDlgItemText(hwndDlg, IDC_BORDERSIZE, tmp);
  swprintf(tmp, 256, L"%d", previewLabelSettings.cornerSize);
  SetDlgItemText(hwndDlg, IDC_CORNERSIZE, tmp);

  swprintf(tmp, 256, L"%d", labelSpacing);
  SetDlgItemText(hwndDlg, IDC_LABELSPACING, tmp);
  swprintf(tmp, 256, L"%d", maximumLines);
  SetDlgItemText(hwndDlg, IDC_MAXIMUMLINES, tmp);
  SetDlgItemText(hwndDlg, IDC_BRANDING, branding);
  SetDlgItemText(hwndDlg, IDC_COMBSCHEME, comboChars);
  CheckDlgButton(hwndDlg, IDC_VISIBLESHIFT,
                 visibleShift ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_VISIBLEMODIFIER,
                 visibleModifier ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MOUSECAPTURING,
                 mouseCapturing ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MOUSECAPTURINGMOD,
                 mouseCapturingMod ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_KEYAUTOREPEAT,
                 keyAutoRepeat ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MERGEMOUSEACTIONS,
                 mergeMouseActions ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_ONLYCOMMANDKEYS,
                 onlyCommandKeys ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_DRAGGABLELABEL,
                 draggableLabel ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MODCTRL,
                 (tcModifiers & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MODALT,
                 (tcModifiers & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MODSHIFT,
                 (tcModifiers & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwndDlg, IDC_MODWIN,
                 (tcModifiers & MOD_WIN) ? BST_CHECKED : BST_UNCHECKED);
  swprintf(tmp, 256, L"%c", MapVirtualKey(tcKey, MAPVK_VK_TO_CHAR));
  SetDlgItemText(hwndDlg, IDC_TCKEY, tmp);
  ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_ALIGNMENT), alignment);
}
void getLabelSettings(HWND hwndDlg, LabelSettings &lblSettings) {
  WCHAR tmp[256];
  GetDlgItemText(hwndDlg, IDC_KEYSTROKEDELAY, tmp, 256);
  lblSettings.keyStrokeDelay = _wtoi(tmp);
  GetDlgItemText(hwndDlg, IDC_LINGERTIME, tmp, 256);
  lblSettings.lingerTime = _wtoi(tmp);
  GetDlgItemText(hwndDlg, IDC_FADEDURATION, tmp, 256);
  lblSettings.fadeDuration = _wtoi(tmp);
  if (lblSettings.fadeDuration < SHOWTIMER_INTERVAL * 5) {
    lblSettings.fadeDuration = SHOWTIMER_INTERVAL * 5;
  }
  GetDlgItemText(hwndDlg, IDC_BGOPACITY, tmp, 256);
  lblSettings.bgOpacity = _wtoi(tmp);
  lblSettings.bgOpacity = min(lblSettings.bgOpacity, 255);
  GetDlgItemText(hwndDlg, IDC_TEXTOPACITY, tmp, 256);
  lblSettings.textOpacity = _wtoi(tmp);
  lblSettings.textOpacity = min(lblSettings.textOpacity, 255);
  GetDlgItemText(hwndDlg, IDC_BORDEROPACITY, tmp, 256);
  lblSettings.borderOpacity = _wtoi(tmp);
  lblSettings.borderOpacity = min(lblSettings.borderOpacity, 255);
  GetDlgItemText(hwndDlg, IDC_BORDERSIZE, tmp, 256);
  lblSettings.borderSize = _wtoi(tmp);
  GetDlgItemText(hwndDlg, IDC_CORNERSIZE, tmp, 256);
  lblSettings.cornerSize = _wtoi(tmp);
}
DWORD previewTime = 0;
#define PREVIEWTIMER_INTERVAL 5
static void previewLabel() {
  RECT rt = {12, 58, 222, 238};

  getLabelSettings(hDlgSettings, previewLabelSettings);
  DWORD mg =
      previewLabelSettings.lingerTime + previewLabelSettings.fadeDuration + 600;
  double r;
  if (previewTime < PREVIEWTIMER_INTERVAL || previewTime > mg) {
    previewTime = mg;
  }
  if (previewTime > mg - 600) {
    previewTime -= PREVIEWTIMER_INTERVAL;
    r = 0;
  } else if (previewTime > previewLabelSettings.fadeDuration) {
    r = 1;
    previewTime -= PREVIEWTIMER_INTERVAL;
  } else if (previewTime >= PREVIEWTIMER_INTERVAL) {
    previewTime -= PREVIEWTIMER_INTERVAL;
    r = 1.0 * previewTime / previewLabelSettings.fadeDuration;
  }
  HDC hdc = GetDC(hDlgSettings);
  int rtWidth = rt.right - rt.left;
  int rtHeight = rt.bottom - rt.top;
  RectF rc(0, 0, (REAL)rtWidth, (REAL)rtHeight);
  HDC memDC = ::CreateCompatibleDC(hdc);
  HBITMAP memBitmap =
      ::CreateCompatibleBitmap(hdc, (int)rc.Width, (int)rc.Height);
  ::SelectObject(memDC, memBitmap);
  Graphics g(memDC);
  g.SetSmoothingMode(SmoothingModeAntiAlias);
  g.SetTextRenderingHint(TextRenderingHintAntiAlias);

  WCHAR text[] = L"BH";
  HFONT hFont = CreateFontIndirect(&previewLabelSettings.font);
  SelectObject(memDC, hFont);
  Font font(memDC, hFont);

  PointF origin(rc.X + previewLabelSettings.borderSize,
                rc.Y + previewLabelSettings.borderSize);
  g.MeasureString(text, 2, &font, origin, &rc);

  rc.X += (rtWidth - (int)rc.Width) / 2 - previewLabelSettings.borderSize;
  rc.Y += (rtHeight - (int)rc.Height) / 2 - previewLabelSettings.borderSize;
  origin.X = rc.X;
  origin.Y = rc.Y;

  int bgAlpha = (int)(r * previewLabelSettings.bgOpacity),
      textAlpha = (int)(r * previewLabelSettings.textOpacity),
      borderAlpha = (int)(r * previewLabelSettings.borderOpacity);
  Pen penPlus(Color::Color(BR(borderAlpha, previewLabelSettings.borderColor)),
              previewLabelSettings.borderSize + 0.0f);
  SolidBrush brushPlus(Color::Color(BR(bgAlpha, previewLabelSettings.bgColor)));
  drawLabelFrame(&g, &penPlus, &brushPlus, rc,
                 (REAL)previewLabelSettings.cornerSize);
  SolidBrush textBrushPlus(
      Color(BR(textAlpha, previewLabelSettings.textColor)));
  g.DrawString(text, wcslen(text), &font, origin, &textBrushPlus);
  BitBlt(hdc, rt.left, rt.top, rtWidth, rtHeight, memDC, 0, 0, SRCCOPY);
  DeleteDC(memDC);
  DeleteObject(memBitmap);
  DeleteObject(hFont);
  ReleaseDC(hDlgSettings, hdc);
}

BOOL CALLBACK SettingsWndProc(HWND hwndDlg, UINT msg, WPARAM wParam,
                              LPARAM lParam) {
  WCHAR tmp[256];
  switch (msg) {
  case WM_INITDIALOG: {
    renderSettingsData(hwndDlg);

    SetDlgItemText(hwndDlg, IDOK, I18N(L"Settings", L"Save", L"Save").c_str());
    SetDlgItemText(hwndDlg, IDCANCEL,
                   I18N(L"Settings", L"Cancel", L"Cancel").c_str());
    SetDlgItemText(hwndDlg, IDC_BGCOLOR,
                   I18N(L"Settings", L"ChooseColor", L"Choose Color").c_str());
    SetDlgItemText(hwndDlg, IDC_TEXTCOLOR,
                   I18N(L"Settings", L"ChooseColor", L"Choose Color").c_str());
    SetDlgItemText(hwndDlg, IDC_BORDERCOLOR,
                   I18N(L"Settings", L"ChooseColor", L"Choose Color").c_str());
    SetDlgItemText(hwndDlg, IDC_TEXTFONT,
                   I18N(L"Settings", L"ChooseFont", L"Choose Font").c_str());
    SetDlgItemText(
        hwndDlg, IDC_GROUP_TOGGLECAPTURING,
        I18N(L"Settings", L"ToggleCapturing", L"Toggle Capturing").c_str());

    SetDlgItemText(
        hwndDlg, IDC_STATIC_KEYSTROKEDELAY,
        I18N(L"Settings", L"KeystrokeDelay", L"Keystroke Delay(ms):").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_LINGERTIME,
        I18N(L"Settings", L"LingerTime", L"Linger Time(ms):").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_FADEDURATION,
        I18N(L"Settings", L"FadeDuration", L"Fade Duration(ms):").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_TEXTFONT,
                   I18N(L"Settings", L"TextFont", L"Text Font:").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_BGCOLOR,
        I18N(L"Settings", L"BackgroundColor", L"Background Color:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_TEXTCOLOR,
                   I18N(L"Settings", L"TextColor", L"Text Color:").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_TEXTOPACITY,
        I18N(L"Settings", L"TextOpacity", L"Text Opacity(0-255):").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_MAXIMUMLINES,
        I18N(L"Settings", L"MaximumLines", L"Maximum Lines:").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_COMBSCHEME,
        I18N(L"Settings", L"CombinationChars", L"Combination Chars:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_VISIBLEMODIFIER,
                   I18N(L"Settings", L"DisplayStandaloneModifierKey",
                        L"Display Standalone Modifier Key:")
                       .c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_VISIBLESHIFT,
        I18N(L"Settings", L"ShiftAsModifierKey", L"SHIFT as Modifier Key:")
            .c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_ONLYCOMMANDKEYS,
        I18N(L"Settings", L"OnlyCommandKeys", L"Only Command Keys:").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_KEYAUTOREPEAT,
        I18N(L"Settings", L"HoldDownToRepeat", L"Hold Down to Repeat:")
            .c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_BORDERSIZE,
                   I18N(L"Settings", L"BorderSize", L"Border Size:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_BORDERCOLOR,
                   I18N(L"Settings", L"BorderColor", L"Border Color:").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_BORDEROPACITY,
        I18N(L"Settings", L"BorderOpacity", L"Border Opacity(0-255):").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_LABELSPACING,
        I18N(L"Settings", L"LabelSpacing", L"Label Spacing:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_CORNERSIZE,
                   I18N(L"Settings", L"CornerSize", L"Corner Size:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_MOUSECAPTURING,
                   I18N(L"Settings", L"MouseAction", L"Mouse Action:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_MOUSECAPTURINGMOD,
                   I18N(L"Settings", L"MouseOnlyWithModifier",
                        L"Mouse Only With Modifier:")
                       .c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_MERGEMOUSEACTIONS,
        I18N(L"Settings", L"DetectClickDblClick", L"Detect Click/DblClick:")
            .c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_BGOPACITY,
        I18N(L"Settings", L"BackgroundOpacity", L"Background Opacity(0-255):")
            .c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_BRANDING,
                   I18N(L"Settings", L"Branding", L"Branding:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_ALIGNMENT,
                   I18N(L"Settings", L"Alignment", L"Alignment:").c_str());
    SetDlgItemText(hwndDlg, IDC_STATIC_POSITION,
                   I18N(L"Settings", L"Position", L"Position:").c_str());
    SetDlgItemText(
        hwndDlg, IDC_STATIC_DRAGGABLELABEL,
        I18N(L"Settings", L"DraggableLabel", L"Draggable Label:").c_str());

    GetWindowRect(hwndDlg, &settingsDlgRect);
    SetWindowPos(hwndDlg, 0,
                 desktopRect.right - desktopRect.left - settingsDlgRect.right +
                     settingsDlgRect.left,
                 desktopRect.bottom - desktopRect.top - settingsDlgRect.bottom +
                     settingsDlgRect.top,
                 0, 0, SWP_NOSIZE);
    GetWindowRect(hwndDlg, &settingsDlgRect);
    CreateToolTip(hwndDlg, IDC_COMBSCHEME,
                  I18N(L"Settings", L"CombinationKeyTooltip",
                       L"[+] to display combination keys like [Alt + Tab].")
                      .c_str());
    HWND hCtrl = GetDlgItem(hwndDlg, IDC_ALIGNMENT);
    ComboBox_InsertString(hCtrl, 0,
                          I18N(L"Settings", L"AlignmentLeft", L"Left").c_str());
    ComboBox_InsertString(
        hCtrl, 1, I18N(L"Settings", L"AlignmentRight", L"Right").c_str());
  }
    return TRUE;
  case WM_NOTIFY:
    switch (((LPNMHDR)lParam)->code) {

    case NM_CLICK: // Fall through to the next case.
    case NM_RETURN: {
      PNMLINK pNMLink = (PNMLINK)lParam;
      LITEM item = pNMLink->item;
      ShellExecute(NULL, L"open", item.szUrl, NULL, NULL, SW_SHOW);
      break;
    }
    }

    break;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_TEXTFONT: {
      CHOOSEFONT cf;
      cf.lStructSize = sizeof(CHOOSEFONT);
      cf.hwndOwner = hwndDlg;
      cf.hDC = NULL;
      cf.lpLogFont = &previewLabelSettings.font;
      cf.iPointSize = 0;
      cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
      cf.rgbColors = 0;
      cf.lCustData = 0;
      cf.lpfnHook = NULL;
      cf.lpTemplateName = NULL;
      cf.hInstance = NULL;
      cf.lpszStyle = NULL;
      cf.nFontType = 0; // Returned from ChooseFont
      cf.nSizeMin = 0;
      cf.nSizeMax = 0;

      if (ChooseFont(&cf)) {
        prepareLabels();
        saveSettings();
      }
    }
      return TRUE;
    case IDC_TEXTCOLOR:
      if (ColorDialog(hwndDlg, previewLabelSettings.textColor)) {
        prepareLabels();
        saveSettings();
      }
      return TRUE;
    case IDC_BGCOLOR:
      if (ColorDialog(hwndDlg, previewLabelSettings.bgColor)) {
        prepareLabels();
        saveSettings();
      }
      return TRUE;
    case IDC_BORDERCOLOR:
      if (ColorDialog(hwndDlg, previewLabelSettings.borderColor)) {
        prepareLabels();
        saveSettings();
      }
      return TRUE;
    case IDC_POSITION: {
      alignment = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_ALIGNMENT));
      clearColor.SetValue(0x7f7f7f7f);
      gCanvas->Clear(clearColor);
      showText(L"\u254b", 1);
      fadeLastLabel(FALSE);
      positioning = TRUE;
    }
      return TRUE;
    case IDOK:
      labelSettings = previewLabelSettings;
      GetDlgItemText(hwndDlg, IDC_LABELSPACING, tmp, 256);
      labelSpacing = _wtoi(tmp);
      if (labelSpacing > (DWORD)(desktopRect.bottom - desktopRect.top) / 3) {
        labelSpacing = (DWORD)(desktopRect.bottom - desktopRect.top) / 3;
      }
      GetDlgItemText(hwndDlg, IDC_MAXIMUMLINES, tmp, 256);
      maximumLines = _wtoi(tmp);
      if (maximumLines > MAXLABELS) {
        maximumLines = MAXLABELS;
      } else if (maximumLines == 0) {
        maximumLines = 1;
      }
      GetDlgItemText(hwndDlg, IDC_BRANDING, branding, BRANDINGMAX);
      GetDlgItemText(hwndDlg, IDC_COMBSCHEME, comboChars, 4);
      visibleShift =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_VISIBLESHIFT));
      visibleModifier =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_VISIBLEMODIFIER));
      mouseCapturing =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MOUSECAPTURING));
      mouseCapturingMod =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MOUSECAPTURINGMOD));
      keyAutoRepeat =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_KEYAUTOREPEAT));
      mergeMouseActions =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MERGEMOUSEACTIONS));
      onlyCommandKeys =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_ONLYCOMMANDKEYS));
      draggableLabel =
          (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_DRAGGABLELABEL));
      tcModifiers = 0;
      if (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MODCTRL)) {
        tcModifiers |= MOD_CONTROL;
      }
      if (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MODALT)) {
        tcModifiers |= MOD_ALT;
      }
      if (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MODSHIFT)) {
        tcModifiers |= MOD_SHIFT;
      }
      if (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_MODWIN)) {
        tcModifiers |= MOD_WIN;
      }
      GetDlgItemText(hwndDlg, IDC_TCKEY, tmp, 256);
      alignment = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_ALIGNMENT));
      if (tcModifiers != 0 && tmp[0] != '\0') {
        tcKey = VkKeyScanEx(tmp[0], GetKeyboardLayout(0));
        UnregisterHotKey(NULL, 1);
        if (!RegisterHotKey(NULL, 1, tcModifiers | MOD_NOREPEAT, tcKey)) {
          MessageBox(
              NULL,
              I18N(L"Messages", L"UnableToRegisterHotkey",
                   L"Unable to register hotkey, you probably need go to "
                   L"settings to redefine your hotkey for toggle capturing.")
                  .c_str(),
              I18N(L"Messages", L"Warning", L"Warning").c_str(),
              MB_OK | MB_ICONWARNING);
        }
      }
      prepareLabels();
      saveSettings();
    case IDCANCEL:
      EndDialog(hwndDlg, wParam);
      previewTimer.Stop();
      return TRUE;
    }
  }
  return FALSE;
}
LRESULT CALLBACK DraggableWndProc(HWND hWnd, UINT message, WPARAM wParam,
                                  LPARAM lParam) {
  static POINT s_last_mouse;
  switch (message) {
  // hold mouse to move
  case WM_LBUTTONDOWN:
    SetCapture(hWnd);
    GetCursorPos(&s_last_mouse);
    showTimer.Stop();
    break;
  case WM_MOUSEMOVE:
    if (GetCapture() == hWnd) {
      POINT p;
      GetCursorPos(&p);
      int dx = p.x - s_last_mouse.x;
      int dy = p.y - s_last_mouse.y;
      if (dx || dy) {
        s_last_mouse = p;
        RECT r;
        GetWindowRect(hWnd, &r);
        SetWindowPos(hWnd, HWND_TOPMOST, r.left + dx, r.top + dy, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
      }
    }
    break;
  case WM_LBUTTONUP:
    ReleaseCapture();
    showTimer.Start(SHOWTIMER_INTERVAL);
    break;
  case WM_LBUTTONDBLCLK:
    SendMessage(hMainWnd, WM_COMMAND, MENU_CONFIG, 0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

LRESULT CALLBACK WindowFunc(HWND hWnd, UINT message, WPARAM wParam,
                            LPARAM lParam) {
  static POINT s_last_mouse;
  static HMENU hPopMenu;
  static NOTIFYICONDATA nid;

  switch (message) {
  // trayicon
  case WM_CREATE: {
    memset(&nid, 0, sizeof(nid));

    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYMSG;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    lstrcpy(nid.szTip,
            I18N(L"Messages", L"TrayTip", L"KeyCast On Windows by brook hong")
                .c_str());
    Shell_NotifyIcon(NIM_ADD, &nid);

    hPopMenu = CreatePopupMenu();
    // AppendMenu( hPopMenu, MF_STRING, MENU_CONFIG,  L"&Settings..." );
    AppendMenu(hPopMenu, MF_STRING, MENU_CONFIG,
               I18N(L"Menu", L"Settings", L"&Settings...").c_str());
    // AppendMenu( hPopMenu, MF_STRING, MENU_RESTORE,  L"&Restore default
    // settings" );
    AppendMenu(
        hPopMenu, MF_STRING, MENU_RESTORE,
        I18N(L"Menu", L"RestoreDefaultSettings", L"&Restore default settings")
            .c_str());
    // TODO: 添加语言切换按钮
    // AppendMenu( hPopMenu, MF_STRING, MENU_LANGUAGES,  L"&Languages" );
#ifdef _DEBUG
    AppendMenu(hPopMenu, MF_STRING, MENU_REPLAY, L"Re&play");
#endif
    // AppendMenu( hPopMenu, MF_STRING, MENU_EXIT,    L"E&xit" );
    AppendMenu(hPopMenu, MF_STRING, MENU_EXIT,
               I18N(L"Menu", L"Exit", L"E&xit").c_str());
    SetMenuDefaultItem(hPopMenu, MENU_CONFIG, FALSE);
  } break;
  case WM_TRAYMSG: {
    switch (lParam) {
    case WM_RBUTTONUP: {
      POINT pnt;
      GetCursorPos(&pnt);
      SetForegroundWindow(hWnd); // needed to get keyboard focus
      TrackPopupMenu(hPopMenu, TPM_LEFTALIGN, pnt.x, pnt.y, 0, hWnd, NULL);
    } break;
    case WM_LBUTTONDBLCLK:
      SendMessage(hWnd, WM_COMMAND, MENU_CONFIG, 0);
      return 0;
    }
  } break;
  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case MENU_CONFIG:
      CopyMemory(&previewLabelSettings, &labelSettings,
                 sizeof(previewLabelSettings));
      renderSettingsData(hDlgSettings);
      ShowWindow(hDlgSettings, SW_SHOW);
      SetForegroundWindow(hDlgSettings);
      previewTimer.Start(PREVIEWTIMER_INTERVAL);
      break;
    case MENU_RESTORE:
      DeleteFile(iniFile);
      loadSettings();
      updateCanvasSize(deskOrigin);
      createCanvas();
      prepareLabels();
      break;
#ifdef _DEBUG
    case MENU_REPLAY: {
      if (replayStatus == 1) {
        replayStatus = 2;
        ModifyMenu(hPopMenu, MENU_REPLAY, MF_STRING, MENU_REPLAY, L"Re&play");
      } else {
        OPENFILENAME ofn;
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.hInstance = hInstance;
        ofn.lpstrFile = recordFN;
        ofn.nMaxFile = sizeof(recordFN);
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
        if (GetOpenFileName(&ofn)) {
          unsigned long id = 1;
          CreateThread(NULL, 0, replay, recordFN, 0, &id);
          ModifyMenu(hPopMenu, MENU_REPLAY, MF_STRING, MENU_REPLAY,
                     L"Stop re&play");
        }
      }
    } break;
#endif
    case MENU_EXIT:
      Shell_NotifyIcon(NIM_DELETE, &nid);
      ExitProcess(0);
      break;
    default:
      break;
    }
  } break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  case WM_DISPLAYCHANGE: {
    MONITORINFO mi;
    GetWorkAreaByOrigin(deskOrigin, mi);
    CopyMemory(&desktopRect, &mi.rcWork, sizeof(RECT));
    MoveWindow(hMainWnd, desktopRect.left, desktopRect.top, 1, 1, TRUE);
    fixDeskOrigin();
    updateCanvasSize(deskOrigin);
    createCanvas();
    prepareLabels();
  } break;
  // hold mouse to move
  case WM_LBUTTONDOWN:
    SetCapture(hWnd);
    GetCursorPos(&s_last_mouse);
    showTimer.Stop();
    break;
  case WM_MOUSEMOVE:
    if (GetCapture() == hWnd) {
      POINT p;
      GetCursorPos(&p);
      int dx = p.x - s_last_mouse.x;
      int dy = p.y - s_last_mouse.y;
      if (dx || dy) {
        s_last_mouse = p;
        positionOrigin(0, p);
      }
    }
    break;
  case WM_LBUTTONUP:
    ReleaseCapture();
    showTimer.Start(100);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}
ATOM MyRegisterClassEx(HINSTANCE hInst, LPCWSTR className, WNDPROC wndProc) {
  WNDCLASSEX wcl;
  wcl.cbSize = sizeof(WNDCLASSEX);
  wcl.hInstance = hInst;
  wcl.lpszClassName = className;
  wcl.lpfnWndProc = wndProc;
  wcl.style = CS_DBLCLKS;
  wcl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wcl.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
  wcl.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcl.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wcl.lpszMenuName = NULL;
  wcl.cbWndExtra = 0;
  wcl.cbClsExtra = 0;

  return RegisterClassEx(&wcl);
}
void CreateMiniDump(LPEXCEPTION_POINTERS lpExceptionInfo) {
  // Open a file
  WCHAR dumpFile[MAX_PATH];
  wcscpy_s(dumpFile, MAX_PATH, iniFile);
  WCHAR *dot = wcsrchr(dumpFile, L'.');
  if (dot)
    *dot = L'\0';
  wcscat_s(dumpFile, MAX_PATH, L".dmp");

  HANDLE hFile = CreateFile(dumpFile, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile != NULL && hFile != INVALID_HANDLE_VALUE) {

    // Create the minidump
    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = lpExceptionInfo;
    mdei.ClientPointers = FALSE;

    MINIDUMP_TYPE mdt = MiniDumpNormal;
    BOOL retv =
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          mdt, (lpExceptionInfo != 0) ? &mdei : 0, 0, 0);

    if (!retv) {
      char msg[128];
      sprintf_s(msg, "MiniDumpWriteDump failed. Error: %u \n", GetLastError());
      OutputDebugStringA(msg);
    } else {
      OutputDebugStringA("Minidump created.\n");
    }

    // Close the file
    CloseHandle(hFile);

  } else {
    char msg[128];
    sprintf_s(msg, "CreateFile failed. Error: %u \n", GetLastError());
    OutputDebugStringA(msg);
  }
}

LONG __stdcall MyUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo) {
  CreateMiniDump(pExceptionInfo);
  return EXCEPTION_EXECUTE_HANDLER;
}

BOOL ExtractResource(DWORD resourceId, LPCWSTR outputFilename) {
  HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(resourceId), L"TEXT");
  if (!hResource)
    return FALSE;

  HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
  if (!hLoadedResource)
    return FALSE;

  LPVOID pResourceData = LockResource(hLoadedResource);
  if (!pResourceData)
    return FALSE;

  DWORD resourceSize = SizeofResource(NULL, hResource);
  if (resourceSize == 0)
    return FALSE;

  HANDLE hFile =
      CreateFile(outputFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                 FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    return FALSE;

  DWORD bytesWritten;
  BOOL result =
      WriteFile(hFile, pResourceData, resourceSize, &bytesWritten, NULL);
  CloseHandle(hFile);

  return result;
}

int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs,
                   int nWinMode) {
  HANDLE hMutex =
      CreateMutexA(nullptr, FALSE, "F78854180B584C7680CCC6EB262D10DD");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxA(nullptr, "Already Exist.", "Error", MB_OK | MB_ICONERROR);
    return -1;
  }

  SetProcessDPIAware();

  MSG msg;

  hInstance = hThisInst;

  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES | ICC_PAGESCROLLER_CLASS |
               ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES |
               ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES |
               ICC_WIN95_CLASSES;
  InitCommonControlsEx(&icex);

  GetModuleFileName(NULL, iniFile, MAX_PATH);
  WCHAR *dot = wcsrchr(iniFile, L'.');
  if (dot) {
    *dot = L'\0';
  }
  wcscat_s(iniFile, MAX_PATH, L".ini");

  if (GetFileAttributes(iniFile) == INVALID_FILE_ATTRIBUTES) {
    ExtractResource(IDR_INI_DEFAULT, iniFile);
  }

  WCHAR localeFile[MAX_PATH];
  GetModuleFileName(NULL, localeFile, MAX_PATH);
  WCHAR *lastSlash = wcsrchr(localeFile, '\\');
  if (lastSlash) {
    *lastSlash = '\0';
  }

  WCHAR enIni[MAX_PATH];
  swprintf_s(enIni, MAX_PATH, L"%s\\keycastow_en.ini", localeFile);
  if (GetFileAttributes(enIni) == INVALID_FILE_ATTRIBUTES) {
    ExtractResource(IDR_INI_EN, enIni);
  }

  WCHAR zhIni[MAX_PATH];
  swprintf_s(zhIni, MAX_PATH, L"%s\\keycastow_zh.ini", localeFile);
  if (GetFileAttributes(zhIni) == INVALID_FILE_ATTRIBUTES) {
    ExtractResource(IDR_INI_ZH, zhIni);
  }

  LANGID langId = GetUserDefaultUILanguage();
  WCHAR langCode[10] = {0};
  LCID lcid = MAKELCID(langId, SORT_DEFAULT);
  
  // Try to get ISO 639-1 language name (e.g., "fr", "en", "zh")
  if (GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, langCode, 10) > 0) {
      WCHAR targetIni[MAX_PATH];
      swprintf_s(targetIni, MAX_PATH, L"%s\\keycastow_%s.ini", localeFile, langCode);
      
      // If the specific language file exists, use it
      if (GetFileAttributes(targetIni) != INVALID_FILE_ATTRIBUTES) {
          wcscat_s(localeFile, MAX_PATH, L"\\keycastow_");
          wcscat_s(localeFile, MAX_PATH, langCode);
          wcscat_s(localeFile, MAX_PATH, L".ini");
      } else {
          // Fallback logic if specific file doesn't exist
          if ((langId & 0xFF) == LANG_CHINESE) {
            wcscat_s(localeFile, MAX_PATH, L"\\keycastow_zh.ini");
          } else {
            wcscat_s(localeFile, MAX_PATH, L"\\keycastow_en.ini");
          }
      }
  } else {
      // Fallback if GetLocaleInfo fails
      if ((langId & 0xFF) == LANG_CHINESE) {
        wcscat_s(localeFile, MAX_PATH, L"\\keycastow_zh.ini");
      } else {
        wcscat_s(localeFile, MAX_PATH, L"\\keycastow_en.ini");
      }
  }

  if (GetFileAttributes(localeFile) == INVALID_FILE_ATTRIBUTES) {
    GetModuleFileName(NULL, localeFile, MAX_PATH);
    lastSlash = wcsrchr(localeFile, '\\');
    if (lastSlash)
      *lastSlash = '\0';
    wcscat_s(localeFile, MAX_PATH, L"\\keycastow_en.ini");
  }

  LocaleManager::GetInstance().Load(localeFile);

#ifdef _DEBUG
  wcscpy_s(capFile, MAX_PATH, iniFile);
  capFile[wcslen(capFile) - 4] = '\0';
  wcscat_s(capFile, MAX_PATH, L".cap");

  wcscpy_s(logFile, MAX_PATH, iniFile);
  logFile[wcslen(logFile) - 4] = '\0';
  wcscat_s(logFile, MAX_PATH, L".txt");
  errno_t err = _wfopen_s(&capStream, capFile, L"wb");
  err = _wfopen_s(&logStream, logFile, L"a");
#endif

  GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  if (!MyRegisterClassEx(hThisInst, szWinName, WindowFunc)) {
    MessageBox(NULL,
               I18N(L"Messages", L"CouldNotRegisterWindowClass",
                    L"Could not register window class")
                   .c_str(),
               I18N(L"Messages", L"Error", L"Error").c_str(), MB_OK);
    return 0;
  }

  hMainWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                            szWinName, szWinName, WS_POPUP, 0,
                            0,    // X and Y position of window
                            1, 1, // Width and height of window
                            NULL, NULL, hThisInst, NULL);
  if (!hMainWnd) {
    MessageBox(
        NULL,
        I18N(L"Messages", L"CouldNotCreateWindow", L"Could not create window")
            .c_str(),
        I18N(L"Messages", L"Error", L"Error").c_str(), MB_OK);
    return 0;
  }

  loadSettings();
  updateCanvasSize(deskOrigin);
  hDlgSettings = CreateDialog(hThisInst, MAKEINTRESOURCE(IDD_DLGSETTINGS), NULL,
                              (DLGPROC)SettingsWndProc);
  if (!hDlgSettings) {
    MessageBox(NULL,
               I18N(L"Messages", L"CouldNotCreateSettingsDialog",
                    L"Could not create settings dialog")
                   .c_str(),
               I18N(L"Messages", L"Error", L"Error").c_str(), MB_OK);
  }
  MyRegisterClassEx(hThisInst, L"STAMP", DraggableWndProc);
  hWndStamp = CreateWindowEx(WS_EX_LAYERED | WS_EX_NOACTIVATE, L"STAMP",
                             L"STAMP", WS_VISIBLE | WS_POPUP, 0, 0, 1, 1, NULL,
                             NULL, hThisInst, NULL);

  if (!RegisterHotKey(NULL, 1, tcModifiers | MOD_NOREPEAT, tcKey)) {
    MessageBox(NULL,
               I18N(L"Messages", L"UnableToRegisterHotkey",
                    L"Unable to register hotkey, you probably need go to "
                    L"settings to redefine your hotkey for toggle capturing.")
                   .c_str(),
               I18N(L"Messages", L"Warning", L"Warning").c_str(),
               MB_OK | MB_ICONWARNING);
  }
  UpdateWindow(hMainWnd);

  createCanvas();
  prepareLabels();
  ShowWindow(hMainWnd, SW_SHOW);
  HFONT hlabelFont =
      CreateFont(20, 10, 0, 0, FW_BLACK, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                 VARIABLE_PITCH, TEXT("Arial"));
  HWND hlink = GetDlgItem(hDlgSettings, IDC_SYSLINK1);
  SendMessage(hlink, WM_SETFONT, (WPARAM)hlabelFont, TRUE);

  showTimer.OnTimedEvent = startFade;
  showTimer.Start(SHOWTIMER_INTERVAL);
  previewTimer.OnTimedEvent = previewLabel;

  kbdhook = SetWindowsHookEx(WH_KEYBOARD_LL, LLKeyboardProc, hThisInst, NULL);
  moshook = SetWindowsHookEx(WH_MOUSE_LL, LLMouseProc, hThisInst, 0);
  if (!kbdhook || !moshook) {
    MessageBox(
        NULL,
        I18N(L"Messages", L"CouldNotInstallHooks", L"Could not install hooks")
            .c_str(),
        I18N(L"Messages", L"Error", L"Error").c_str(), MB_OK);
  }
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG);
  SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

  while (GetMessage(&msg, NULL, 0, 0)) {
    if (msg.message == WM_HOTKEY) {
      if (kbdhook) {
        std::wstring msg =
            L"\u263b - " + I18N(L"Messages", L"KeyCastOWOff", L"KeyCastOW OFF");
        showText(msg.c_str(), 1);
        UnhookWindowsHookEx(kbdhook);
        kbdhook = NULL;
        UnhookWindowsHookEx(moshook);
        moshook = NULL;
      } else {
        std::wstring msg =
            L"\u263b - " + I18N(L"Messages", L"KeyCastOWOn", L"KeyCastOW ON");
        showText(msg.c_str(), 1);
        kbdhook =
            SetWindowsHookEx(WH_KEYBOARD_LL, LLKeyboardProc, hInstance, NULL);
        moshook = SetWindowsHookEx(WH_MOUSE_LL, LLMouseProc, hThisInst, 0);
      }
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  UnhookWindowsHookEx(kbdhook);
  UnhookWindowsHookEx(moshook);
  UnregisterHotKey(NULL, 1);
  delete gCanvas;
  delete fontPlus;
  DeleteObject(hlabelFont);
#ifdef _DEBUG
  fclose(capStream);
  fclose(logStream);
#endif

  GdiplusShutdown(gdiplusToken);
  return msg.wParam;
}
