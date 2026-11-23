# Eisenhower Matrix

Aplikasi desktop Windows untuk mengelola tugas dengan backend C yang sudah ada di `latihan.c`. GUI native Win32 menjadi antarmuka utama; backend tetap menangani dynamic array, validasi, sorting deadline, CRUD, pencarian, matrix, dan file TXT.

## Build dan jalankan

Gunakan GCC MinGW-w64, misalnya terminal MSYS2 UCRT64. Jalankan `build_gui.bat`, lalu buka `EisenhowerMatrix.exe`. Data aplikasi disimpan di `tugas.txt` di folder yang sama dengan executable.

Untuk membangun dari terminal:

```sh
gcc gui_win32.c -o EisenhowerMatrix.exe -std=c11 -Wall -Wextra -Wpedantic -mwindows -lcomctl32
```

## Fitur

- Tambah tugas dengan nama, deadline, dan prioritas.
- Edit, hapus, dan ubah status tugas.
- Simpan otomatis setelah perubahan, serta tombol simpan manual.
- Cari nama tugas dengan pencarian linear tanpa membedakan huruf besar/kecil.
- Filter semua, belum selesai, selesai, dan overdue.
- Tampilan Matrix dengan empat grup kuadran, urgensi, deadline, dan status.
- Panel detail tugas yang menampilkan sisa hari, urgensi, dan kuadran.

Format deadline pada form adalah `TT-BB-YYYY`. Tugas overdue dapat diedit agar deadline dapat diperbaiki.
