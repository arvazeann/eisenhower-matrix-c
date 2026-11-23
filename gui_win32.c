#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define EISENHOWER_GUI

#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "latihan.c"

#define ID_SEARCH       1001
#define ID_FILTER       1002
#define ID_LIST         1003
#define ID_ADD          1004
#define ID_EDIT         1005
#define ID_DELETE       1006
#define ID_TOGGLE       1007
#define ID_SAVE         1008
#define ID_DETAILS      1009
#define ID_STATUS       1010

#define ID_EDITOR_NAME     2001
#define ID_EDITOR_DATE     2002
#define ID_EDITOR_IMPORTANT 2003
#define ID_EDITOR_SAVE     2004
#define ID_EDITOR_CANCEL   2005

enum FilterTugas
{
    FILTER_SEMUA,
    FILTER_BELUM_SELESAI,
    FILTER_SELESAI,
    FILTER_OVERDUE,
    FILTER_MATRIX
};

struct EditorState
{
    int id;
    int edit;
};

static struct DaftarTugas g_daftar;
static HWND g_mainWindow;
static HWND g_search;
static HWND g_filter;
static HWND g_list;
static HWND g_details;
static HWND g_status;
static HFONT g_font;
static int g_selectedID;
static char g_dataFile[MAX_PATH];
static const COLORREF COLOR_NAVY = RGB(28, 43, 69);

static void utf8ToWide(const char *source, wchar_t *destination, int capacity)
{
    destination[0] = L'\0';
    if (source == NULL || capacity <= 0)
        return;
    MultiByteToWideChar(CP_UTF8, 0, source, -1, destination, capacity);
}

static int wideToUtf8(const wchar_t *source, char *destination, int capacity)
{
    if (source == NULL || destination == NULL || capacity <= 0)
        return 0;
    int length = WideCharToMultiByte(CP_UTF8, 0, source, -1,
                                     destination, capacity, NULL, NULL);
    return length > 0;
}

static const wchar_t *quadrantName(enum Kuadran quadrant)
{
    switch (quadrant)
    {
    case PENTING_MENDESAK:
        return L"I. Penting & mendesak";
    case PENTING_TIDAK_MENDESAK:
        return L"II. Penting & tidak mendesak";
    case TIDAK_PENTING_MENDESAK:
        return L"III. Tidak penting & mendesak";
    case TIDAK_PENTING_TIDAK_MENDESAK:
        return L"IV. Tidak penting & tidak mendesak";
    default:
        return L"Deadline tidak valid";
    }
}

static const wchar_t *urgencyName(int statusDeadline)
{
    if (statusDeadline == -9999)
        return L"Tidak diketahui";
    if (statusDeadline < 0)
        return L"Terlambat";
    if (statusDeadline == 0)
        return L"Hari ini";
    if (statusDeadline == 1)
        return L"Mendesak (1 hari)";
    if (statusDeadline <= 3)
        return L"Mendesak (≤3 hari)";
    return L"Terjadwal";
}

static void setControlFont(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
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
        setControlFont(control);
    return control;
}

static void updateDetails(int id)
{
    int index = cariIndexByID(&g_daftar, id);
    if (index < 0)
    {
        SetWindowTextW(g_details, L"Pilih tugas untuk melihat detailnya.");
        g_selectedID = 0;
        return;
    }

    struct Tugas *task = &g_daftar.data[index];
    wchar_t name[512];
    wchar_t details[1200];
    utf8ToWide(task->nama, name, (int)(sizeof(name) / sizeof(name[0])));
    swprintf(details, sizeof(details) / sizeof(details[0]),
             L"ID %d    •    %ls\r\nPrioritas: %ls    •    Deadline: %02d-%02d-%04d\r\n"
             L"Status: %ls    •    Sisa hari: %d    •    Urgensi: %ls    •    Kuadran: %ls",
             task->id, name,
             task->penting ? L"Penting" : L"Tidak penting",
             task->deadline.tanggal, task->deadline.bulan, task->deadline.tahun,
             task->status == TUGAS_SELESAI ? L"Selesai" : L"Belum selesai",
             hitungSelisihHari(&task->deadline),
             urgencyName(cekStatusDeadline(task)), quadrantName(tentukanKuadran(task)));
    SetWindowTextW(g_details, details);
    g_selectedID = id;
}

static void setStatusText(const wchar_t *message)
{
    if (g_status != NULL)
        SetWindowTextW(g_status, message);
}

static void saveData(void)
{
    if (simpanTugas(&g_daftar, g_dataFile))
    {
        wchar_t message[160];
        swprintf(message, sizeof(message) / sizeof(message[0]),
                 L"Tersimpan • %d tugas", g_daftar.jumlah);
        setStatusText(message);
    }
    else
    {
        MessageBoxW(g_mainWindow,
                    L"Data gagal disimpan. Periksa lokasi file dan izin folder.",
                    L"Gagal menyimpan", MB_OK | MB_ICONERROR);
        setStatusText(L"Perubahan belum tersimpan");
    }
}

static void addListColumn(int index, const wchar_t *title, int width)
{
    LVCOLUMNW column = {0};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = (LPWSTR)title;
    column.cx = width;
    column.iSubItem = index;
    ListView_InsertColumn(g_list, index, &column);
}

static void addMatrixGroups(void)
{
    const wchar_t *headers[] = {
        L"I. PENTING & MENDESAK",
        L"II. PENTING & TIDAK MENDESAK",
        L"III. TIDAK PENTING & MENDESAK",
        L"IV. TIDAK PENTING & TIDAK MENDESAK",
        L"DEADLINE TIDAK VALID"};

    for (int i = 0; i < 5; i++)
    {
        LVGROUP group = {0};
        group.cbSize = sizeof(group);
        group.mask = LVGF_HEADER | LVGF_GROUPID;
        group.pszHeader = (LPWSTR)headers[i];
        group.iGroupId = i + 1;
        ListView_InsertGroup(g_list, -1, &group);
    }
}

static void refreshList(void)
{
    if (g_list == NULL)
        return;

    wchar_t queryWide[256];
    char query[512];
    GetWindowTextW(g_search, queryWide,
                   (int)(sizeof(queryWide) / sizeof(queryWide[0])));
    wideToUtf8(queryWide, query, sizeof(query));

    int filter = (int)SendMessageW(g_filter, CB_GETCURSEL, 0, 0);
    if (filter < 0)
        filter = FILTER_SEMUA;
    int matrix = filter == FILTER_MATRIX;

    ListView_DeleteAllItems(g_list);
    ListView_RemoveAllGroups(g_list);
    ListView_EnableGroupView(g_list, matrix);
    if (matrix)
        addMatrixGroups();

    int firstMatch = query[0] == '\0' ? -1 : cariIndexByNama(&g_daftar, query);
    if (query[0] != '\0' && firstMatch < 0)
    {
        setStatusText(L"Tidak ada tugas dengan nama tersebut");
        updateDetails(0);
        return;
    }

    for (int i = 0; i < g_daftar.jumlah; i++)
    {
        struct Tugas *task = &g_daftar.data[i];
        if (task->nama == NULL)
            continue;
        if (query[0] != '\0' && !namaMengandung(task->nama, query))
            continue;
        if (filter == FILTER_BELUM_SELESAI && task->status != TUGAS_BELUM_SELESAI)
            continue;
        if (filter == FILTER_SELESAI && task->status != TUGAS_SELESAI)
            continue;
        if (filter == FILTER_OVERDUE &&
            (task->status != TUGAS_BELUM_SELESAI || cekStatusDeadline(task) != -1))
            continue;
        if (matrix && task->status != TUGAS_BELUM_SELESAI)
            continue;

        wchar_t idText[24];
        wchar_t name[512];
        wchar_t priority[40];
        wchar_t deadline[40];
        wchar_t urgency[64];
        wchar_t status[40];
        wchar_t quadrant[100];
        swprintf(idText, 24, L"%d", task->id);
        utf8ToWide(task->nama, name, 512);
        wcscpy(priority, task->penting ? L"Penting" : L"Tidak penting");
        swprintf(deadline, 40, L"%02d-%02d-%04d",
                 task->deadline.tanggal, task->deadline.bulan, task->deadline.tahun);
        wcscpy(urgency, urgencyName(cekStatusDeadline(task)));
        wcscpy(status, task->status == TUGAS_SELESAI ? L"Selesai" : L"Belum selesai");
        wcscpy(quadrant, quadrantName(tentukanKuadran(task)));

        int row = ListView_GetItemCount(g_list);
        LVITEMW item = {0};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        if (matrix)
        {
            item.mask |= LVIF_GROUPID;
            item.iGroupId = (int)tentukanKuadran(task) + 1;
        }
        item.iItem = row;
        item.pszText = idText;
        item.lParam = task->id;
        ListView_InsertItem(g_list, &item);
        ListView_SetItemText(g_list, row, 1, name);
        ListView_SetItemText(g_list, row, 2, priority);
        ListView_SetItemText(g_list, row, 3, deadline);
        ListView_SetItemText(g_list, row, 4, urgency);
        ListView_SetItemText(g_list, row, 5, status);
        ListView_SetItemText(g_list, row, 6, quadrant);
    }

    if (query[0] == '\0')
    {
        wchar_t message[160];
        swprintf(message, sizeof(message) / sizeof(message[0]),
                 L"%d tugas • %ls", ListView_GetItemCount(g_list),
                 matrix ? L"Tampilan Eisenhower Matrix" : L"Data terbaru");
        setStatusText(message);
    }

    if (g_selectedID != 0 && cariIndexByID(&g_daftar, g_selectedID) >= 0)
        updateDetails(g_selectedID);
    else
        updateDetails(0);
}

static void closeEditor(HWND window)
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
        HWND label = addControl(0, L"STATIC", L"Nama tugas", WS_VISIBLE,
                                24, 20, 350, 24, window, 0);
        HWND name = addControl(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               ES_AUTOHSCROLL | WS_TABSTOP,
                               24, 45, 370, 32, window, ID_EDITOR_NAME);
        addControl(0, L"STATIC", L"Deadline (TT-BB-YYYY)", WS_VISIBLE,
                   24, 92, 220, 24, window, 0);
        HWND date = addControl(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               ES_AUTOHSCROLL | WS_TABSTOP,
                               24, 117, 180, 32, window, ID_EDITOR_DATE);
        HWND important = addControl(0, L"BUTTON", L"Penting",
                                    BS_AUTOCHECKBOX | WS_TABSTOP,
                                    24, 165, 180, 28, window, ID_EDITOR_IMPORTANT);
        addControl(0, L"BUTTON", L"Simpan", BS_DEFPUSHBUTTON | WS_TABSTOP,
                   218, 215, 82, 34, window, ID_EDITOR_SAVE);
        addControl(0, L"BUTTON", L"Batal", BS_PUSHBUTTON | WS_TABSTOP,
                   312, 215, 82, 34, window, ID_EDITOR_CANCEL);
        setControlFont(label);

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

    if (message == WM_COMMAND)
    {
        int command = LOWORD(wParam);
        if (command == ID_EDITOR_CANCEL)
        {
            closeEditor(window);
            return 0;
        }
        if (command == ID_EDITOR_SAVE)
        {
            wchar_t wideName[256];
            wchar_t wideDate[64];
            char name[600];
            int day, month, year;
            GetWindowTextW(GetDlgItem(window, ID_EDITOR_NAME), wideName, 256);
            GetWindowTextW(GetDlgItem(window, ID_EDITOR_DATE), wideDate, 64);

            if (!wideToUtf8(wideName, name, sizeof(name)) ||
                swscanf(wideDate, L"%d-%d-%d %c", &day, &month, &year, &(wchar_t){0}) != 3)
            {
                MessageBoxW(window, L"Isi nama dan deadline dengan format TT-BB-YYYY.",
                            L"Data belum valid", MB_OK | MB_ICONWARNING);
                return 0;
            }

            struct Tanggal deadline = {day, month, year};
            if (!validasiTanggalKalender(&deadline) ||
                !validasiInputTugas(name,
                    SendMessageW(GetDlgItem(window, ID_EDITOR_IMPORTANT), BM_GETCHECK, 0, 0) == BST_CHECKED))
            {
                MessageBoxW(window, L"Nama atau tanggal tidak valid.",
                            L"Data belum valid", MB_OK | MB_ICONWARNING);
                return 0;
            }

            int important = SendMessageW(GetDlgItem(window, ID_EDITOR_IMPORTANT),
                                         BM_GETCHECK, 0, 0) == BST_CHECKED;
            int success;
            if (state != NULL && state->edit)
                success = editTugas(&g_daftar, state->id, name, important, deadline);
            else
                success = tambahTugas(&g_daftar, name, important, deadline);

            if (!success)
            {
                MessageBoxW(window, L"Tugas tidak dapat disimpan. Periksa data atau kapasitas.",
                            L"Gagal menyimpan tugas", MB_OK | MB_ICONERROR);
                return 0;
            }

            if (state != NULL && !state->edit)
                g_selectedID = g_daftar.idBerikut - 1;
            else if (state != NULL)
                g_selectedID = state->id;
            saveData();
            refreshList();
            closeEditor(window);
            return 0;
        }
    }

    if (message == WM_CLOSE)
    {
        closeEditor(window);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void openEditor(int edit)
{
    if (edit && g_selectedID == 0)
    {
        MessageBoxW(g_mainWindow, L"Pilih tugas yang ingin diedit.",
                    L"Belum ada pilihan", MB_OK | MB_ICONINFORMATION);
        return;
    }

    struct EditorState state = {g_selectedID, edit};
    HWND editor = CreateWindowExW(WS_EX_DLGMODALFRAME,
                                  L"EisenhowerTaskEditor",
                                  edit ? L"Edit tugas" : L"Tambah tugas",
                                  WS_CAPTION | WS_SYSMENU | WS_POPUP,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 440, 305,
                                  g_mainWindow, NULL, GetModuleHandleW(NULL), &state);
    if (editor == NULL)
        return;

    RECT ownerRect;
    GetWindowRect(g_mainWindow, &ownerRect);
    SetWindowPos(editor, HWND_TOP,
                 ownerRect.left + 120, ownerRect.top + 100,
                 440, 305, SWP_SHOWWINDOW);
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
    {
        MessageBoxW(g_mainWindow, L"Pilih tugas yang ingin dihapus.",
                    L"Belum ada pilihan", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxW(g_mainWindow, L"Hapus tugas yang dipilih?",
                    L"Konfirmasi hapus", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    if (hapusTugas(&g_daftar, g_selectedID))
    {
        g_selectedID = 0;
        saveData();
        refreshList();
    }
}

static void toggleSelectedStatus(void)
{
    int index = cariIndexByID(&g_daftar, g_selectedID);
    if (index < 0)
    {
        MessageBoxW(g_mainWindow, L"Pilih tugas terlebih dahulu.",
                    L"Belum ada pilihan", MB_OK | MB_ICONINFORMATION);
        return;
    }

    enum StatusTugas status = g_daftar.data[index].status == TUGAS_SELESAI
                                  ? TUGAS_BELUM_SELESAI
                                  : TUGAS_SELESAI;
    if (ubahStatusTugas(&g_daftar, g_selectedID, status))
    {
        saveData();
        refreshList();
    }
}

static void layoutControls(HWND window)
{
    RECT client;
    GetClientRect(window, &client);
    int width = client.right;
    int height = client.bottom;
    int actionY = 98;

    MoveWindow(g_search, 24, actionY, 270, 34, TRUE);
    MoveWindow(g_filter, 306, actionY, 190, 34, TRUE);
    MoveWindow(GetDlgItem(window, ID_ADD), width - 456, actionY, 82, 34, TRUE);
    MoveWindow(GetDlgItem(window, ID_EDIT), width - 366, actionY, 68, 34, TRUE);
    MoveWindow(GetDlgItem(window, ID_DELETE), width - 290, actionY, 68, 34, TRUE);
    MoveWindow(GetDlgItem(window, ID_TOGGLE), width - 214, actionY, 104, 34, TRUE);
    MoveWindow(GetDlgItem(window, ID_SAVE), width - 102, actionY, 78, 34, TRUE);
    MoveWindow(g_list, 24, 148, width - 48, height - 274, TRUE);
    MoveWindow(g_details, 24, height - 112, width - 48, 62, TRUE);
    MoveWindow(g_status, 24, height - 38, width - 48, 22, TRUE);
}

static LRESULT CALLBACK mainProcedure(HWND window, UINT message,
                                      WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        int actionY = 98;
        g_search = addControl(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              ES_AUTOHSCROLL | WS_TABSTOP,
                              24, 98, 270, 34, window, ID_SEARCH);
        SendMessageW(g_search, EM_SETCUEBANNER, TRUE,
                     (LPARAM)L"Cari nama tugas...");
        g_filter = addControl(0, WC_COMBOBOXW, L"",
                              CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                              306, 98, 190, 160, window, ID_FILTER);
        const wchar_t *filters[] = {L"Semua tugas", L"Belum selesai", L"Selesai",
                                    L"Terlambat", L"Eisenhower Matrix"};
        for (int i = 0; i < 5; i++)
            SendMessageW(g_filter, CB_ADDSTRING, 0, (LPARAM)filters[i]);
        SendMessageW(g_filter, CB_SETCURSEL, FILTER_SEMUA, 0);

        addControl(0, L"BUTTON", L"+ Tambah", BS_PUSHBUTTON | WS_TABSTOP,
                   0, actionY, 88, 34, window, ID_ADD);
        addControl(0, L"BUTTON", L"Edit", BS_PUSHBUTTON | WS_TABSTOP,
                   0, actionY, 78, 34, window, ID_EDIT);
        addControl(0, L"BUTTON", L"Hapus", BS_PUSHBUTTON | WS_TABSTOP,
                   0, actionY, 78, 34, window, ID_DELETE);
        addControl(0, L"BUTTON", L"Ubah status", BS_PUSHBUTTON | WS_TABSTOP,
                   0, actionY, 122, 34, window, ID_TOGGLE);
        addControl(0, L"BUTTON", L"Simpan", BS_PUSHBUTTON | WS_TABSTOP,
                   0, actionY, 82, 34, window, ID_SAVE);

        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                 WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
                                 24, 148, 1000, 450, window,
                                 (HMENU)(INT_PTR)ID_LIST,
                                 GetModuleHandleW(NULL), NULL);
        setControlFont(g_list);
        ListView_SetExtendedListViewStyle(g_list,
                                           LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        addListColumn(0, L"ID", 56);
        addListColumn(1, L"Nama tugas", 270);
        addListColumn(2, L"Prioritas", 120);
        addListColumn(3, L"Deadline", 120);
        addListColumn(4, L"Urgensi", 130);
        addListColumn(5, L"Status", 120);
        addListColumn(6, L"Kuadran", 240);

        g_details = addControl(0, L"STATIC", L"Pilih tugas untuk melihat detailnya.",
                               SS_LEFT | SS_NOPREFIX, 24, 610, 1000, 62,
                               window, ID_DETAILS);
        g_status = addControl(0, L"STATIC", L"Memuat data...", SS_LEFT,
                              24, 680, 1000, 22, window, ID_STATUS);
        layoutControls(window);
        refreshList();
        return 0;
    }
    case WM_SIZE:
        layoutControls(window);
        return 0;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lParam;
        limits->ptMinTrackSize.x = 900;
        limits->ptMinTrackSize.y = 620;
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT rect;
        GetClientRect(window, &rect);
        RECT header = {0, 0, rect.right, 78};
        HBRUSH brush = CreateSolidBrush(COLOR_NAVY);
        FillRect(dc, &header, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        HFONT oldFont = (HFONT)SelectObject(dc, g_font);
        RECT title = {24, 13, rect.right - 24, 42};
        DrawTextW(dc, L"Eisenhower Matrix", -1, &title,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT subtitle = {24, 43, rect.right - 24, 68};
        DrawTextW(dc, L"Atur prioritas, pantau deadline, dan selesaikan tugas dengan fokus.",
                  -1, &subtitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        EndPaint(window, &paint);
        return 0;
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
            saveData();
            return 0;
        case ID_FILTER:
            if (HIWORD(wParam) == CBN_SELCHANGE)
                refreshList();
            return 0;
        case ID_SEARCH:
            if (HIWORD(wParam) == EN_CHANGE)
                refreshList();
            return 0;
        }
        break;
    case WM_NOTIFY:
    {
        NMHDR *header = (NMHDR *)lParam;
        if (header->idFrom == ID_LIST && header->code == LVN_ITEMCHANGED)
        {
            NMLISTVIEW *change = (NMLISTVIEW *)lParam;
            if ((change->uNewState & LVIS_SELECTED) != 0)
            {
                LVITEMW item = {0};
                item.mask = LVIF_PARAM;
                item.iItem = change->iItem;
                if (ListView_GetItem(g_list, &item))
                    updateDetails((int)item.lParam);
            }
            return 0;
        }
        if (header->idFrom == ID_LIST && header->code == NM_DBLCLK)
        {
            openEditor(1);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        saveData();
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        freeDaftarTugas(&g_daftar);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static int registerWindowClass(HINSTANCE instance, const wchar_t *name,
                               WNDPROC procedure, UINT style, HBRUSH background)
{
    WNDCLASSEXW cls = {0};
    cls.cbSize = sizeof(cls);
    cls.style = style;
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
        strcpy(g_dataFile, "tugas.txt");
        return;
    }

    char *slash = strrchr(g_dataFile, '\\');
    char *forwardSlash = strrchr(g_dataFile, '/');
    if (forwardSlash != NULL && (slash == NULL || forwardSlash > slash))
        slash = forwardSlash;
    if (slash == NULL || (size_t)(slash - g_dataFile) + 12 >= sizeof(g_dataFile))
    {
        strcpy(g_dataFile, "tugas.txt");
        return;
    }
    strcpy(slash + 1, "tugas.txt");
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR commandLine, int showCommand)
{
    (void)previous;
    (void)commandLine;

    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                         L"Segoe UI");
    if (g_font == NULL)
        g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    if (!registerWindowClass(instance, L"EisenhowerMainWindow", mainProcedure,
                             CS_HREDRAW | CS_VREDRAW, (HBRUSH)(COLOR_WINDOW + 1)) ||
        !registerWindowClass(instance, L"EisenhowerTaskEditor", editorProcedure,
                             CS_HREDRAW | CS_VREDRAW, (HBRUSH)(COLOR_WINDOW + 1)))
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
    int loaded = muatTugas(&g_daftar, g_dataFile);
    if (!loaded && GetFileAttributesA(g_dataFile) != INVALID_FILE_ATTRIBUTES)
    {
        freeDaftarTugas(&g_daftar);
        MessageBoxW(NULL,
                    L"File tugas.txt tersedia tetapi tidak dapat dibaca. File tidak akan ditimpa.",
                    L"Data tugas gagal dimuat", MB_OK | MB_ICONERROR);
        if (g_font != NULL && GetObjectType(g_font) != 0)
            DeleteObject(g_font);
        return 1;
    }

    g_mainWindow = CreateWindowExW(0, L"EisenhowerMainWindow",
                                   L"Eisenhower Matrix — Manajemen Tugas",
                                   WS_OVERLAPPEDWINDOW,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760,
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
    wchar_t startup[160];
    if (loaded)
        swprintf(startup, 160, L"%d tugas dimuat dari tugas.txt", g_daftar.jumlah);
    else
        swprintf(startup, 160, L"Siap • data akan disimpan di samping aplikasi");
    setStatusText(startup);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_font != NULL && GetObjectType(g_font) != 0)
        DeleteObject(g_font);
    return (int)message.wParam;
}
