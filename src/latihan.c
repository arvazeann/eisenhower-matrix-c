#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>

enum StatusTugas
{
    TUGAS_BELUM_SELESAI,
    TUGAS_SELESAI
};

enum Kuadran
{
    PENTING_MENDESAK,
    PENTING_TIDAK_MENDESAK,
    TIDAK_PENTING_MENDESAK,
    TIDAK_PENTING_TIDAK_MENDESAK,
    KUADRAN_TIDAK_VALID
};

struct Tanggal
{
    int tanggal;
    int bulan;
    int tahun;
};

struct Tugas
{
    int id;
    char *nama;
    int penting;
    struct Tanggal deadline;
    enum StatusTugas status;
};

struct DaftarTugas
{
    struct Tugas *data;
    int jumlah;
    int kapasitas;
    int idBerikut;
};

struct StatistikTugas
{
    int total;
    int selesai;
    int belumSelesai;
    int overdue;
};

// Inisialisasi dan pembersihan
void initDaftarTugas(struct DaftarTugas *daftar)
{
    if (daftar == NULL)
        return;

    daftar->jumlah = 0;
    daftar->kapasitas = 2;
    daftar->idBerikut = 1;

    daftar->data = malloc(daftar->kapasitas * sizeof(struct Tugas));

    if (daftar->data == NULL)
    {
        daftar->kapasitas = 0;
    }
}

void freeDaftarTugas(struct DaftarTugas *daftar)
{
    if (daftar == NULL)
        return;

    if (daftar->data != NULL)
    {
        int batas = daftar->jumlah;

        if (batas < 0)
            batas = 0;

        if (batas > daftar->kapasitas)
            batas = daftar->kapasitas;

        for (int i = 0; i < batas; i++)
        {
            free(daftar->data[i].nama);
        }
    }

    free(daftar->data);
    daftar->data = NULL;
    daftar->jumlah = 0;
    daftar->kapasitas = 0;
    daftar->idBerikut = 1;
}
// Validasi tanggal
struct Tanggal ambilTanggalSekarang(void)
{
    time_t sekarang = time(NULL);

    if (sekarang == (time_t)-1)
    {
        return (struct Tanggal){0, 0, 0};
    }

    struct tm *waktu = localtime(&sekarang);

    if (waktu == NULL)
    {
        return (struct Tanggal){0, 0, 0};
    }

    struct Tanggal hariIni;

    hariIni.tanggal = waktu->tm_mday;
    hariIni.bulan = waktu->tm_mon + 1;
    hariIni.tahun = waktu->tm_year + 1900;

    return hariIni;
}

int validasiTahunKabisat(int tahun)
{
    if (tahun % 400 == 0)
    {
        return 1;
    }
    else if (tahun % 100 == 0)
    {
        return 0;
    }
    else if (tahun % 4 == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int validasiTanggalKalender(struct Tanggal *tanggal)
{
    if (tanggal == NULL)
    {
        return 0;
    }

    if (tanggal->tahun < 1)
    {
        return 0;
    }

    if (tanggal->bulan < 1 || tanggal->bulan > 12)
    {
        return 0;
    }

    int hariMaks;

    if (tanggal->bulan == 2)
    {
        if (validasiTahunKabisat(tanggal->tahun))
        {
            hariMaks = 29;
        }
        else
        {
            hariMaks = 28;
        }
    }
    else if (tanggal->bulan == 4 || tanggal->bulan == 6 ||
             tanggal->bulan == 9 || tanggal->bulan == 11)
    {
        hariMaks = 30;
    }
    else
    {
        hariMaks = 31;
    }

    if (tanggal->tanggal < 1 || tanggal->tanggal > hariMaks)
    {
        return 0;
    }

    return 1;
}

int validasiTanggal(struct Tanggal *tanggal)
{
    if (!validasiTanggalKalender(tanggal))
    {
        return 0;
    }

    struct Tanggal sekarang = ambilTanggalSekarang();

    if (!validasiTanggalKalender(&sekarang))
    {
        return 0;
    }

    if (tanggal->tahun < sekarang.tahun)
        return 0;

    if (tanggal->tahun == sekarang.tahun)
    {
        if (tanggal->bulan < sekarang.bulan)
            return 0;

        if (tanggal->bulan == sekarang.bulan &&
            tanggal->tanggal < sekarang.tanggal)
            return 0;
    }

    return 1;
}

int hitungSelisihHari(struct Tanggal *tanggal)
{
    if (!validasiTanggalKalender(tanggal))
        return -9999;

    struct Tanggal sekarang = ambilTanggalSekarang();

    if (!validasiTanggalKalender(&sekarang))
        return -9999;

    struct tm waktuSekarang = {0};
    struct tm waktuDeadline = {0};

    waktuSekarang.tm_mday = sekarang.tanggal;
    waktuSekarang.tm_mon = sekarang.bulan - 1;
    waktuSekarang.tm_year = sekarang.tahun - 1900;
    waktuSekarang.tm_hour = 12;

    waktuDeadline.tm_mday = tanggal->tanggal;
    waktuDeadline.tm_mon = tanggal->bulan - 1;
    waktuDeadline.tm_year = tanggal->tahun - 1900;
    waktuDeadline.tm_hour = 12;

    time_t waktu1 = mktime(&waktuSekarang);
    time_t waktu2 = mktime(&waktuDeadline);

    if (waktu1 == (time_t)-1 || waktu2 == (time_t)-1)
        return -9999;

    double selisih = difftime(waktu2, waktu1);
    double jumlahHari = selisih / 86400.0;

    if (jumlahHari < INT_MIN || jumlahHari > INT_MAX)
        return -9999;

    return (int)jumlahHari;
}
// Pengurutan deadline
int bandingkanTanggal(struct Tanggal a, struct Tanggal b)
{
    if (a.tahun != b.tahun)
        return (a.tahun > b.tahun) ? 1 : -1;

    if (a.bulan != b.bulan)
        return (a.bulan > b.bulan) ? 1 : -1;

    if (a.tanggal != b.tanggal)
        return (a.tanggal > b.tanggal) ? 1 : -1;

    return 0;
}

void insertionSortDeadline(struct DaftarTugas *daftar)
{
    if (daftar == NULL || daftar->data == NULL ||
        daftar->jumlah < 2 || daftar->jumlah > daftar->kapasitas)
    {
        return;
    }

    for (int i = 1; i < daftar->jumlah; i++)
    {
        struct Tugas key = daftar->data[i];

        int j = i - 1;

        while (
            j >= 0 &&
            bandingkanTanggal(daftar->data[j].deadline, key.deadline) > 0)
        {
            daftar->data[j + 1] = daftar->data[j];
            j--;
        }

        daftar->data[j + 1] = key;
    }
}
// Klasifikasi urgensi dan kuadran
int cekStatusDeadline(struct Tugas *tugas)
{
    if (tugas == NULL)
    {
        return -9999;
    }

    int sisaHari = hitungSelisihHari(&tugas->deadline);

    if (sisaHari == -9999)
    {
        return -9999;
    }
    else if (sisaHari < 0)
    {
        return -1;
    }
    else if (sisaHari == 0)
    {
        return 0;
    }
    else if (sisaHari <= 3)
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

enum Kuadran tentukanKuadran(struct Tugas *tugas)
{
    if (tugas == NULL || (tugas->penting != 0 && tugas->penting != 1))
    {
        return KUADRAN_TIDAK_VALID;
    }

    int statusDeadline = cekStatusDeadline(tugas);

    if (statusDeadline == -9999)
    {
        return KUADRAN_TIDAK_VALID;
    }

    if (tugas->penting == 1)
    {
        if (statusDeadline == -1 ||
            statusDeadline == 0 ||
            statusDeadline == 1)
        {
            return PENTING_MENDESAK;
        }
        else
        {
            return PENTING_TIDAK_MENDESAK;
        }
    }
    else
    {
        if (statusDeadline == -1 ||
            statusDeadline == 0 ||
            statusDeadline == 1)
        {
            return TIDAK_PENTING_MENDESAK;
        }
        else
        {
            return TIDAK_PENTING_TIDAK_MENDESAK;
        }
    }
}
// Validasi, kapasitas, dan pencarian
int validasiInputTugas(const char *nama, int penting)
{
    if (nama == NULL || nama[0] == '\0')
        return 0;

    if (penting != 0 && penting != 1)
        return 0;

    int adaKarakter = 0;
    int panjang = 0;

    while (nama[panjang] != '\0')
    {
        unsigned char karakter = (unsigned char)nama[panjang];

        if (karakter == '|' || karakter == '\n' || karakter == '\r')
            return 0;

        if (!isspace(karakter))
            adaKarakter = 1;

        panjang++;

        if (panjang > 199)
            return 0;
    }

    return adaKarakter;
}

int ubahKapasitas(struct DaftarTugas *daftar, int kapasitasBaru)
{
    if (daftar == NULL)
        return 0;

    if (kapasitasBaru < 2 ||
        daftar->kapasitas < 0 ||
        daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas ||
        kapasitasBaru < daftar->jumlah)
    {
        return 0;
    }

    if (daftar->data == NULL && daftar->jumlah != 0)
        return 0;

    if ((size_t)kapasitasBaru > SIZE_MAX / sizeof(struct Tugas))
        return 0;

    struct Tugas *temp = realloc(
        daftar->data,
        (size_t)kapasitasBaru * sizeof(struct Tugas));

    if (temp == NULL)
        return 0;

    daftar->data = temp;
    daftar->kapasitas = kapasitasBaru;

    return 1;
}

int cariIndexByID(struct DaftarTugas *daftar, int id)
{
    if (daftar == NULL || daftar->data == NULL ||
        daftar->jumlah < 0 || daftar->jumlah > daftar->kapasitas ||
        id <= 0)
    {
        return -1;
    }

    for (int i = 0; i < daftar->jumlah; i++)
    {
        if (daftar->data[i].id == id)
        {
            return i;
        }
    }

    return -1;
}

int namaMengandung(const char *nama, const char *kueri)
{
    if (nama == NULL || kueri == NULL || kueri[0] == '\0')
        return 0;

    for (const char *awal = nama; *awal != '\0'; awal++)
    {
        const char *teks = awal;
        const char *cari = kueri;

        while (*teks != '\0' && *cari != '\0' &&
               tolower((unsigned char)*teks) == tolower((unsigned char)*cari))
        {
            teks++;
            cari++;
        }

        if (*cari == '\0')
            return 1;
    }

    return 0;
}

int cariIndexByNama(struct DaftarTugas *daftar, const char *kueri)
{
    if (daftar == NULL || daftar->data == NULL || kueri == NULL ||
        kueri[0] == '\0' || daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return -1;
    }

    for (int i = 0; i < daftar->jumlah; i++)
    {
        const char *nama = daftar->data[i].nama;
        if (nama == NULL)
            continue;

        if (namaMengandung(nama, kueri))
            return i;
    }

    return -1;
}

// Fungsi ini ditambahkan oleh Codex.
int cariSemuaIndexByNama(struct DaftarTugas *daftar, const char *kueri, int *hasil, int kapasitasHasil)
{
    if (daftar == NULL || daftar->data == NULL || kueri == NULL ||
        kueri[0] == '\0' || daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas || kapasitasHasil < 0 ||
        (kapasitasHasil > 0 && hasil == NULL))
    {
        return -1;
    }

    int jumlahCocok = 0;
    for (int i = 0; i < daftar->jumlah; i++)
    {
        const char *nama = daftar->data[i].nama;
        if (nama == NULL || !namaMengandung(nama, kueri))
            continue;

        if (jumlahCocok < kapasitasHasil)
            hasil[jumlahCocok] = i;
        jumlahCocok++;
    }

    return jumlahCocok;
}

// Fungsi ini ditambahkan oleh Codex.
int ambilIndexTugasDenganFilter(struct DaftarTugas *daftar, int statusFilter,
                               int hanyaOverdue, const char *kueri,
                               int *hasil, int kapasitasHasil)
{
    if (daftar == NULL || daftar->data == NULL || daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas || kapasitasHasil < 0 ||
        (kapasitasHasil > 0 && hasil == NULL) ||
        (statusFilter != -1 && statusFilter != TUGAS_BELUM_SELESAI &&
         statusFilter != TUGAS_SELESAI) ||
        (hanyaOverdue != 0 && hanyaOverdue != 1))
    {
        return -1;
    }

    int jumlahCocok = 0;
    for (int i = 0; i < daftar->jumlah; i++)
    {
        struct Tugas *tugas = &daftar->data[i];
        if (tugas->nama == NULL ||
            (statusFilter >= 0 && (int)tugas->status != statusFilter) ||
            (hanyaOverdue && (tugas->status != TUGAS_BELUM_SELESAI ||
                              cekStatusDeadline(tugas) != -1)) ||
            (kueri != NULL && kueri[0] != '\0' &&
             !namaMengandung(tugas->nama, kueri)))
        {
            continue;
        }

        if (jumlahCocok < kapasitasHasil)
            hasil[jumlahCocok] = i;
        jumlahCocok++;
    }

    return jumlahCocok;
}

// Fungsi ini ditambahkan oleh Codex.
struct Tugas *ambilDetailTugas(struct DaftarTugas *daftar, int id)
{
    int index = cariIndexByID(daftar, id);
    return index < 0 ? NULL : &daftar->data[index];
}

// Operasi CRUD
int tambahTugas(struct DaftarTugas *daftar, const char *nama, int penting, struct Tanggal deadline)
{
    if (daftar == NULL || daftar->data == NULL)
        return 0;

    if (daftar->kapasitas < 2 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return 0;
    }

    if (daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas ||
        daftar->idBerikut <= 0 ||
        daftar->idBerikut == INT_MAX)
    {
        return 0;
    }

    if (!validasiInputTugas(nama, penting))
        return 0;

    if (!validasiTanggal(&deadline))
    {
        return 0;
    }

    if (daftar->jumlah == daftar->kapasitas)
    {
        if (daftar->kapasitas > INT_MAX / 2)
            return 0;

        if (!ubahKapasitas(daftar, daftar->kapasitas * 2))
            return 0;
    }

    char *salinan = malloc(strlen(nama) + 1);

    if (salinan == NULL)
    {
        return 0;
    }

    strcpy(salinan, nama);

    int index = daftar->jumlah;

    daftar->data[index].id = daftar->idBerikut;
    daftar->data[index].nama = salinan;
    daftar->data[index].penting = penting;
    daftar->data[index].deadline = deadline;
    daftar->data[index].status = TUGAS_BELUM_SELESAI;

    daftar->jumlah++;
    daftar->idBerikut++;

    insertionSortDeadline(daftar);

    return 1;
}

int editTugas(struct DaftarTugas *daftar, int id, const char *namaBaru, int pentingBaru, struct Tanggal deadlineBaru)
{
    if (daftar == NULL || daftar->data == NULL)
        return 0;

    if (daftar->kapasitas < 2 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return 0;
    }

    if (daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return 0;
    }

    if (!validasiInputTugas(namaBaru, pentingBaru))
        return 0;

    if (!validasiTanggalKalender(&deadlineBaru))
        return 0;

    int index = cariIndexByID(daftar, id);

    if (index == -1)
        return 0;

    char *salinan = malloc(strlen(namaBaru) + 1);
    if (salinan == NULL)
        return 0;

    strcpy(salinan, namaBaru);

    free(daftar->data[index].nama);

    daftar->data[index].deadline = deadlineBaru;
    daftar->data[index].penting = pentingBaru;
    daftar->data[index].nama = salinan;

    insertionSortDeadline(daftar);

    return 1;
}

int hapusTugas(struct DaftarTugas *daftar, int id)
{
    if (daftar == NULL || daftar->data == NULL)
        return 0;

    if (daftar->kapasitas < 2 ||
        daftar->jumlah <= 0 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return 0;
    }

    int index = cariIndexByID(daftar, id);

    if (index == -1)
        return 0;

    free(daftar->data[index].nama);

    for (int i = index; i < daftar->jumlah - 1; i++)
    {
        daftar->data[i] = daftar->data[i + 1];
    }

    daftar->jumlah--;

    if (daftar->jumlah <= daftar->kapasitas / 4 && daftar->kapasitas > 2)
    {
        int newKapasitas = daftar->kapasitas / 2;
        newKapasitas = (newKapasitas < 2) ? 2 : newKapasitas;

        if (!ubahKapasitas(daftar, newKapasitas))
        {
            printf("Warning: Gagal mengecilkan kapasitas array.\n");
        }
    }

    return 1;
}
// Perubahan status
int ubahStatusTugas(struct DaftarTugas *daftar, int id, enum StatusTugas statusBaru)
{
    if (daftar == NULL || daftar->data == NULL)
        return 0;

    if (daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return 0;
    }

    if (statusBaru != TUGAS_SELESAI &&
        statusBaru != TUGAS_BELUM_SELESAI)
    {
        return 0;
    }

    int index = cariIndexByID(daftar, id);

    if (index == -1)
        return 0;

    daftar->data[index].status = statusBaru;

    return 1;
}
// Statistik dan tampilan tugas
// Fungsi ini ditambahkan oleh Codex.
struct StatistikTugas hitungStatistikTugas(struct DaftarTugas *daftar)
{
    struct StatistikTugas statistik = {0, 0, 0, 0};
    if (daftar == NULL || daftar->data == NULL || daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas)
    {
        return statistik;
    }

    statistik.total = daftar->jumlah;
    for (int i = 0; i < daftar->jumlah; i++)
    {
        struct Tugas *tugas = &daftar->data[i];
        if (tugas->status == TUGAS_SELESAI)
            statistik.selesai++;
        else if (tugas->status == TUGAS_BELUM_SELESAI)
        {
            statistik.belumSelesai++;
            if (cekStatusDeadline(tugas) == -1)
                statistik.overdue++;
        }
    }

    return statistik;
}

// Fungsi ini ditambahkan oleh Codex.
static void tampilkanTugasDenganFilter(struct DaftarTugas *daftar, int filterStatus, int hanyaOverdue)
{
    if (daftar == NULL || daftar->data == NULL || daftar->jumlah < 0 ||
        daftar->jumlah > daftar->kapasitas)
    {
        printf("Daftar tugas tidak valid.\n");
        return;
    }

    int ditemukan = 0;
    for (int i = 0; i < daftar->jumlah; i++)
    {
        struct Tugas *tugas = &daftar->data[i];
        if (tugas->nama == NULL ||
            (filterStatus >= 0 && (int)tugas->status != filterStatus) ||
            (hanyaOverdue && (tugas->status != TUGAS_BELUM_SELESAI ||
                              cekStatusDeadline(tugas) != -1)))
        {
            continue;
        }

        ditemukan = 1;
        printf("ID: %d | Nama: %s | Deadline: %02d-%02d-%04d | %s\n",
               tugas->id, tugas->nama, tugas->deadline.tanggal,
               tugas->deadline.bulan, tugas->deadline.tahun,
               tugas->status == TUGAS_SELESAI ? "Selesai" : "Belum selesai");
    }

    if (!ditemukan)
        printf("Tidak ada tugas.\n");
}

// Fungsi ini ditambahkan oleh Codex.
void tampilkanSemuaTugas(struct DaftarTugas *daftar)
{
    tampilkanTugasDenganFilter(daftar, -1, 0);
}

// Fungsi ini ditambahkan oleh Codex.
void tampilkanTugasBelumSelesai(struct DaftarTugas *daftar)
{
    tampilkanTugasDenganFilter(daftar, TUGAS_BELUM_SELESAI, 0);
}

// Fungsi ini ditambahkan oleh Codex.
void tampilkanTugasSelesai(struct DaftarTugas *daftar)
{
    tampilkanTugasDenganFilter(daftar, TUGAS_SELESAI, 0);
}

// Fungsi ini ditambahkan oleh Codex.
void tampilkanTugasOverdue(struct DaftarTugas *daftar)
{
    tampilkanTugasDenganFilter(daftar, -1, 1);
}

// Tampilan matrix
void tampilkanMatrix(struct DaftarTugas *daftar)
{
    if (daftar == NULL || daftar->data == NULL ||
        daftar->jumlah < 0 || daftar->jumlah > daftar->kapasitas)
    {
        printf("Daftar tugas tidak valid.\n");
        return;
    }

    const char *judulKuadran[] =
        {
            "I. PENTING & MENDESAK",
            "II. PENTING & TIDAK MENDESAK",
            "III. TIDAK PENTING & MENDESAK",
            "IV. TIDAK PENTING & TIDAK MENDESAK"};

    printf("\n========== EISENHOWER MATRIX ==========\n");

    for (int q = PENTING_MENDESAK;
         q <= TIDAK_PENTING_TIDAK_MENDESAK;
         q++)
    {
        printf("\n[ %s ]\n", judulKuadran[q]);

        int ditemukan = 0;

        for (int i = 0; i < daftar->jumlah; i++)
        {
            if (daftar->data[i].nama == NULL ||
                daftar->data[i].status != TUGAS_BELUM_SELESAI)
                continue;

            if (tentukanKuadran(&daftar->data[i]) != (enum Kuadran)q)
                continue;

            ditemukan = 1;

            printf("ID       : %d\n", daftar->data[i].id);
            printf("Nama     : %s\n", daftar->data[i].nama);
            printf("Deadline : %02d-%02d-%04d\n",
                   daftar->data[i].deadline.tanggal,
                   daftar->data[i].deadline.bulan,
                   daftar->data[i].deadline.tahun);
            printf("-----------------------------\n");
        }

        if (!ditemukan)
            printf("Tidak ada tugas.\n");
    }

    printf("\n[ TUGAS DENGAN DEADLINE TIDAK VALID ]\n");

    int ditemukan = 0;

    for (int i = 0; i < daftar->jumlah; i++)
    {
        if (daftar->data[i].nama == NULL ||
            daftar->data[i].status != TUGAS_BELUM_SELESAI)
            continue;

        if (tentukanKuadran(&daftar->data[i]) != KUADRAN_TIDAK_VALID)
            continue;

        ditemukan = 1;
        printf("ID   : %d\n", daftar->data[i].id);
        printf("Nama : %s\n", daftar->data[i].nama);
    }

    if (!ditemukan)
        printf("Tidak ada tugas.\n");

    printf("\n=======================================\n");
}
// Penyimpanan data
int simpanTugas(struct DaftarTugas *daftar, const char *namaFile)
{
    if (daftar == NULL || namaFile == NULL || namaFile[0] == '\0')
        return 0;

    if (daftar->jumlah < 0 ||
        daftar->kapasitas < 0 ||
        daftar->jumlah > daftar->kapasitas ||
        (daftar->jumlah > 0 && daftar->data == NULL))
    {
        return 0;
    }

    for (int i = 0; i < daftar->jumlah; i++)
    {
        if (daftar->data[i].nama == NULL ||
            !validasiInputTugas(daftar->data[i].nama, daftar->data[i].penting) ||
            !validasiTanggalKalender(&daftar->data[i].deadline) ||
            daftar->data[i].id <= 0 ||
            (daftar->data[i].status != TUGAS_SELESAI &&
             daftar->data[i].status != TUGAS_BELUM_SELESAI))
        {
            return 0;
        }

        for (int j = i + 1; j < daftar->jumlah; j++)
        {
            if (daftar->data[i].id == daftar->data[j].id)
                return 0;
        }
    }

    FILE *file = fopen(namaFile, "w");

    if (file == NULL)
        return 0;

    int berhasil = 1;

    for (int i = 0; i < daftar->jumlah; i++)
    {
        if (fprintf(
                file,
                "%d|%d|%d|%d|%d|%d|%s\n",
                daftar->data[i].id,
                daftar->data[i].penting,
                daftar->data[i].deadline.tanggal,
                daftar->data[i].deadline.bulan,
                daftar->data[i].deadline.tahun,
                daftar->data[i].status,
                daftar->data[i].nama) < 0)
        {
            berhasil = 0;
            break;
        }
    }

    if (fclose(file) != 0)
        berhasil = 0;

    return berhasil;
}

int muatTugas(struct DaftarTugas *daftar, const char *namaFile)
{
    if (daftar == NULL || namaFile == NULL || namaFile[0] == '\0')
        return 0;

    if (daftar->data == NULL ||
        daftar->jumlah != 0 ||
        daftar->kapasitas < 2 ||
        daftar->jumlah > daftar->kapasitas ||
        daftar->idBerikut <= 0)
    {
        return 0;
    }

    FILE *file = fopen(namaFile, "r");

    if (file == NULL)
    {
        return 0;
    }

    char baris[512];
    char nama[200];

    int id, penting, tanggal, bulan, tahun, status;
    int offset = 0;
    int berhasil = 1;

    while (fgets(baris, sizeof(baris), file) != NULL)
    {

        // Periksa apakah satu baris terlalu panjang
        if (strchr(baris, '\n') == NULL && !feof(file))
        {
            berhasil = 0;
            break;
        }

        // Hapus newline dan carriage return
        baris[strcspn(baris, "\r\n")] = '\0';

        // Parsing data
        int jumlahData = sscanf(
            baris,
            "%d|%d|%d|%d|%d|%d|%199[^\r\n]%n",
            &id,
            &penting,
            &tanggal,
            &bulan,
            &tahun,
            &status,
            nama,
            &offset);

        // Pastikan seluruh kolom berhasil dibaca
        if (jumlahData != 7 || baris[offset] != '\0')
        {
            berhasil = 0;
            break;
        }

        // Validasi nilai
        struct Tanggal deadline = {tanggal, bulan, tahun};

        if (
            id <= 0 ||
            id == INT_MAX ||
            (penting != 0 && penting != 1) ||
            !validasiTanggalKalender(&deadline) ||
            (status != 0 && status != 1) ||
            !validasiInputTugas(nama, penting))
        {
            berhasil = 0;
            break;
        }

        // Periksa ID duplikat
        if (cariIndexByID(daftar, id) != -1)
        {
            berhasil = 0;
            break;
        }

        // Perbesar kapasitas jika penuh
        if (daftar->jumlah == daftar->kapasitas)
        {
            if (daftar->kapasitas > INT_MAX / 2)
            {
                berhasil = 0;
                break;
            }

            int kapasitasBaru = daftar->kapasitas * 2;

            if (kapasitasBaru < 2)
            {
                kapasitasBaru = 2;
            }

            if (!ubahKapasitas(daftar, kapasitasBaru))
            {
                berhasil = 0;
                break;
            }
        }

        // Alokasikan memori nama
        char *salinan = malloc(strlen(nama) + 1);

        if (salinan == NULL)
        {
            berhasil = 0;
            break;
        }

        strcpy(salinan, nama);

        // Masukkan ke dynamic array
        int index = daftar->jumlah;

        daftar->data[index].id = id;
        daftar->data[index].nama = salinan;
        daftar->data[index].penting = penting;

        daftar->data[index].deadline = deadline;

        daftar->data[index].status =
            (enum StatusTugas)status;

        daftar->jumlah++;

        // Perbarui ID berikutnya
        if (id >= daftar->idBerikut)
        {
            daftar->idBerikut = id + 1;
        }
    }

    // Periksa error pembacaan file
    if (ferror(file))
    {
        berhasil = 0;
    }

    if (fclose(file) != 0)
    {
        berhasil = 0;
    }

    // Jika terjadi kegagalan, bersihkan data yang sempat dimuat
    if (!berhasil)
    {

        for (int i = 0; i < daftar->jumlah; i++)
        {
            free(daftar->data[i].nama);
            daftar->data[i].nama = NULL;
        }

        daftar->jumlah = 0;
        daftar->idBerikut = 1;

        return 0;
    }

    // Urutkan berdasarkan deadline
    insertionSortDeadline(daftar);

    return 1;
}

#ifndef EISENHOWER_GUI
// Helper input CLI, ditambahkan oleh Codex.
static int cliBacaBaris(const char *prompt, char *buffer, size_t ukuran)
{
    printf("%s", prompt);
    if (fgets(buffer, (int)ukuran, stdin) == NULL)
        return 0;

    size_t panjang = strlen(buffer);
    if (panjang > 0 && buffer[panjang - 1] != '\n' && !feof(stdin))
    {
        int karakter;
        while ((karakter = getchar()) != '\n' && karakter != EOF)
        {
        }
        printf("Input terlalu panjang. Coba lagi.\n");
        return -1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static int cliBacaInt(const char *prompt, int minimum, int maksimum, int *hasil)
{
    char baris[128];
    while (1)
    {
        int status = cliBacaBaris(prompt, baris, sizeof(baris));
        if (status == 0)
            return 0;
        if (status < 0)
            continue;

        char *akhir;
        long nilai = strtol(baris, &akhir, 10);
        while (isspace((unsigned char)*akhir))
            akhir++;

        if (akhir != baris && *akhir == '\0' &&
            nilai >= minimum && nilai <= maksimum)
        {
            *hasil = (int)nilai;
            return 1;
        }

        printf("Masukkan angka antara %d dan %d.\n", minimum, maksimum);
    }
}

static int cliBacaTanggal(const char *prompt, int izinkanTanggalLampau,
                          struct Tanggal *hasil)
{
    char baris[128];
    while (1)
    {
        int status = cliBacaBaris(prompt, baris, sizeof(baris));
        if (status == 0)
            return 0;
        if (status < 0)
            continue;

        int tanggal, bulan, tahun, offset = 0;
        char *akhir;
        if (sscanf(baris, "%d-%d-%d%n", &tanggal, &bulan, &tahun, &offset) == 3)
        {
            akhir = baris + offset;
            while (isspace((unsigned char)*akhir))
                akhir++;
            struct Tanggal nilai = {tanggal, bulan, tahun};
            if (*akhir == '\0' && validasiTanggalKalender(&nilai) &&
                (izinkanTanggalLampau || validasiTanggal(&nilai)))
            {
                *hasil = nilai;
                return 1;
            }
        }

        printf(izinkanTanggalLampau
                   ? "Tanggal tidak valid. Gunakan format HH-BB-YYYY.\n"
                   : "Tanggal tidak valid atau sudah lewat. Gunakan HH-BB-YYYY.\n");
    }
}

static void cliTampilkanDetail(const struct Tugas *tugas)
{
    if (tugas == NULL)
    {
        printf("Tugas tidak ditemukan.\n");
        return;
    }
    printf("ID       : %d\n", tugas->id);
    printf("Nama     : %s\n", tugas->nama);
    printf("Prioritas: %s\n", tugas->penting ? "Penting" : "Tidak penting");
    printf("Deadline : %02d-%02d-%04d\n", tugas->deadline.tanggal,
           tugas->deadline.bulan, tugas->deadline.tahun);
    printf("Status   : %s\n", tugas->status == TUGAS_SELESAI ? "Selesai" : "Belum selesai");
}

static int cliSimpan(struct DaftarTugas *daftar, const char *fileData)
{
    if (!simpanTugas(daftar, fileData))
    {
        printf("Gagal menyimpan ke %s.\n", fileData);
        return 0;
    }
    printf("Data tersimpan.\n");
    return 1;
}

// main interaktif menggantikan skenario dummy; disusun oleh Codex.
int main(void)
{
    const char *fileData = "data/tugas.txt";
    struct DaftarTugas daftar;
    initDaftarTugas(&daftar);
    if (daftar.data == NULL)
    {
        printf("Memori tidak cukup untuk memulai.\n");
        return 1;
    }

    FILE *file = fopen(fileData, "r");
    if (file != NULL)
    {
        fclose(file);
        if (!muatTugas(&daftar, fileData))
        {
            printf("File %s tidak valid atau gagal dibaca; data tidak akan ditimpa.\n", fileData);
            freeDaftarTugas(&daftar);
            return 1;
        }
        printf("Memuat %d tugas dari %s.\n", daftar.jumlah, fileData);
    }
    else
    {
        printf("File data belum ada; mulai dengan daftar kosong.\n");
    }

    int selesai = 0;
    while (!selesai)
    {
        printf("\n=== PENGELOLA TUGAS ===\n"
               "1. Tambah tugas\n"
               "2. Tampilkan semua tugas\n"
               "3. Tampilkan tugas belum selesai\n"
               "4. Tampilkan tugas selesai\n"
               "5. Tampilkan tugas overdue\n"
               "6. Cari tugas berdasarkan nama\n"
               "7. Lihat detail tugas\n"
               "8. Edit tugas\n"
               "9. Ubah status tugas\n"
               "10. Hapus tugas\n"
               "11. Tampilkan statistik\n"
               "12. Tampilkan Eisenhower Matrix\n"
               "13. Simpan sekarang\n"
               "0. Simpan dan keluar\n");

        int pilihan;
        if (!cliBacaInt("Pilih menu: ", 0, 13, &pilihan))
        {
            printf("\nInput berakhir.\n");
            break;
        }

        if (pilihan == 0)
        {
            if (cliSimpan(&daftar, fileData))
                selesai = 1;
            continue;
        }

        if (pilihan == 1)
        {
            char nama[200];
            int penting;
            struct Tanggal deadline;
            int status = cliBacaBaris("Nama tugas: ", nama, sizeof(nama));
            if (status == 0)
                break;
            if (status < 0 || !validasiInputTugas(nama, 0))
            {
                printf("Nama tugas tidak valid (maksimal 199 karakter; karakter | tidak boleh dipakai).\n");
                continue;
            }
            if (!cliBacaInt("Penting? (1=ya, 0=tidak): ", 0, 1, &penting) ||
                !cliBacaTanggal("Deadline (HH-BB-YYYY): ", 0, &deadline))
                break;
            if (tambahTugas(&daftar, nama, penting, deadline))
            {
                printf("Tugas ditambahkan dengan ID %d.\n", daftar.idBerikut - 1);
                cliSimpan(&daftar, fileData);
            }
            else
                printf("Tugas gagal ditambahkan.\n");
        }
        else if (pilihan >= 2 && pilihan <= 5)
        {
            if (pilihan == 2)
                tampilkanSemuaTugas(&daftar);
            else if (pilihan == 3)
                tampilkanTugasBelumSelesai(&daftar);
            else if (pilihan == 4)
                tampilkanTugasSelesai(&daftar);
            else
                tampilkanTugasOverdue(&daftar);
        }
        else if (pilihan == 6)
        {
            char kueri[200];
            int status = cliBacaBaris("Kata pencarian: ", kueri, sizeof(kueri));
            if (status == 0)
                break;
            if (status < 0 || kueri[0] == '\0')
            {
                printf("Kata pencarian tidak boleh kosong.\n");
                continue;
            }
            int *hasil = daftar.jumlah > 0
                             ? malloc((size_t)daftar.jumlah * sizeof(int))
                             : NULL;
            if (daftar.jumlah > 0 && hasil == NULL)
            {
                printf("Memori tidak cukup untuk pencarian.\n");
                continue;
            }
            int jumlah = cariSemuaIndexByNama(&daftar, kueri, hasil, daftar.jumlah);
            if (jumlah <= 0)
                printf("Tidak ada tugas yang cocok.\n");
            else
                for (int i = 0; i < jumlah; i++)
                    cliTampilkanDetail(&daftar.data[hasil[i]]);
            free(hasil);
        }
        else if (pilihan == 7 || pilihan == 8 || pilihan == 9 || pilihan == 10)
        {
            int id;
            if (!cliBacaInt("ID tugas: ", 1, INT_MAX, &id))
                break;
            struct Tugas *tugas = ambilDetailTugas(&daftar, id);
            if (pilihan == 7)
            {
                cliTampilkanDetail(tugas);
                continue;
            }
            if (tugas == NULL)
            {
                printf("Tugas dengan ID %d tidak ditemukan.\n", id);
                continue;
            }
            if (pilihan == 8)
            {
                char nama[200];
                int penting;
                struct Tanggal deadline;
                cliTampilkanDetail(tugas);
                int status = cliBacaBaris("Nama baru: ", nama, sizeof(nama));
                if (status == 0)
                    break;
                if (status < 0 || !validasiInputTugas(nama, 0))
                {
                    printf("Nama tugas tidak valid.\n");
                    continue;
                }
                if (!cliBacaInt("Penting? (1=ya, 0=tidak): ", 0, 1, &penting) ||
                    !cliBacaTanggal("Deadline baru (HH-BB-YYYY): ", 1, &deadline))
                    break;
                if (editTugas(&daftar, id, nama, penting, deadline))
                {
                    printf("Tugas berhasil diperbarui.\n");
                    cliSimpan(&daftar, fileData);
                }
                else
                    printf("Tugas gagal diperbarui.\n");
            }
            else if (pilihan == 9)
            {
                enum StatusTugas statusBaru = tugas->status == TUGAS_SELESAI
                                                  ? TUGAS_BELUM_SELESAI
                                                  : TUGAS_SELESAI;
                if (ubahStatusTugas(&daftar, id, statusBaru))
                {
                    printf("Status tugas diperbarui.\n");
                    cliSimpan(&daftar, fileData);
                }
            }
            else
            {
                int konfirmasi;
                if (!cliBacaInt("Hapus tugas ini? (1=ya, 0=batal): ", 0, 1,
                                &konfirmasi))
                    break;
                if (konfirmasi && hapusTugas(&daftar, id))
                {
                    printf("Tugas dihapus.\n");
                    cliSimpan(&daftar, fileData);
                }
            }
        }
        else if (pilihan == 11)
        {
            struct StatistikTugas statistik = hitungStatistikTugas(&daftar);
            printf("Total: %d | Selesai: %d | Belum selesai: %d | Overdue: %d\n",
                   statistik.total, statistik.selesai,
                   statistik.belumSelesai, statistik.overdue);
        }
        else if (pilihan == 12)
            tampilkanMatrix(&daftar);
        else if (pilihan == 13)
            cliSimpan(&daftar, fileData);
    }

    freeDaftarTugas(&daftar);
    return 0;
}
#endif
