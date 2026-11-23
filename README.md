# Eisenhower Matrix

Aplikasi desktop Windows untuk mengelola tugas. Backend C berada di `src/latihan.c`, sedangkan GUI native Win32 berada di `src/gui/gui_win32.c`. Backend menangani dynamic array, validasi, sorting deadline, CRUD, pencarian, matrix, dan file TXT.

## Build dan jalankan

Gunakan GCC MinGW-w64, misalnya terminal MSYS2 UCRT64. Jalankan `build_gui.bat`, lalu buka `build/EisenhowerMatrix.exe` untuk aplikasi GUI. CLI pada `src/latihan.c` dan GUI memakai file data yang sama, yaitu `data/tugas.txt`.

Build manual dari terminal MSYS2 UCRT64:

```sh
cd resources
windres gui_resources.rc -O coff -o ../build/gui_resources.o
cd ..
gcc src/gui/gui_win32.c build/gui_resources.o -I src -o build/EisenhowerMatrix.exe -std=c11 -Wall -Wextra -Wpedantic -mwindows -lcomctl32
```

## Struktur folder

- `src/` berisi backend C dan GUI Win32.
- `resources/` berisi resource GUI dan manifest aplikasi.
- `data/` berisi data tugas aplikasi.
- `tests/` berisi executable dan data pengujian.
- `build/` berisi hasil build.
- `docs/` disediakan untuk dokumentasi tambahan.

## Fitur

- Tambah tugas dengan nama, deadline, dan prioritas.
- Edit, hapus, dan ubah status tugas.
- Simpan otomatis setelah perubahan, serta tombol simpan manual.
- Cari nama tugas dengan pencarian linear tanpa membedakan huruf besar/kecil.
- Filter semua, belum selesai, selesai, dan overdue.
- Dashboard ringkasan total tugas, selesai, belum selesai, dan overdue.
- Tampilan Matrix empat kuadran sebagai fokus dashboard, dengan urgensi dan deadline.
- Panel detail tugas yang menampilkan sisa hari, urgensi, dan kuadran.
- Navigasi sidebar, empty state, feedback aksi, dan daftar yang mendukung scroll.

Format deadline pada form adalah `TT-BB-YYYY`. Tugas overdue dapat diedit agar deadline dapat diperbaiki.
