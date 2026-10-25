#include "app/i18n.h"
#include "app/i18n_tables.h"
namespace i18n {
const std::unordered_map<std::string, const char *> &extra_table(int language) {
 /* One table per i18n::Lang, in its order: tables[(int)lang] is that language's. */
 static const std::unordered_map<std::string, const char *> tables[] = {
    {}, /* English: i18n.cpp */
    {{"Hjem", "ホーム"}, {"Filmer", "映画"}, {"Serier", "テレビ番組"}, {"Musikk", "音楽"}, {"Søk", "検索"}, {"Innstillinger", "設定"}, {"Spill av", "再生"}, {"Fortsett", "続ける"}, {"Tilbake", "戻る"}, {"Språk", "言語"}, {"Undertekster", "字幕"}, {"Lyd", "音声"}, {"Laster …", "読み込み中…"}},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {{"Hjem", "Start"}, {"Filmer", "Films"}, {"Serier", "Series"}, {"Musikk", "Muziek"}, {"Søk", "Zoeken"}, {"Innstillinger", "Instellingen"}, {"Spill av", "Afspelen"}, {"Fortsett", "Doorgaan"}, {"Tilbake", "Terug"}, {"Språk", "Taal"}, {"Undertekster", "Ondertitels"}, {"Lyd", "Audio"}, {"Laster …", "Laden…"}},
    {{"Hjem", "Главная"}, {"Filmer", "Фильмы"}, {"Serier", "Сериалы"}, {"Musikk", "Музыка"}, {"Søk", "Поиск"}, {"Innstillinger", "Настройки"}, {"Spill av", "Воспроизвести"}, {"Fortsett", "Продолжить"}, {"Tilbake", "Назад"}, {"Språk", "Язык"}, {"Undertekster", "Субтитры"}, {"Lyd", "Аудио"}, {"Laster …", "Загрузка…"}},
    {{"Hjem", "홈"}, {"Filmer", "영화"}, {"Serier", "TV 프로그램"}, {"Musikk", "음악"}, {"Søk", "검색"}, {"Innstillinger", "설정"}, {"Spill av", "재생"}, {"Fortsett", "계속"}, {"Tilbake", "뒤로"}, {"Språk", "언어"}, {"Undertekster", "자막"}, {"Lyd", "오디오"}, {"Laster …", "로딩 중…"}},
    {}, /* Chinese (Simplified): i18n_zh.cpp */
    {}, /* Chinese (Traditional): i18n_zh.cpp */
    {{"Hjem", "Strona główna"}, {"Filmer", "Filmy"}, {"Serier", "Seriale"}, {"Musikk", "Muzyka"}, {"Søk", "Szukaj"}, {"Innstillinger", "Ustawienia"}, {"Spill av", "Odtwórz"}, {"Fortsett", "Kontynuuj"}, {"Tilbake", "Wstecz"}, {"Språk", "Język"}, {"Undertekster", "Napisy"}, {"Lyd", "Dźwięk"}, {"Laster …", "Ładowanie…"}},
    {{"Hjem", "Hjem"}, {"Filmer", "Film"}, {"Serier", "Serier"}, {"Musikk", "Musik"}, {"Søk", "Søg"}, {"Innstillinger", "Indstillinger"}, {"Spill av", "Afspil"}, {"Fortsett", "Fortsæt"}, {"Tilbake", "Tilbage"}, {"Språk", "Sprog"}, {"Undertekster", "Undertekster"}, {"Lyd", "Lyd"}, {"Laster …", "Indlæser…"}},
    {{"Hjem", "Hem"}, {"Filmer", "Filmer"}, {"Serier", "Serier"}, {"Musikk", "Musik"}, {"Søk", "Sök"}, {"Innstillinger", "Inställningar"}, {"Spill av", "Spela upp"}, {"Fortsett", "Fortsätt"}, {"Tilbake", "Tillbaka"}, {"Språk", "Språk"}, {"Undertekster", "Undertexter"}, {"Lyd", "Ljud"}, {"Laster …", "Läser in…"}},
    {},
    {{"Hjem", "Koti"}, {"Filmer", "Elokuvat"}, {"Serier", "Sarjat"}, {"Musikk", "Musiikki"}, {"Søk", "Haku"}, {"Innstillinger", "Asetukset"}, {"Spill av", "Toista"}, {"Fortsett", "Jatka"}, {"Tilbake", "Takaisin"}, {"Språk", "Kieli"}, {"Undertekster", "Tekstitykset"}, {"Lyd", "Ääni"}, {"Laster …", "Ladataan…"}},
    {{"Hjem", "Ana Sayfa"}, {"Filmer", "Filmler"}, {"Serier", "Diziler"}, {"Musikk", "Müzik"}, {"Søk", "Ara"}, {"Innstillinger", "Ayarlar"}, {"Spill av", "Oynat"}, {"Fortsett", "Devam et"}, {"Tilbake", "Geri"}, {"Språk", "Dil"}, {"Undertekster", "Altyazılar"}, {"Lyd", "Ses"}, {"Laster …", "Yükleniyor…"}},
    {{"Hjem", "الرئيسية"}, {"Filmer", "الأفلام"}, {"Serier", "المسلسلات"}, {"Musikk", "الموسيقى"}, {"Søk", "بحث"}, {"Innstillinger", "الإعدادات"}, {"Spill av", "تشغيل"}, {"Fortsett", "متابعة"}, {"Tilbake", "رجوع"}, {"Språk", "اللغة"}, {"Undertekster", "الترجمة"}, {"Lyd", "الصوت"}, {"Laster …", "جار التحميل…"}},
    {{"Hjem", "Domů"}, {"Filmer", "Filmy"}, {"Serier", "Seriály"}, {"Musikk", "Hudba"}, {"Søk", "Hledat"}, {"Innstillinger", "Nastavení"}, {"Spill av", "Přehrát"}, {"Fortsett", "Pokračovat"}, {"Tilbake", "Zpět"}, {"Språk", "Jazyk"}, {"Undertekster", "Titulky"}, {"Lyd", "Zvuk"}, {"Laster …", "Načítání…"}},
    {{"Hjem", "Αρχική"}, {"Filmer", "Ταινίες"}, {"Serier", "Σειρές"}, {"Musikk", "Μουσική"}, {"Søk", "Αναζήτηση"}, {"Innstillinger", "Ρυθμίσεις"}, {"Spill av", "Αναπαραγωγή"}, {"Fortsett", "Συνέχεια"}, {"Tilbake", "Πίσω"}, {"Språk", "Γλώσσα"}, {"Undertekster", "Υπότιτλοι"}, {"Lyd", "Ήχος"}, {"Laster …", "Φόρτωση…"}},
    {{"Hjem", "Kezdőlap"}, {"Filmer", "Filmek"}, {"Serier", "Sorozatok"}, {"Musikk", "Zene"}, {"Søk", "Keresés"}, {"Innstillinger", "Beállítások"}, {"Spill av", "Lejátszás"}, {"Fortsett", "Folytatás"}, {"Tilbake", "Vissza"}, {"Språk", "Nyelv"}, {"Undertekster", "Feliratok"}, {"Lyd", "Hang"}, {"Laster …", "Betöltés…"}},
    {{"Hjem", "Acasă"}, {"Filmer", "Filme"}, {"Serier", "Seriale"}, {"Musikk", "Muzică"}, {"Søk", "Căutare"}, {"Innstillinger", "Setări"}, {"Spill av", "Redare"}, {"Fortsett", "Continuă"}, {"Tilbake", "Înapoi"}, {"Språk", "Limbă"}, {"Undertekster", "Subtitrări"}, {"Lyd", "Audio"}, {"Laster …", "Se încarcă…"}},
    {{"Hjem", "หน้าหลัก"}, {"Filmer", "ภาพยนตร์"}, {"Serier", "ซีรีส์"}, {"Musikk", "เพลง"}, {"Søk", "ค้นหา"}, {"Innstillinger", "การตั้งค่า"}, {"Spill av", "เล่น"}, {"Fortsett", "ดำเนินการต่อ"}, {"Tilbake", "กลับ"}, {"Språk", "ภาษา"}, {"Undertekster", "คำบรรยาย"}, {"Lyd", "เสียง"}, {"Laster …", "กำลังโหลด…"}},
    {{"Hjem", "Beranda"}, {"Filmer", "Film"}, {"Serier", "Serial"}, {"Musikk", "Musik"}, {"Søk", "Cari"}, {"Innstillinger", "Pengaturan"}, {"Spill av", "Putar"}, {"Fortsett", "Lanjutkan"}, {"Tilbake", "Kembali"}, {"Språk", "Bahasa"}, {"Undertekster", "Subtitle"}, {"Lyd", "Audio"}, {"Laster …", "Memuat…"}},
    {{"Hjem", "Trang chủ"}, {"Filmer", "Phim"}, {"Serier", "Phim bộ"}, {"Musikk", "Âm nhạc"}, {"Søk", "Tìm kiếm"}, {"Innstillinger", "Cài đặt"}, {"Spill av", "Phát"}, {"Fortsett", "Tiếp tục"}, {"Tilbake", "Quay lại"}, {"Språk", "Ngôn ngữ"}, {"Undertekster", "Phụ đề"}, {"Lyd", "Âm thanh"}, {"Laster …", "Đang tải…"}},
    {{"Hjem", "Головна"}, {"Filmer", "Фільми"}, {"Serier", "Серіали"}, {"Musikk", "Музика"}, {"Søk", "Пошук"}, {"Innstillinger", "Налаштування"}, {"Spill av", "Відтворити"}, {"Fortsett", "Продовжити"}, {"Tilbake", "Назад"}, {"Språk", "Мова"}, {"Undertekster", "Субтитри"}, {"Lyd", "Аудіо"}, {"Laster …", "Завантаження…"}},
    {{"Hjem", "Početna"}, {"Filmer", "Filmovi"}, {"Serier", "Serije"}, {"Musikk", "Glazba"}, {"Søk", "Pretraži"}, {"Innstillinger", "Postavke"}, {"Spill av", "Reproduciraj"}, {"Fortsett", "Nastavi"}, {"Tilbake", "Natrag"}, {"Språk", "Jezik"}, {"Undertekster", "Titlovi"}, {"Lyd", "Zvuk"}, {"Laster …", "Učitavanje…"}},
    {{"Hjem", "Почетна"}, {"Filmer", "Филмови"}, {"Serier", "Серије"}, {"Musikk", "Музика"}, {"Søk", "Претрага"}, {"Innstillinger", "Подешавања"}, {"Spill av", "Пусти"}, {"Fortsett", "Настави"}, {"Tilbake", "Назад"}, {"Språk", "Језик"}, {"Undertekster", "Титлови"}, {"Lyd", "Звук"}, {"Laster …", "Учитавање…"}},
 };
 static_assert(sizeof(tables) / sizeof(tables[0]) == kLangCount, "one table per i18n::Lang");
 static const std::unordered_map<std::string, const char *> empty;
 return language >= 0 && language < (int)(sizeof(tables)/sizeof(tables[0])) ? tables[language] : empty;
}
}
