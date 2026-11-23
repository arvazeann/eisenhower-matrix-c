#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define EISENHOWER_GUI

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../latihan.c"

#define ID_SEARCH             1001
#define ID_ADD                1004
#define ID_EDIT               1005
#define ID_DELETE             1006
#define ID_TOGGLE             1007
#define ID_SAVE               1008
#define ID_EDITOR_NAME        2001
#define ID_EDITOR_DATE        2002
#define ID_EDITOR_IMPORTANT   2003
#define ID_EDITOR_SAVE        2004
#define ID_EDITOR_CANCEL      2005
#define ID_TIMER_TOAST        3001

enum Halaman
{
    HALAMAN_DASHBOARD,
    HALAMAN_MATRIX,
    HALAMAN_SEMUA,
    HALAMAN_SELESAI,
    HALAMAN_OVERDUE,
    JUMLAH_HALAMAN
};

struct EditorState
{
    int id;
    int edit;
};

struct TaskHit
{
    RECT rect;
    int id;
};

static struct DaftarTugas g_daftar;
static HWND g_mainWindow;
static HWND g_search;
static HWND g_actionAdd;
static HWND g_actionEdit;
static HWND g_actionDelete;
static HWND g_actionToggle;
static HWND g_actionSave;
static HFONT g_fontBody;
static HFONT g_fontSmall;
static HFONT g_fontSection;
static HFONT g_fontQuadrant;
static HFONT g_fontTitle;
static HBRUSH g_editBrush;
static int g_page = HALAMAN_DASHBOARD;
static int g_loading = 1;
static int g_selectedID;
static int *g_visibleIDs;
static int g_visibleCount;
static int g_scrollRow;
static int g_visibleRows;
static int g_taskHitCount;
static struct TaskHit g_taskHits[384];
static RECT g_navRects[JUMLAH_HALAMAN];
static RECT g_matrixRects[4];
static RECT g_taskListRect;
static RECT g_inspectorRect;
static wchar_t g_toast[200];
static char g_dataFile[MAX_PATH];

static const int SIDEBAR_WIDTH = 224;
static const COLORREF C_BACKGROUND = RGB(246, 247, 249);
static const COLORREF C_SURFACE = RGB(255, 255, 255);
static const COLORREF C_TEXT = RGB(36, 42, 51);
static const COLORREF C_MUTED = RGB(115, 124, 137);
static const COLORREF C_BORDER = RGB(230, 233, 238);
static const COLORREF C_NAV = RGB(250, 251, 252);
static const COLORREF C_ACCENT = RGB(67, 104, 85);
static const COLORREF C_ACCENT_PALE = RGB(236, 243, 238);
static const COLORREF C_DANGER = RGB(166, 75, 69);
static const COLORREF C_DANGER_PALE = RGB(250, 239, 237);
static const COLORREF C_QUADRANT[4] = {
    RGB(177, 91, 77), RGB(76, 112, 151), RGB(170, 128, 56), RGB(72, 127, 94)};
static const COLORREF C_QUADRANT_PALE[4] = {
    RGB(252, 244, 242), RGB(243, 247, 251), RGB(251, 248, 240), RGB(242, 248, 244)};

static RECT rectMake(int left, int top, int right, int bottom)
{
    RECT rect = {left, top, right, bottom};
    return rect;
}

static void drawFill(HDC dc, RECT rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

static void drawRule(HDC dc, int x1, int y1, int x2, int y2, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

static void drawRoundBox(HDC dc, RECT rect, COLORREF fill, COLORREF border, int radius)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void drawText(HDC dc, const wchar_t *text, RECT rect, HFONT font,
                     COLORREF color, UINT flags)
{
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, flags | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

static void drawPill(HDC dc, RECT rect, const wchar_t *text,
                     COLORREF fill, COLORREF ink)
{
    drawRoundBox(dc, rect, fill, fill, 10);
    RECT label = rect;
    InflateRect(&label, -8, 0);
    drawText(dc, text, label, g_fontSmall, ink,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void addTaskHit(RECT rect, int id)
{
    if (g_taskHitCount < (int)(sizeof(g_taskHits) / sizeof(g_taskHits[0])))
    {
        g_taskHits[g_taskHitCount].rect = rect;
        g_taskHits[g_taskHitCount].id = id;
        g_taskHitCount++;
    }
}

static void utf8ToWide(const char *source, wchar_t *destination, int capacity)
{
    if (capacity <= 0)
        return;
    destination[0] = L'\0';
    if (source != NULL)
        MultiByteToWideChar(CP_UTF8, 0, source, -1, destination, capacity);
}

static int wideToUtf8(const wchar_t *source, char *destination, int capacity)
{
    if (source == NULL || destination == NULL || capacity <= 0)
        return 0;
    return WideCharToMultiByte(CP_UTF8, 0, source, -1,
                               destination, capacity, NULL, NULL) > 0;
}

static const wchar_t *pageTitle(int page)
{
    switch (page)
    {
    case HALAMAN_MATRIX:
        return L"Eisenhower Matrix";
    case HALAMAN_SEMUA:
        return L"Semua tugas";
    case HALAMAN_SELESAI:
        return L"Tugas selesai";
    case HALAMAN_OVERDUE:
        return L"Terlambat";
    default:
        return L"Dashboard";
    }
}

static const wchar_t *pageSubtitle(int page)
{
    switch (page)
    {
    case HALAMAN_MATRIX:
        return L"Tentukan fokus berdasarkan kepentingan dan urgensi.";
    case HALAMAN_SEMUA:
        return L"Kelola, cari, dan perbarui seluruh pekerjaanmu.";
    case HALAMAN_SELESAI:
        return L"Lihat kembali pekerjaan yang sudah dituntaskan.";
    case HALAMAN_OVERDUE:
        return L"Tugas belum selesai yang melewati deadline.";
    default:
        return L"Ruang kerja tugasmu, tertata dalam satu tampilan.";
    }
}

static const wchar_t *quadrantTitle(enum Kuadran quadrant)
{
    switch (quadrant)
    {
    case PENTING_MENDESAK:
        return L"Penting & mendesak";
    case PENTING_TIDAK_MENDESAK:
        return L"Penting & tidak mendesak";
    case TIDAK_PENTING_MENDESAK:
        return L"Tidak penting & mendesak";
    case TIDAK_PENTING_TIDAK_MENDESAK:
        return L"Tidak penting & tidak mendesak";
    default:
        return L"Deadline tidak valid";
    }
}

static const wchar_t *urgencyText(struct Tugas *task)
{
    int days = cekStatusDeadline(task);
    if (days == -9999)
        return L"Tidak diketahui";
    if (days < 0)
        return L"Terlambat";
    if (days == 0)
        return L"Hari ini";
    if (days <= 3)
        return L"Mendesak";
    return L"Terjadwal";
}

static void setToast(const wchar_t *message)
{
    wcsncpy(g_toast, message, sizeof(g_toast) / sizeof(g_toast[0]) - 1);
    g_toast[sizeof(g_toast) / sizeof(g_toast[0]) - 1] = L'\0';
    if (g_mainWindow != NULL)
        SetTimer(g_mainWindow, ID_TIMER_TOAST, 2800, NULL);
    InvalidateRect(g_mainWindow, NULL, FALSE);
}

static int saveData(void)
{
    if (simpanTugas(&g_daftar, g_dataFile))
        return 1;

    MessageBoxW(g_mainWindow,
                L"Data gagal disimpan. Periksa lokasi file dan izin folder.",
                L"Gagal menyimpan", MB_OK | MB_ICONERROR);
    return 0;
}

static void updateScrollBar(void)
{
    if (g_mainWindow == NULL)
        return;

    int isListPage = g_page == HALAMAN_SEMUA ||
                     g_page == HALAMAN_SELESAI ||
                     g_page == HALAMAN_OVERDUE;
    int maxScroll = g_visibleCount - g_visibleRows;
    if (maxScroll < 0)
        maxScroll = 0;

    ShowScrollBar(g_mainWindow, SB_VERT, isListPage && maxScroll > 0);
    SCROLLINFO info = {0};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = g_visibleCount > 0 ? g_visibleCount - 1 : 0;
    info.nPage = g_visibleRows > 0 ? (UINT)g_visibleRows : 1;
    info.nPos = g_scrollRow;
    SetScrollInfo(g_mainWindow, SB_VERT, &info, TRUE);
}

static void layoutControls(HWND window)
{
    RECT client;
    GetClientRect(window, &client);
    int width = client.right;
    int height = client.bottom;
    int contentX = SIDEBAR_WIDTH + 32;
    int contentRight = width - 32;
    int listPage = g_page == HALAMAN_SEMUA ||
                   g_page == HALAMAN_SELESAI ||
                   g_page == HALAMAN_OVERDUE;
    int selected = g_selectedID != 0 && cariIndexByID(&g_daftar, g_selectedID) >= 0;

    MoveWindow(g_actionAdd, contentRight - 140, 28, 140, 40, TRUE);
    MoveWindow(g_actionSave, contentRight - 238, 28, 86, 40, TRUE);
    ShowWindow(g_actionAdd, SW_SHOW);
    ShowWindow(g_actionSave, SW_SHOW);

    ShowWindow(g_search, listPage ? SW_SHOW : SW_HIDE);
    ShowWindow(g_actionEdit, listPage && selected ? SW_SHOW : SW_HIDE);
    ShowWindow(g_actionDelete, listPage && selected ? SW_SHOW : SW_HIDE);
    ShowWindow(g_actionToggle, listPage && selected ? SW_SHOW : SW_HIDE);

    if (listPage)
    {
        int actionsWidth = selected ? 316 : 0;
        int searchWidth = contentRight - contentX - actionsWidth - 16;
        if (searchWidth > 360)
            searchWidth = 360;
        if (searchWidth < 180)
            searchWidth = 180;
        MoveWindow(g_search, contentX, 112, searchWidth, 38, TRUE);
        if (selected)
        {
            int right = contentRight;
            MoveWindow(g_actionDelete, right - 82, 112, 82, 38, TRUE);
            MoveWindow(g_actionEdit, right - 174, 112, 78, 38, TRUE);
            MoveWindow(g_actionToggle, right - 308, 112, 120, 38, TRUE);
        }

        g_taskListRect = rectMake(contentX, 166, contentRight, height - 122);
        g_inspectorRect = rectMake(contentX, height - 106, contentRight, height - 32);
        int rowsStart = g_taskListRect.top + 46;
        int rowsBottom = g_inspectorRect.top - 10;
        g_visibleRows = (rowsBottom - rowsStart) / 58;
        if (g_visibleRows < 1)
            g_visibleRows = 1;
    }
    else
    {
        g_visibleRows = 0;
    }

    updateScrollBar();
}

static void rebuildView(void)
{
    free(g_visibleIDs);
    g_visibleIDs = NULL;
    g_visibleCount = 0;
    g_scrollRow = 0;

    if (g_page == HALAMAN_SEMUA || g_page == HALAMAN_SELESAI ||
        g_page == HALAMAN_OVERDUE)
    {
        if (g_daftar.jumlah > 0)
            g_visibleIDs = malloc((size_t)g_daftar.jumlah * sizeof(int));
        if (g_daftar.jumlah > 0 && g_visibleIDs == NULL)
        {
            setToast(L"Memori tidak cukup untuk menampilkan tugas.");
            return;
        }

        wchar_t wideQuery[256] = L"";
        char query[512] = "";
        if (g_search != NULL)
        {
            GetWindowTextW(g_search, wideQuery,
                           (int)(sizeof(wideQuery) / sizeof(wideQuery[0])));
            wideToUtf8(wideQuery, query, sizeof(query));
        }

        int *indeksCocok = g_daftar.jumlah > 0
                               ? malloc((size_t)g_daftar.jumlah * sizeof(int))
                               : NULL;
        if (g_daftar.jumlah > 0 && indeksCocok == NULL)
        {
            setToast(L"Memori tidak cukup untuk memfilter tugas.");
            return;
        }

        int statusFilter = g_page == HALAMAN_SELESAI ? TUGAS_SELESAI : -1;
        int overdueOnly = g_page == HALAMAN_OVERDUE;
        int jumlahCocok = ambilIndexTugasDenganFilter(
            &g_daftar, statusFilter, overdueOnly, query,
            indeksCocok, g_daftar.jumlah);
        if (jumlahCocok < 0)
        {
            free(indeksCocok);
            return;
        }

        for (int i = 0; i < jumlahCocok; i++)
        {
            int index = indeksCocok[i];
            struct Tugas *task = &g_daftar.data[index];
            g_visibleIDs[g_visibleCount++] = task->id;
        }
        free(indeksCocok);
    }

    int selectedVisible = 0;
    for (int i = 0; i < g_visibleCount; i++)
    {
        if (g_visibleIDs[i] == g_selectedID)
            selectedVisible = 1;
    }
    if (g_selectedID != 0 && !selectedVisible)
        g_selectedID = 0;

    if (g_mainWindow != NULL)
    {
        layoutControls(g_mainWindow);
        InvalidateRect(g_mainWindow, NULL, FALSE);
    }
}

static void setPage(int page)
{
    if (page < 0 || page >= JUMLAH_HALAMAN)
        return;
    g_page = page;
    g_scrollRow = 0;
    rebuildView();
}

static int countTasks(int statusFilter, int overdueOnly)
{
    struct StatistikTugas statistik = hitungStatistikTugas(&g_daftar);
    if (overdueOnly)
        return statistik.overdue;
    if (statusFilter == TUGAS_SELESAI)
        return statistik.selesai;
    if (statusFilter == TUGAS_BELUM_SELESAI)
        return statistik.belumSelesai;
    return statistik.total;
}

static void drawMetric(HDC dc, RECT rect, const wchar_t *label, int count,
                       COLORREF marker)
{
    drawRoundBox(dc, rect, C_SURFACE, C_BORDER, 10);
    RECT stripe = rectMake(rect.left + 1, rect.top + 18,
                           rect.left + 4, rect.bottom - 18);
    drawFill(dc, stripe, marker);
    wchar_t number[32];
    swprintf(number, 32, L"%d", count);
    RECT labelRect = rectMake(rect.left + 20, rect.top + 13,
                              rect.right - 12, rect.top + 37);
    RECT numberRect = rectMake(rect.left + 20, rect.top + 37,
                               rect.right - 12, rect.bottom - 8);
    drawText(dc, label, labelRect, g_fontSmall, C_MUTED,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    drawText(dc, number, numberRect, g_fontTitle, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void drawTaskMini(HDC dc, RECT rect, struct Tugas *task, int showDate)
{
    addTaskHit(rect, task->id);
    RECT row = rect;
    InflateRect(&row, -6, -1);
    if (task->id == g_selectedID)
        drawRoundBox(dc, row, RGB(255, 255, 255), RGB(238, 240, 243), 6);

    wchar_t name[512];
    utf8ToWide(task->nama, name, 512);
    RECT nameRect = rectMake(rect.left + 8, rect.top, rect.right - 94, rect.bottom);
    drawText(dc, name, nameRect, g_fontBody, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (showDate)
    {
        wchar_t date[32];
        swprintf(date, 32, L"%02d %s %04d", task->deadline.tanggal,
                 task->deadline.bulan == 1 ? L"Jan" : task->deadline.bulan == 2 ? L"Feb" :
                 task->deadline.bulan == 3 ? L"Mar" : task->deadline.bulan == 4 ? L"Apr" :
                 task->deadline.bulan == 5 ? L"Mei" : task->deadline.bulan == 6 ? L"Jun" :
                 task->deadline.bulan == 7 ? L"Jul" : task->deadline.bulan == 8 ? L"Agu" :
                 task->deadline.bulan == 9 ? L"Sep" : task->deadline.bulan == 10 ? L"Okt" :
                 task->deadline.bulan == 11 ? L"Nov" : L"Des",
                 task->deadline.tahun);
        RECT dateRect = rectMake(rect.right - 90, rect.top, rect.right - 8, rect.bottom);
        drawText(dc, date, dateRect, g_fontSmall, C_MUTED,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawQuadrantCard(HDC dc, RECT rect, int quadrant,
                             int maxTasks, int fullMatrix)
{
    COLORREF accent = C_QUADRANT[quadrant];
    drawRoundBox(dc, rect, C_QUADRANT_PALE[quadrant], C_BORDER, 10);
    RECT marker = rectMake(rect.left + 1, rect.top + 16,
                           rect.left + 4, rect.top + 48);
    drawFill(dc, marker, accent);

    RECT titleRect = rectMake(rect.left + 18, rect.top + 12,
                              rect.right - 92, rect.top + 38);
    drawText(dc, quadrantTitle((enum Kuadran)quadrant), titleRect,
             g_fontQuadrant, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int count = 0;
    for (int i = 0; i < g_daftar.jumlah; i++)
    {
        struct Tugas *task = &g_daftar.data[i];
        if (task->nama != NULL && task->status == TUGAS_BELUM_SELESAI &&
            (int)tentukanKuadran(task) == quadrant)
            count++;
    }
    wchar_t countLabel[48];
    swprintf(countLabel, 48, L"%d", count);
    RECT countRect = rectMake(rect.right - 52, rect.top + 14,
                              rect.right - 18, rect.top + 34);
    drawText(dc, countLabel, countRect, g_fontSmall, C_MUTED,
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    drawRule(dc, rect.left + 18, rect.top + 48,
             rect.right - 18, rect.top + 48, C_BORDER);

    int shown = 0;
    int lineHeight = fullMatrix ? 38 : 30;
    int top = rect.top + 54;
    int available = (rect.bottom - top - 8) / lineHeight;
    if (available > maxTasks)
        available = maxTasks;
    if (available < 0)
        available = 0;

    for (int i = 0; i < g_daftar.jumlah && shown < available; i++)
    {
        struct Tugas *task = &g_daftar.data[i];
        if (task->nama == NULL || task->status != TUGAS_BELUM_SELESAI ||
            (int)tentukanKuadran(task) != quadrant)
            continue;
        RECT taskRect = rectMake(rect.left + 15, top + shown * lineHeight,
                                 rect.right - 15, top + (shown + 1) * lineHeight);
        drawTaskMini(dc, taskRect, task, 1);
        shown++;
    }

    if (count == 0)
    {
        RECT empty = rectMake(rect.left + 20, top + 2,
                              rect.right - 20, rect.bottom - 8);
        drawText(dc, L"Belum ada tugas di kuadran ini.", empty,
                 g_fontSmall, C_MUTED,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    else if (count > shown && shown > 0)
    {
        wchar_t more[48];
        swprintf(more, 48, L"+%d tugas lainnya", count - shown);
        RECT moreRect = rectMake(rect.left + 24, top + shown * lineHeight,
                                 rect.right - 18, top + (shown + 1) * lineHeight);
        drawText(dc, more, moreRect, g_fontSmall, C_MUTED,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawDashboard(HDC dc, RECT client)
{
    int contentLeft = SIDEBAR_WIDTH + 32;
    int contentRight = client.right - 32;
    int contentWidth = contentRight - contentLeft;
    int total = g_daftar.jumlah;
    int pending = countTasks(TUGAS_BELUM_SELESAI, 0);
    int completed = countTasks(TUGAS_SELESAI, 0);
    int overdue = countTasks(-1, 1);

    int metricGap = 12;
    int metricWidth = (contentWidth - metricGap * 3) / 4;
    int metricTop = 108;
    RECT metricRects[4];
    for (int i = 0; i < 4; i++)
    {
        int left = contentLeft + i * (metricWidth + metricGap);
        metricRects[i] = rectMake(left, metricTop, left + metricWidth, metricTop + 84);
    }
    drawMetric(dc, metricRects[0], L"Semua tugas", total, C_ACCENT);
    drawMetric(dc, metricRects[1], L"Belum selesai", pending, C_QUADRANT[1]);
    drawMetric(dc, metricRects[2], L"Selesai", completed, C_QUADRANT[3]);
    drawMetric(dc, metricRects[3], L"Overdue", overdue, C_QUADRANT[0]);

    RECT matrixHeading = rectMake(contentLeft, 212, contentRight, 241);
    drawText(dc, L"Prioritas hari ini", matrixHeading, g_fontSection, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT matrixLink = rectMake(contentRight - 130, 212, contentRight, 241);
    drawText(dc, L"Lihat Matrix  →", matrixLink, g_fontSmall, C_ACCENT,
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    int matrixTop = 250;
    int matrixGapX = 12;
    int matrixGapY = 10;
    int cardWidth = (contentWidth - matrixGapX) / 2;
    int cardHeight = (client.bottom - matrixTop - 230) / 2;
    if (cardHeight < 96)
        cardHeight = 96;
    for (int q = 0; q < 4; q++)
    {
        int col = q % 2;
        int row = q / 2;
        int x = contentLeft + col * (cardWidth + matrixGapX);
        int y = matrixTop + row * (cardHeight + matrixGapY);
        g_matrixRects[q] = rectMake(x, y, x + cardWidth, y + cardHeight);
        drawQuadrantCard(dc, g_matrixRects[q], q, 4, 0);
    }

    int recentTop = matrixTop + cardHeight * 2 + matrixGapY + 25;
    RECT recentHeading = rectMake(contentLeft, recentTop,
                                  contentRight, recentTop + 25);
    drawText(dc, L"Tugas berikutnya", recentHeading,
             g_fontSection, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int rowTop = recentTop + 32;
    int shown = 0;
    int maxRecent = (client.bottom - 34 - rowTop) / 36;
    if (maxRecent > 3)
        maxRecent = 3;
    if (maxRecent < 0)
        maxRecent = 0;
    for (int i = 0; i < g_daftar.jumlah && shown < maxRecent; i++)
    {
        struct Tugas *task = &g_daftar.data[i];
        if (task->nama == NULL || task->status != TUGAS_BELUM_SELESAI)
            continue;
        RECT rowRect = rectMake(contentLeft, rowTop + shown * 36,
                                contentRight, rowTop + (shown + 1) * 36);
        drawTaskMini(dc, rowRect, task, 1);
        shown++;
    }
    if (shown == 0)
    {
        RECT empty = rectMake(contentLeft, rowTop, contentRight, rowTop + 32);
        drawText(dc, L"Semua beres. Tambahkan tugas baru untuk mulai merencanakan.",
                 empty, g_fontBody, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawMatrixPage(HDC dc, RECT client)
{
    int left = SIDEBAR_WIDTH + 32;
    int right = client.right - 32;
    int width = right - left;
    int gapX = 16;
    int gapY = 16;
    int top = 136;
    int bottom = client.bottom - 32;
    int cardWidth = (width - gapX) / 2;
    int cardHeight = (bottom - top - gapY) / 2;
    for (int q = 0; q < 4; q++)
    {
        int col = q % 2;
        int row = q / 2;
        int x = left + col * (cardWidth + gapX);
        int y = top + row * (cardHeight + gapY);
        g_matrixRects[q] = rectMake(x, y, x + cardWidth, y + cardHeight);
        drawQuadrantCard(dc, g_matrixRects[q], q, 6, 1);
    }
}

static void drawInspector(HDC dc)
{
    drawRoundBox(dc, g_inspectorRect, C_SURFACE, C_BORDER, 9);
    struct Tugas *task = ambilDetailTugas(&g_daftar, g_selectedID);
    RECT text = g_inspectorRect;
    InflateRect(&text, -16, -7);
    if (task == NULL)
    {
        drawText(dc, L"Pilih tugas untuk melihat detail dan mengaktifkan tindakan.",
                 text, g_fontSmall, C_MUTED,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }

    wchar_t name[512];
    wchar_t detail[700];
    utf8ToWide(task->nama, name, 512);
    int days = hitungSelisihHari(&task->deadline);
    swprintf(detail, 700,
             L"%ls   ·   %02d-%02d-%04d   ·   %ls   ·   %d hari   ·   %ls",
             name, task->deadline.tanggal, task->deadline.bulan, task->deadline.tahun,
             task->status == TUGAS_SELESAI ? L"Selesai" : L"Belum selesai",
             days, quadrantTitle(tentukanKuadran(task)));
    drawText(dc, L"DETAIL TUGAS", rectMake(text.left, text.top, text.right, text.top + 18),
             g_fontSmall, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT value = rectMake(text.left, text.top + 18, text.right, text.bottom);
    drawText(dc, detail, value, g_fontBody, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void drawTaskList(HDC dc)
{
    int left = g_taskListRect.left;
    int right = g_taskListRect.right;
    int top = g_taskListRect.top;
    int width = right - left;
    int headerHeight = 42;
    int rowHeight = 58;
    RECT outer = g_taskListRect;
    drawRoundBox(dc, outer, C_SURFACE, C_BORDER, 9);
    RECT header = rectMake(left + 1, top + 1, right - 1, top + headerHeight);
    drawFill(dc, header, C_NAV);

    int nameX = left + 20;
    int dueX = left + (width * 55) / 100;
    int priorityX = left + (width * 72) / 100;
    int statusX = left + (width * 84) / 100;
    drawText(dc, L"TUGAS", rectMake(nameX, top + 3, dueX - 14, top + headerHeight - 2),
             g_fontSmall, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, L"DEADLINE", rectMake(dueX, top + 3, priorityX - 12, top + headerHeight - 2),
             g_fontSmall, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, L"PRIORITAS", rectMake(priorityX, top + 3, statusX - 8, top + headerHeight - 2),
             g_fontSmall, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, L"STATUS", rectMake(statusX, top + 3, right - 18, top + headerHeight - 2),
             g_fontSmall, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int rowsTop = top + headerHeight;
    int rowsBottom = g_inspectorRect.top - 10;
    int availableRows = (rowsBottom - rowsTop) / rowHeight;
    if (availableRows < 0)
        availableRows = 0;
    g_visibleRows = availableRows;
    updateScrollBar();

    if (g_visibleCount == 0)
    {
        wchar_t query[256] = L"";
        GetWindowTextW(g_search, query, 256);
        int centerX = (left + right) / 2;
        int centerY = (rowsTop + rowsBottom) / 2;
        RECT icon = rectMake(centerX - 23, centerY - 58,
                             centerX + 23, centerY - 12);
        drawRoundBox(dc, icon, C_ACCENT_PALE, C_ACCENT_PALE, 12);
        drawText(dc, L"✓", icon, g_fontSection, C_ACCENT,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const wchar_t *message = query[0] != L'\0'
                                     ? L"Tidak ada tugas yang cocok"
                                     : g_page == HALAMAN_OVERDUE
                                           ? L"Tidak ada tugas yang terlambat"
                                           : g_page == HALAMAN_SELESAI
                                                 ? L"Belum ada tugas yang selesai"
                                                 : L"Belum ada tugas di sini";
        RECT title = rectMake(left + 30, centerY - 7, right - 30, centerY + 22);
        drawText(dc, message, title, g_fontSection, C_TEXT,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const wchar_t *hint = query[0] != L'\0'
                                  ? L"Coba kata kunci lain atau hapus pencarian."
                                  : g_page == HALAMAN_SELESAI
                                        ? L"Tugas yang kamu selesaikan akan muncul di sini."
                                        : g_page == HALAMAN_OVERDUE
                                              ? L"Semua deadline masih aman."
                                              : L"Tambahkan tugas untuk mulai merencanakan pekerjaanmu.";
        RECT hintRect = rectMake(left + 30, centerY + 24, right - 30, centerY + 50);
        drawText(dc, hint, hintRect, g_fontSmall, C_MUTED,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }

    int end = g_scrollRow + availableRows;
    if (end > g_visibleCount)
        end = g_visibleCount;
    for (int row = g_scrollRow; row < end; row++)
    {
        int index = cariIndexByID(&g_daftar, g_visibleIDs[row]);
        if (index < 0)
            continue;
        struct Tugas *task = &g_daftar.data[index];
        int y = rowsTop + (row - g_scrollRow) * rowHeight;
        RECT rowRect = rectMake(left + 1, y, right - 1, y + rowHeight);
        if (task->id == g_selectedID)
            drawFill(dc, rowRect, RGB(244, 248, 245));

        addTaskHit(rowRect, task->id);
        wchar_t name[512];
        wchar_t date[40];
        utf8ToWide(task->nama, name, 512);
        swprintf(date, 40, L"%02d-%02d-%04d",
                 task->deadline.tanggal, task->deadline.bulan, task->deadline.tahun);

        RECT nameRect = rectMake(nameX, y + 6, dueX - 14, y + 31);
        drawText(dc, name, nameRect, g_fontBody, C_TEXT,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        wchar_t urgency[96];
        swprintf(urgency, 96, L"%ls", urgencyText(task));
        RECT urgencyRect = rectMake(nameX, y + 31, dueX - 14, y + 52);
        drawText(dc, urgency, urgencyRect, g_fontSmall,
                 cekStatusDeadline(task) < 0 ? C_DANGER : C_MUTED,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT dateRect = rectMake(dueX, y + 4, priorityX - 12, y + rowHeight - 4);
        drawText(dc, date, dateRect, g_fontBody, C_TEXT,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT priorityRect = rectMake(priorityX, y + 17, priorityX + 105, y + 41);
        drawPill(dc, priorityRect,
                 task->penting ? L"Penting" : L"Tidak penting",
                 task->penting ? RGB(240, 244, 249) : RGB(246, 246, 247),
                 task->penting ? RGB(75, 101, 132) : C_MUTED);

        RECT statusRect = rectMake(statusX, y + 17, statusX + 112, y + 41);
        COLORREF statusFill = task->status == TUGAS_SELESAI ? C_ACCENT_PALE : RGB(244, 245, 247);
        COLORREF statusInk = task->status == TUGAS_SELESAI ? C_ACCENT : C_MUTED;
        drawPill(dc, statusRect,
                 task->status == TUGAS_SELESAI ? L"Selesai" : L"Belum selesai",
                 statusFill, statusInk);
        drawRule(dc, left + 18, y + rowHeight - 1,
                 right - 18, y + rowHeight - 1, C_BORDER);
    }
}

static void drawSidebar(HDC dc, RECT client)
{
    RECT sidebar = rectMake(0, 0, SIDEBAR_WIDTH, client.bottom);
    drawFill(dc, sidebar, C_SURFACE);
    drawRule(dc, SIDEBAR_WIDTH - 1, 0, SIDEBAR_WIDTH - 1, client.bottom, C_BORDER);

    RECT logo = rectMake(22, 27, 54, 59);
    drawRoundBox(dc, logo, C_ACCENT, C_ACCENT, 8);
    drawText(dc, L"E", logo, g_fontSection, RGB(255, 255, 255),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, L"FOCUS", rectMake(64, 25, 190, 44), g_fontSection, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, L"TASK PLANNER", rectMake(64, 44, 194, 62), g_fontSmall, C_MUTED,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    drawText(dc, L"WORKSPACE", rectMake(24, 104, 196, 126), g_fontSmall, C_MUTED,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    const wchar_t *labels[JUMLAH_HALAMAN] = {
        L"Ikhtisar", L"Matrix", L"Semua tugas", L"Selesai", L"Overdue"};
    const wchar_t *icons[JUMLAH_HALAMAN] = {L"⌂", L"▦", L"≡", L"✓", L"◷"};
    for (int i = 0; i < JUMLAH_HALAMAN; i++)
    {
        int top = 138 + i * 46;
        g_navRects[i] = rectMake(12, top, SIDEBAR_WIDTH - 12, top + 38);
        int active = i == g_page;
        if (active)
            drawRoundBox(dc, g_navRects[i], C_ACCENT_PALE, C_ACCENT_PALE, 7);
        drawText(dc, icons[i], rectMake(25, top + 3, 54, top + 35), g_fontBody,
                 active ? C_ACCENT : C_MUTED,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        drawText(dc, labels[i], rectMake(59, top + 3, SIDEBAR_WIDTH - 25, top + 35),
                 active ? g_fontSection : g_fontBody,
                 active ? C_ACCENT : C_TEXT,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    int overdue = countTasks(-1, 1);
    if (overdue > 0)
    {
        wchar_t count[24];
        swprintf(count, 24, L"%d", overdue);
        RECT badge = rectMake(SIDEBAR_WIDTH - 48, g_navRects[HALAMAN_OVERDUE].top + 8,
                              SIDEBAR_WIDTH - 23, g_navRects[HALAMAN_OVERDUE].top + 30);
        drawPill(dc, badge, count, C_DANGER_PALE, C_DANGER);
    }

    RECT footerLine = rectMake(20, client.bottom - 67, SIDEBAR_WIDTH - 20, client.bottom - 66);
    drawFill(dc, footerLine, C_BORDER);
    drawText(dc, L"Penyimpanan lokal", rectMake(24, client.bottom - 56, 190, client.bottom - 34),
             g_fontSmall, C_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, L"TXT  ·  Tersimpan otomatis", rectMake(24, client.bottom - 34,
             SIDEBAR_WIDTH - 18, client.bottom - 14), g_fontSmall, C_ACCENT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void drawPageHeader(HDC dc, RECT client)
{
    int left = SIDEBAR_WIDTH + 32;
    RECT title = rectMake(left, 25, client.right - 280, 62);
    drawText(dc, pageTitle(g_page), title, g_fontTitle, C_TEXT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT subtitle = rectMake(left, 62, client.right - 280, 88);
    drawText(dc, pageSubtitle(g_page), subtitle, g_fontBody, C_MUTED,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void drawToast(HDC dc, RECT client)
{
    if (g_toast[0] == L'\0')
        return;
    RECT rect = rectMake(client.right - 370, client.bottom - 78,
                         client.right - 24, client.bottom - 28);
    drawRoundBox(dc, rect, RGB(43, 54, 48), RGB(43, 54, 48), 8);
    drawText(dc, g_toast, rect, g_fontBody, RGB(255, 255, 255),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void paintMain(HWND window, HDC target)
{
    RECT client;
    GetClientRect(window, &client);
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(buffer, bitmap);

    drawFill(buffer, client, C_BACKGROUND);
    drawSidebar(buffer, client);
    drawPageHeader(buffer, client);

    if (g_loading)
    {
        int contentLeft = SIDEBAR_WIDTH + 32;
        RECT loadingBox = rectMake(contentLeft, 132, client.right - 32, client.bottom - 32);
        drawRoundBox(buffer, loadingBox, C_SURFACE, C_BORDER, 10);
        RECT loadingTitle = rectMake(loadingBox.left + 24, loadingBox.top + 28,
                                     loadingBox.right - 24, loadingBox.top + 62);
        RECT loadingHint = rectMake(loadingBox.left + 24, loadingBox.top + 66,
                                    loadingBox.right - 24, loadingBox.top + 94);
        drawText(buffer, L"Memuat daftar tugas...", loadingTitle,
                 g_fontSection, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        drawText(buffer, L"Data lokal sedang dibaca.", loadingHint,
                 g_fontSmall, C_MUTED, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else if (g_page == HALAMAN_DASHBOARD)
        drawDashboard(buffer, client);
    else if (g_page == HALAMAN_MATRIX)
        drawMatrixPage(buffer, client);
    else
    {
        drawTaskList(buffer);
        drawInspector(buffer);
    }
    drawToast(buffer, client);

    BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

static void setControlFont(HWND control, HFONT font)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static HWND addControl(DWORD exStyle, const wchar_t *className,
                       const wchar_t *text, DWORD style,
                       int x, int y, int width, int height,
                       HWND parent, int id)
{
    HWND control = CreateWindowExW(exStyle, className, text,
                                   WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, parent,
                                   (HMENU)(INT_PTR)id,
                                   GetModuleHandleW(NULL), NULL);
    if (control != NULL)
        setControlFont(control, g_fontBody);
    return control;
}

static void updateToggleLabel(void)
{
    int index = cariIndexByID(&g_daftar, g_selectedID);
    if (index < 0)
        return;
    SetWindowTextW(g_actionToggle,
                   g_daftar.data[index].status == TUGAS_SELESAI
                       ? L"Buka kembali" : L"Tandai selesai");
}

static void openEditor(int edit);
static void drawButton(const DRAWITEMSTRUCT *item);

static void editorClose(HWND window)
{
    DestroyWindow(window);
}

static LRESULT CALLBACK editorProcedure(HWND window, UINT message,
                                        WPARAM wParam, LPARAM lParam)
{
    struct EditorState *state = (struct EditorState *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lParam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return TRUE;
    }

    if (message == WM_CREATE)
    {
        HWND nameLabel = addControl(0, L"STATIC", L"Nama tugas", 0,
                                    28, 31, 400, 23, window, 0);
        HWND name = addControl(0, L"EDIT", L"",
                               ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
                               28, 56, 444, 38, window, ID_EDITOR_NAME);
        HWND dateLabel = addControl(0, L"STATIC", L"Deadline  ·  TT-BB-YYYY", 0,
                                    28, 115, 250, 23, window, 0);
        HWND date = addControl(0, L"EDIT", L"",
                               ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
                               28, 140, 190, 38, window, ID_EDITOR_DATE);
        HWND important = addControl(0, L"BUTTON", L"Tugas ini penting",
                                    BS_AUTOCHECKBOX | WS_TABSTOP,
                                    28, 198, 260, 30, window, ID_EDITOR_IMPORTANT);
        HWND save = addControl(0, L"BUTTON", L"Simpan tugas",
                               BS_OWNERDRAW | WS_TABSTOP,
                               278, 256, 112, 40, window, ID_EDITOR_SAVE);
        HWND cancel = addControl(0, L"BUTTON", L"Batal",
                                 BS_OWNERDRAW | WS_TABSTOP,
                                 400, 256, 72, 40, window, ID_EDITOR_CANCEL);
        setControlFont(nameLabel, g_fontSmall);
        setControlFont(dateLabel, g_fontSmall);
        setControlFont(name, g_fontBody);
        setControlFont(date, g_fontBody);
        setControlFont(important, g_fontBody);
        setControlFont(save, g_fontBody);
        setControlFont(cancel, g_fontBody);

        if (state != NULL && state->edit)
        {
            int index = cariIndexByID(&g_daftar, state->id);
            if (index >= 0)
            {
                struct Tugas *task = &g_daftar.data[index];
                wchar_t wideName[512];
                wchar_t dateText[40];
                utf8ToWide(task->nama, wideName, 512);
                swprintf(dateText, 40, L"%02d-%02d-%04d",
                         task->deadline.tanggal, task->deadline.bulan,
                         task->deadline.tahun);
                SetWindowTextW(name, wideName);
                SetWindowTextW(date, dateText);
                SendMessageW(important, BM_SETCHECK,
                             task->penting ? BST_CHECKED : BST_UNCHECKED, 0);
            }
        }
        else
        {
            SendMessageW(important, BM_SETCHECK, BST_CHECKED, 0);
        }
        return 0;
    }

    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT)
    {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_SURFACE);
        return (LRESULT)g_editBrush;
    }

    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        drawFill(dc, client, C_SURFACE);
        drawFill(dc, rectMake(0, 0, client.right, 5), C_ACCENT);
        EndPaint(window, &paint);
        return 0;
    }

    if (message == WM_DRAWITEM)
    {
        drawButton((DRAWITEMSTRUCT *)lParam);
        return TRUE;
    }

    if (message == WM_COMMAND)
    {
        int command = LOWORD(wParam);
        if (command == ID_EDITOR_CANCEL)
        {
            editorClose(window);
            return 0;
        }
        if (command == ID_EDITOR_SAVE)
        {
            wchar_t wideName[256];
            wchar_t wideDate[64];
            wchar_t extra;
            char name[600];
            int day, month, year;
            GetWindowTextW(GetDlgItem(window, ID_EDITOR_NAME), wideName, 256);
            GetWindowTextW(GetDlgItem(window, ID_EDITOR_DATE), wideDate, 64);
            int parsed = swscanf(wideDate, L"%d-%d-%d %c", &day, &month, &year, &extra);
            if (!wideToUtf8(wideName, name, sizeof(name)) || parsed != 3)
            {
                MessageBoxW(window, L"Isi nama dan deadline dengan format TT-BB-YYYY.",
                            L"Periksa kembali", MB_OK | MB_ICONWARNING);
                return 0;
            }

            int important = SendMessageW(GetDlgItem(window, ID_EDITOR_IMPORTANT),
                                         BM_GETCHECK, 0, 0) == BST_CHECKED;
            struct Tanggal deadline = {day, month, year};
            if (!validasiTanggalKalender(&deadline) ||
                !validasiInputTugas(name, important))
            {
                MessageBoxW(window, L"Nama atau tanggal tidak valid.",
                            L"Periksa kembali", MB_OK | MB_ICONWARNING);
                return 0;
            }

            int success = state != NULL && state->edit
                              ? editTugas(&g_daftar, state->id, name, important, deadline)
                              : tambahTugas(&g_daftar, name, important, deadline);
            if (!success)
            {
                MessageBoxW(window, L"Tugas tidak dapat disimpan. Periksa deadline dan kapasitas.",
                            L"Gagal menyimpan tugas", MB_OK | MB_ICONERROR);
                return 0;
            }

            if (state != NULL && state->edit)
            {
                g_selectedID = state->id;
                if (saveData())
                    setToast(L"Perubahan tugas berhasil disimpan.");
            }
            else
            {
                g_selectedID = g_daftar.idBerikut - 1;
                g_page = HALAMAN_SEMUA;
                if (saveData())
                    setToast(L"Tugas baru berhasil ditambahkan.");
            }
            rebuildView();
            editorClose(window);
            return 0;
        }
    }

    if (message == WM_CLOSE)
    {
        editorClose(window);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void openEditor(int edit)
{
    if (edit && g_selectedID == 0)
        return;

    struct EditorState state = {g_selectedID, edit};
    HWND editor = CreateWindowExW(WS_EX_DLGMODALFRAME,
                                  L"EisenhowerTaskEditor",
                                  edit ? L"Edit tugas" : L"Tugas baru",
                                  WS_CAPTION | WS_SYSMENU | WS_POPUP,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 520, 345,
                                  g_mainWindow, NULL, GetModuleHandleW(NULL), &state);
    if (editor == NULL)
        return;

    RECT owner;
    RECT dialog;
    GetWindowRect(g_mainWindow, &owner);
    GetWindowRect(editor, &dialog);
    int width = dialog.right - dialog.left;
    int height = dialog.bottom - dialog.top;
    SetWindowPos(editor, HWND_TOP,
                 owner.left + ((owner.right - owner.left) - width) / 2,
                 owner.top + ((owner.bottom - owner.top) - height) / 2,
                 width, height, SWP_SHOWWINDOW);
    EnableWindow(g_mainWindow, FALSE);
    SetFocus(GetDlgItem(editor, ID_EDITOR_NAME));

    MSG message;
    while (IsWindow(editor) && GetMessageW(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageW(editor, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(g_mainWindow, TRUE);
    SetForegroundWindow(g_mainWindow);
}

static void deleteSelected(void)
{
    if (g_selectedID == 0)
        return;
    if (MessageBoxW(g_mainWindow, L"Hapus tugas yang dipilih?",
                    L"Konfirmasi", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        return;
    if (hapusTugas(&g_daftar, g_selectedID))
    {
        g_selectedID = 0;
        if (saveData())
            setToast(L"Tugas berhasil dihapus.");
        rebuildView();
    }
}

static void toggleSelectedStatus(void)
{
    int index = cariIndexByID(&g_daftar, g_selectedID);
    if (index < 0)
        return;
    int completing = g_daftar.data[index].status != TUGAS_SELESAI;
    enum StatusTugas status = completing ? TUGAS_SELESAI : TUGAS_BELUM_SELESAI;
    if (ubahStatusTugas(&g_daftar, g_selectedID, status))
    {
        if (saveData())
            setToast(completing ? L"Tugas ditandai selesai." : L"Tugas dibuka kembali.");
        rebuildView();
    }
}

static void drawButton(const DRAWITEMSTRUCT *item)
{
    wchar_t label[96];
    GetWindowTextW(item->hwndItem, label, 96);
    int primary = item->CtlID == ID_ADD || item->CtlID == ID_EDITOR_SAVE;
    int danger = item->CtlID == ID_DELETE;
    int disabled = (item->itemState & ODS_DISABLED) != 0;
    COLORREF fill = primary ? C_ACCENT : danger ? C_DANGER_PALE : C_SURFACE;
    COLORREF ink = primary ? RGB(255, 255, 255) : danger ? C_DANGER : C_TEXT;
    COLORREF border = primary ? C_ACCENT : danger ? C_DANGER_PALE : C_BORDER;
    if (disabled)
    {
        fill = RGB(245, 246, 247);
        ink = RGB(167, 172, 180);
        border = fill;
    }
    if (item->itemState & ODS_SELECTED)
        fill = primary ? RGB(57, 91, 73) : RGB(237, 240, 243);

    RECT rect = item->rcItem;
    InflateRect(&rect, -1, -1);
    drawRoundBox(item->hDC, rect, fill, border, 8);
    drawText(item->hDC, label, rect, g_fontBody, ink,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void handleTaskHit(int x, int y)
{
    for (int i = 0; i < g_taskHitCount; i++)
    {
        if (PtInRect(&g_taskHits[i].rect, (POINT){x, y}))
        {
            g_selectedID = g_taskHits[i].id;
            if (g_page == HALAMAN_DASHBOARD || g_page == HALAMAN_MATRIX)
                g_page = HALAMAN_SEMUA;
            rebuildView();
            updateToggleLabel();
            return;
        }
    }
}

static LRESULT CALLBACK mainProcedure(HWND window, UINT message,
                                      WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_mainWindow = window;
        g_search = addControl(0, L"EDIT", L"",
                              ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
                              0, 0, 320, 38, window, ID_SEARCH);
        SendMessageW(g_search, EM_SETCUEBANNER, TRUE,
                     (LPARAM)L"Cari berdasarkan nama tugas");
        g_actionAdd = addControl(0, L"BUTTON", L"+  Tugas baru",
                                 BS_OWNERDRAW | WS_TABSTOP,
                                 0, 0, 140, 40, window, ID_ADD);
        g_actionEdit = addControl(0, L"BUTTON", L"Edit",
                                  BS_OWNERDRAW | WS_TABSTOP,
                                  0, 0, 78, 38, window, ID_EDIT);
        g_actionDelete = addControl(0, L"BUTTON", L"Hapus",
                                    BS_OWNERDRAW | WS_TABSTOP,
                                    0, 0, 82, 38, window, ID_DELETE);
        g_actionToggle = addControl(0, L"BUTTON", L"Tandai selesai",
                                    BS_OWNERDRAW | WS_TABSTOP,
                                    0, 0, 120, 38, window, ID_TOGGLE);
        g_actionSave = addControl(0, L"BUTTON", L"Simpan",
                                  BS_OWNERDRAW | WS_TABSTOP,
                                  0, 0, 86, 40, window, ID_SAVE);
        g_editBrush = CreateSolidBrush(C_SURFACE);
        layoutControls(window);
        rebuildView();
        PostMessageW(window, WM_APP + 1, 0, 0);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        layoutControls(window);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lParam;
        limits->ptMinTrackSize.x = 1060;
        limits->ptMinTrackSize.y = 710;
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        g_taskHitCount = 0;
        paintMain(window, dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DRAWITEM:
        drawButton((DRAWITEMSTRUCT *)lParam);
        return TRUE;
    case WM_CTLCOLOREDIT:
    {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_SURFACE);
        return (LRESULT)g_editBrush;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_SURFACE);
        return (LRESULT)g_editBrush;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_ADD:
            openEditor(0);
            return 0;
        case ID_EDIT:
            openEditor(1);
            return 0;
        case ID_DELETE:
            deleteSelected();
            return 0;
        case ID_TOGGLE:
            toggleSelectedStatus();
            return 0;
        case ID_SAVE:
            if (saveData())
                setToast(L"Semua perubahan sudah tersimpan.");
            return 0;
        case ID_SEARCH:
            if (HIWORD(wParam) == EN_CHANGE)
                rebuildView();
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        handleTaskHit(x, y);
        for (int i = 0; i < JUMLAH_HALAMAN; i++)
        {
            if (PtInRect(&g_navRects[i], (POINT){x, y}))
            {
                setPage(i);
                return 0;
            }
        }
        if (g_page == HALAMAN_DASHBOARD)
        {
            RECT link = rectMake(g_matrixRects[0].left,
                                 g_matrixRects[0].top - 39,
                                 g_matrixRects[0].right + (g_matrixRects[1].right - g_matrixRects[1].left) + 12,
                                 g_matrixRects[0].top - 7);
            if (PtInRect(&link, (POINT){x, y}))
            {
                setPage(HALAMAN_MATRIX);
                return 0;
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int maxScroll = g_visibleCount - g_visibleRows;
        if (maxScroll < 0)
            maxScroll = 0;
        g_scrollRow -= delta / WHEEL_DELTA;
        if (g_scrollRow < 0)
            g_scrollRow = 0;
        if (g_scrollRow > maxScroll)
            g_scrollRow = maxScroll;
        updateScrollBar();
        InvalidateRect(window, NULL, FALSE);
        return 0;
    }
    case WM_VSCROLL:
    {
        int maxScroll = g_visibleCount - g_visibleRows;
        if (maxScroll < 0)
            maxScroll = 0;
        switch (LOWORD(wParam))
        {
        case SB_LINEUP:
            g_scrollRow--;
            break;
        case SB_LINEDOWN:
            g_scrollRow++;
            break;
        case SB_PAGEUP:
            g_scrollRow -= g_visibleRows;
            break;
        case SB_PAGEDOWN:
            g_scrollRow += g_visibleRows;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            g_scrollRow = HIWORD(wParam);
            break;
        default:
            break;
        }
        if (g_scrollRow < 0)
            g_scrollRow = 0;
        if (g_scrollRow > maxScroll)
            g_scrollRow = maxScroll;
        updateScrollBar();
        InvalidateRect(window, NULL, FALSE);
        return 0;
    }
    case WM_TIMER:
        if (wParam == ID_TIMER_TOAST)
        {
            KillTimer(window, ID_TIMER_TOAST);
            g_toast[0] = L'\0';
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_APP + 1:
    {
        int loaded = muatTugas(&g_daftar, g_dataFile);
        if (!loaded && GetFileAttributesA(g_dataFile) != INVALID_FILE_ATTRIBUTES)
        {
            g_loading = 0;
            MessageBoxW(window,
                        L"File tugas.txt tersedia tetapi tidak dapat dibaca. File tidak akan ditimpa.",
                        L"Data tugas gagal dimuat", MB_OK | MB_ICONERROR);
            DestroyWindow(window);
            return 0;
        }

        g_loading = 0;
        rebuildView();
        if (loaded && g_daftar.jumlah > 0)
            setToast(L"Data tugas berhasil dimuat.");
        else
            setToast(L"Belum ada tugas tersimpan. Tambahkan tugas untuk mulai.");
        return 0;
    }
    case WM_CLOSE:
        if (g_loading)
        {
            DestroyWindow(window);
            return 0;
        }
        if (!saveData())
            return 0;
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        free(g_visibleIDs);
        g_visibleIDs = NULL;
        freeDaftarTugas(&g_daftar);
        if (g_editBrush != NULL)
            DeleteObject(g_editBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static int registerWindowClass(HINSTANCE instance, const wchar_t *name,
                               WNDPROC procedure, HBRUSH background)
{
    WNDCLASSEXW cls = {0};
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = procedure;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.hbrBackground = background;
    cls.lpszClassName = name;
    return RegisterClassExW(&cls) != 0;
}

static void makeDataPath(void)
{
    DWORD length = GetModuleFileNameA(NULL, g_dataFile, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        strcpy(g_dataFile, "..\\data\\tugas.txt");
        return;
    }
    char *slash = strrchr(g_dataFile, '\\');
    char *forwardSlash = strrchr(g_dataFile, '/');
    if (forwardSlash != NULL && (slash == NULL || forwardSlash > slash))
        slash = forwardSlash;
    if (slash == NULL || (size_t)(slash - g_dataFile) + sizeof("..\\data\\tugas.txt") > sizeof(g_dataFile))
    {
        strcpy(g_dataFile, "..\\data\\tugas.txt");
        return;
    }
    strcpy(slash + 1, "..\\data\\tugas.txt");
}

static HFONT createFont(int height, int weight)
{
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR commandLine, int showCommand)
{
    (void)previous;
    (void)commandLine;
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    g_fontBody = createFont(-16, FW_NORMAL);
    g_fontSmall = createFont(-13, FW_NORMAL);
    g_fontSection = createFont(-19, FW_SEMIBOLD);
    g_fontQuadrant = createFont(-16, FW_SEMIBOLD);
    g_fontTitle = createFont(-29, FW_SEMIBOLD);
    g_editBrush = CreateSolidBrush(C_SURFACE);

    if (g_fontBody == NULL || g_fontSmall == NULL ||
        g_fontSection == NULL || g_fontQuadrant == NULL || g_fontTitle == NULL)
    {
        MessageBoxW(NULL, L"Font aplikasi gagal disiapkan.",
                    L"Eisenhower Matrix", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!registerWindowClass(instance, L"EisenhowerMainWindow",
                             mainProcedure, NULL) ||
        !registerWindowClass(instance, L"EisenhowerTaskEditor",
                             editorProcedure, (HBRUSH)(COLOR_WINDOW + 1)))
    {
        MessageBoxW(NULL, L"Gagal menyiapkan jendela aplikasi.",
                    L"Eisenhower Matrix", MB_OK | MB_ICONERROR);
        return 1;
    }

    initDaftarTugas(&g_daftar);
    if (g_daftar.data == NULL)
    {
        MessageBoxW(NULL, L"Memori tidak cukup untuk memulai aplikasi.",
                    L"Eisenhower Matrix", MB_OK | MB_ICONERROR);
        return 1;
    }

    makeDataPath();

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int width = screenWidth - 64;
    int height = screenHeight - 72;
    if (width > 1420)
        width = 1420;
    if (height > 920)
        height = 920;
    if (width < 1060)
        width = 1060;
    if (height < 710)
        height = 710;
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;

    g_mainWindow = CreateWindowExW(0, L"EisenhowerMainWindow",
                                   L"Eisenhower Matrix",
                                   WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
                                   x, y, width, height,
                                   NULL, NULL, instance, NULL);
    if (g_mainWindow == NULL)
    {
        freeDaftarTugas(&g_daftar);
        MessageBoxW(NULL, L"Jendela aplikasi gagal dibuat.",
                    L"Eisenhower Matrix", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_mainWindow, showCommand);
    UpdateWindow(g_mainWindow);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DeleteObject(g_fontBody);
    DeleteObject(g_fontSmall);
    DeleteObject(g_fontSection);
    DeleteObject(g_fontQuadrant);
    DeleteObject(g_fontTitle);
    return (int)message.wParam;
}
