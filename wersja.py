# Skrypt PlatformIO (extra_scripts): przed każdą kompilacją zapisuje do src/wersja.h
# numer wersji programu z tagów Git w nowym stylu (v2.0, v2.1, ...) i datę ostatniego commitu.
# Pokazuje je strona (panel "Stan zegara", stopka, "Aktualizacja programu").
#
# Numeracja: każde wgranie na zegar dostaje kolejny tag, np. v2.1 albo v2.1.1:
#   druga liczba (v2.1 -> v2.2) = nowa funkcja, trzecia (v2.1 -> v2.1.1) = drobna poprawka.
#   "2.1", "2.1.1" = dokładnie wersja z tym tagiem
#   "2.1+3"  = 3 commity po v2.1, bez nowego tagu (wersja próbna)
#   "...*"   = do tego niezapisane zmiany (kompilacja z plików nie zapisanych w Git)
# Tagi bez kropki nie są brane pod uwagę.
# Identyfikator commitu (np. 86f175a) idzie osobno do WERSJA_GIT, strona pokazuje go drobnym drukiem.
#
# Bez Gita (np. projekt pobrany z GitHub jako ZIP) albo bez tagu numer jest brany z pliku wersja.txt.
# Przy każdej nowej wersji wpisać tam ten sam numer co w tagu (np. 2.2.2), przed commitem.
# Gdy Git jest, a numer w wersja.txt różni się od tagu, kompilacja wypisuje ostrzeżenie.
#
# Wersja z ESP-IDF (esp_app_desc) się nie nadaje: PlatformIO liczy ją raz i potem jest nieaktualna.
# Plik zapisywany tylko przy zmianie, żeby nie kompilować web.cpp za każdym razem. src/wersja.h jest w .gitignore.
import os
import re
import subprocess

Import("env")  # noqa: F821 (dostarcza PlatformIO)

katalog = env.subst("$PROJECT_DIR")  # noqa: F821


def git(*args):
    try:
        return subprocess.check_output(["git"] + list(args), cwd=katalog, text=True,
                                       stderr=subprocess.DEVNULL).strip()
    except Exception:
        return ""


# Numer z pliku wersja.txt (pierwsza linia, np. "2.2.1"), gdy nie ma Gita albo tagu
plik_numeru = os.path.join(katalog, "wersja.txt")
numer_z_pliku = ""
if os.path.exists(plik_numeru):
    numer_z_pliku = open(plik_numeru, encoding="utf-8").readline().strip()
    if not re.match(r"^\d+\.\d+(?:\.\d+)?$", numer_z_pliku):
        numer_z_pliku = ""

opis = git("describe", "--tags", "--long", "--dirty", "--match", "v[0-9]*.[0-9]*")
m = re.match(r"^v(\d+\.\d+(?:\.\d+)?)-(\d+)-g([0-9a-f]+)(-dirty)?$", opis)
if m:
    numer, po, kod, brudna = m.group(1), int(m.group(2)), m.group(3), bool(m.group(4))
    wersja = numer + ("+%d" % po if po else "") + ("*" if brudna else "")
    if numer_z_pliku and numer_z_pliku != numer and po == 0:
        print("UWAGA: tag v%s, a w wersja.txt jest %s: popraw wersja.txt" % (numer, numer_z_pliku))
else:
    # brak tagu w nowym stylu albo brak Gita: numer z wersja.txt
    kod = git("rev-parse", "--short", "HEAD")
    wersja = numer_z_pliku or "nieznana"
# Data ostatniego commitu, a bez Gita data kompilacji
data = git("log", "-1", "--format=%cd", "--date=format:%d.%m.%Y %H:%M") or \
    __import__("datetime").datetime.now().strftime("%d.%m.%Y %H:%M")
tresc = ('// Plik tworzony przez wersja.py przy kompilacji, nie edytować\n'
         '#define WERSJA_PROGRAMU "%s"\n#define WERSJA_GIT "%s"\n#define WERSJA_DATA "%s"\n'
         % (wersja, kod, data))

plik = os.path.join(katalog, "src", "wersja.h")
stara = open(plik, encoding="utf-8").read() if os.path.exists(plik) else ""
if stara != tresc:
    with open(plik, "w", encoding="utf-8") as f:
        f.write(tresc)
print("Wersja programu: %s (git %s, %s)" % (wersja, kod, data))
