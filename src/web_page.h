// Strona ustawień zegara (http://budzik.local w sieci domowej, http://192.168.4.1 w sieci zegara).
// Cała w jednym pliku: HTML, wygląd i skrypt. Dane idą przez /api/... (web.cpp).
// Wygląd: ciemny, pomarańcz obudowy, panele rozwijane (<details>, bez skryptu). Alarm w pasku stanu,
// panel jasności z wykresem i przełącznikiem dziś / 21 czerwca / 21 grudnia.
// Lekka: bez czcionek i plików z internetu, ikony jako jeden zestaw <symbol>, wykres doby z CSS.
#ifndef _INCLUDE_WEB_PAGE_H_
#define _INCLUDE_WEB_PAGE_H_

static const char WEB_PAGE[] = R"rawliteral(<!DOCTYPE html>
<html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Budzik: ustawienia</title>
<!-- Ikona karty: przód obudowy (pomarańczowy, ekran w ramce, kratka głośnika), na siatce 16 px, wpisana w stronę, bez osobnego pliku -->
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect x='3' y='3' width='58' height='58' rx='7' fill='%23ff8c3a'/%3E%3Crect x='12' y='8' width='40' height='24' fill='%23ff6b2b'/%3E%3Crect x='16' y='12' width='32' height='16' fill='%231e1f22'/%3E%3Cg fill='%231e1f22'%3E%3Crect x='20' y='36' width='24' height='4' rx='2'/%3E%3Crect x='20' y='44' width='24' height='4' rx='2'/%3E%3Crect x='20' y='52' width='24' height='4' rx='2'/%3E%3C/g%3E%3C/svg%3E">
<style>
:root{color-scheme:dark}*{box-sizing:border-box}[hidden]{display:none!important}
body{margin:0;background:#121214;color:#ededed;font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main,#reszta{display:flex;flex-direction:column;gap:12px}
main{max-width:560px;margin:auto;padding:16px 12px 32px}
header{display:flex;align-items:center;gap:12px;padding:4px}
h1{margin:0;font-size:1.5em;font-weight:900}h2{margin:0;font-size:1.1em}
p{margin:0}a{color:#ff9d57}
.szary{color:#a8a8ad}.maly{font-size:.85em}.ok{color:#8fdc9a}.blad{color:#ff8a80}
.karta{background:#1c1c1f;border-radius:16px}
.stan{padding:2px 16px}.stare{opacity:.5}
.wiersz{display:flex;align-items:center;gap:12px;padding:12px 0}
.wiersz+.wiersz{border-top:1px solid #2a2a2e}
.wiersz b{text-align:right}
.godz{font-size:1.9em;font-weight:800;line-height:1.1}
.prawo{display:flex;flex-direction:column;align-items:flex-end;gap:4px;text-align:right}
.chip{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;font-size:.85em;font-weight:600;background:#2a2a2e;color:#c4c4c8}
.chip.ok{background:#1f3324;color:#8fdc9a}.chip.pom{background:#3a2414;color:#ffb47f}
.kropka::before,.chip.ok::before,.chip.pom::before{content:"";display:inline-block;width:8px;height:8px;border-radius:50%;background:currentColor;margin-right:6px;vertical-align:1px}
.chip::before{margin-right:0!important}
.baner{display:flex;gap:12px;padding:16px;background:#2a1a1a}.baner b{color:#ffb3ab}.baner span{color:#d8c4c2;font-size:.9em}
svg.i{width:22px;height:22px;flex:none;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
summary{list-style:none;display:flex;flex-wrap:wrap;align-items:center;gap:12px;padding:16px;min-height:56px;cursor:pointer}
summary::-webkit-details-marker{display:none}
summary h2{flex:1}
summary::after{content:"";width:8px;height:8px;border:solid #a8a8ad;border-width:0 2px 2px 0;transform:rotate(45deg);margin:-4px 4px 0}
details[open]>summary::after{transform:rotate(225deg);margin-top:4px}
.opis{color:#a8a8ad;font-size:.9em;text-align:right}.opis.blad{color:#ff8a80}.opis.ok{color:#8fdc9a}
.pasek{flex-basis:100%;order:2}details[open]>summary .pasek{display:none}
.pasek-tlo{position:relative;height:10px;border-radius:5px;margin-top:2px}
#pasek-teraz{position:absolute;top:-5px;bottom:-5px;width:3px;border-radius:2px;background:#ededed;margin-left:-1px}
.tresc{padding:0 16px 18px;display:flex;flex-direction:column;gap:12px}
input,button{font:inherit;color:inherit}
input[type=text],input[type=password]{width:100%;height:48px;border-radius:12px;border:1px solid #333338;background:#141416;padding:0 14px}
input[type=time],select{height:40px;border-radius:10px;border:1px solid #3a3a40;background:#1c1c1f;padding:0 8px;font:inherit;font-weight:600;color:inherit}
button{min-height:48px;border:0;border-radius:12px;background:#2a2a2e;padding:0 16px;cursor:pointer}
button.gl{background:#ff8c3a;color:#1a1a1a;font-weight:700}
button.gl:disabled{background:#2a2a2e;color:#8a8a90;cursor:default}
button.ramka{background:none;border:1px solid #3a3a40}
.rzad{display:flex;gap:10px}.rzad>input{flex:1;min-width:0}.rzad>.gl{flex:1}
.wynik{display:block;width:100%;text-align:left;background:#141416;padding:10px 14px;margin-top:6px}
ol{margin:0;padding-left:1.3em;line-height:1.5}
.zakl{display:flex;gap:4px;padding:4px;background:#141416;border-radius:12px}
.zakl button{flex:1;min-height:40px;border-radius:9px;background:none;font-size:.95em;font-weight:600;padding:0 6px}
.zakl button[aria-pressed=true]{background:#ff8c3a;color:#1a1a1a}
.wyk-rama{display:flex;gap:8px}
.wyk{position:relative;flex:1;height:110px;background:#141416;border-radius:10px;overflow:hidden}
.wyk>*{position:absolute}.pole{inset:0}
.poz{left:0;right:0;border-top:1px dashed #303036}
.sl{top:0;bottom:0;border-left:2px dashed rgba(237,237,237,.4);margin-left:-1px}
#teraz{top:0;bottom:0;width:2px;background:#ededed;margin-left:-1px}
.poziomy{position:relative;width:44px;font-size:.75em;color:#a8a8ad}.poziomy span{position:absolute;transform:translateY(-50%)}
.osi{display:flex;justify-content:space-between;font-size:.75em;color:#a8a8ad}
.wiersz-j{background:#141416;border-radius:12px;padding:12px 14px;display:flex;flex-direction:column;gap:4px}
.wiersz-j h3{margin:0;font-size:1em;display:flex;align-items:center;gap:8px}
.zdanie{color:#d4d4d8;line-height:2.5}.zdanie input,.zdanie select{height:36px;vertical-align:middle}
.dzis{font-size:.8em;color:#a8a8ad}
.stopka{text-align:center;margin-top:8px}
.pokaz{display:flex;align-items:center;gap:8px;cursor:pointer}.pokaz input{width:20px;height:20px;margin:0;accent-color:#ff8c3a}
input.ukryty{position:absolute;width:1px;height:1px;opacity:0;overflow:hidden}
.plik{display:flex;align-items:center;gap:12px;padding:14px;border:1px dashed #3a3a40;border-radius:12px;background:#141416;cursor:pointer}
.plik.wybrany{border:1px solid #ff8c3a}input.ukryty:focus-visible+.plik{outline:2px solid #ff8c3a;outline-offset:2px}
.uwaga{background:#2a1a1a;color:#ffb3ab;border-radius:12px;padding:10px 14px;font-size:.9em}
.postep{height:10px;border-radius:5px;background:#141416;overflow:hidden}.postep>div{height:100%;width:0;background:#ff8c3a}
@media(min-width:700px){body{font-size:17px}main{padding-top:32px}}
</style></head><body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>
<symbol id="i0" viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="12" rx="2"/><path d="M8 21h8M4 3l16 18"/></symbol>
<symbol id="i1" viewBox="0 0 24 24"><path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/></symbol>
<symbol id="i2" viewBox="0 0 24 24"><path d="M12 2v8M5.2 11.2l1.4 1.4M2 18h2M20 18h2M17.4 12.6l1.4-1.4M22 22H2M16 18a4 4 0 0 0-8 0M8 6l4-4 4 4"/></symbol>
<symbol id="i4" viewBox="0 0 24 24"><path d="M12 10V2M5.2 11.2l1.4 1.4M2 18h2M20 18h2M17.4 12.6l1.4-1.4M22 22H2M16 18a4 4 0 0 0-8 0M16 6l-4 4-4-4"/></symbol>
<symbol id="i3" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></symbol>
<symbol id="ib" viewBox="0 0 24 24"><path d="M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9M10.3 21a1.94 1.94 0 0 0 3.4 0"/></symbol>
<symbol id="ibx" viewBox="0 0 24 24"><path d="M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9M10.3 21a1.94 1.94 0 0 0 3.4 0M3 3l18 18"/></symbol>
<symbol id="iw" viewBox="0 0 24 24"><path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1"/></symbol>
<symbol id="iwx" viewBox="0 0 24 24"><path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0M3 3l18 18"/><circle cx="12" cy="19" r="1"/></symbol>
<symbol id="ip" viewBox="0 0 24 24"><path d="M17.5 19H9a7 7 0 1 1 6.7-9h1.8a4.5 4.5 0 1 1 0 9z"/></symbol>
<symbol id="it" viewBox="0 0 24 24"><path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/></symbol>
<symbol id="im" viewBox="0 0 24 24"><path d="M12 22s7-6.2 7-12a7 7 0 0 0-14 0c0 5.8 7 12 7 12z"/><circle cx="12" cy="10" r="2.5"/></symbol>
<symbol id="is" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.01"/></symbol>
<symbol id="id" viewBox="0 0 24 24"><circle cx="12" cy="13" r="8"/><path d="M12 9v4l2.5 2.5M4 3.5 2 6M20 3.5 22 6"/></symbol>
<symbol id="iu" viewBox="0 0 24 24"><path d="M12 15V3M7 8l5-5 5 5M4 15v4a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-4"/></symbol>
</defs></svg>
<main>
<header>
<svg width="40" height="40" viewBox="0 0 64 64" aria-hidden="true"><rect x="3" y="3" width="58" height="58" rx="10" fill="#ff8c3a"/><rect x="12" y="8" width="40" height="24" rx="2" fill="#ff6b2b"/><rect x="16" y="12" width="32" height="16" fill="#1e1f22"/><g fill="#1e1f22"><rect x="20" y="37" width="24" height="4" rx="2"/><rect x="20" y="45" width="24" height="4" rx="2"/><rect x="20" y="53" width="24" height="4" rx="2"/></g></svg>
<div><h1>Budzik</h1><div class="szary">ustawienia</div></div>
</header>

<section class="karta baner" id="baner" hidden><svg class="i blad"><use href="#iwx"/></svg>
<div style="display:flex;flex-direction:column;gap:4px"><b>Brak połączenia z zegarem</b><span>Sprawdź, czy zegar jest włączony, a telefon lub komputer jest w sieci domowej. Strona sama spróbuje ponownie za 15 sekund.</span></div></section>

<section class="karta stan" id="stan">
<div class="wiersz" id="w-alarm" hidden style="padding:14px 0;flex-wrap:wrap">
<svg class="i" id="a-on" style="color:#ff8c3a"><use href="#ib"/></svg><svg class="i szary" id="a-off" hidden><use href="#ibx"/></svg>
<div style="flex:1"><div class="szary maly">Alarm</div><div class="godz" id="a-godz"></div></div>
<div class="prawo"><span class="chip" id="a-chip"></span></div>
<div class="szary maly" style="flex-basis:100%;margin:-6px 0 0 34px">ustawiasz pokrętłem na zegarze</div></div>
<div class="wiersz"><svg class="i szary"><use href="#iw"/></svg><span class="szary" style="flex:1">Wi-Fi</span><b id="s-wifi">wczytuję...</b>
<svg id="s-zasieg" width="18" height="14" viewBox="0 0 18 14" role="img" hidden><rect y="10" width="3" height="4" rx="1"/><rect x="5" y="7" width="3" height="7" rx="1"/><rect x="10" y="4" width="3" height="10" rx="1"/><rect x="15" width="3" height="14" rx="1"/></svg></div>
<div class="wiersz"><svg class="i szary"><use href="#ip"/></svg><span class="szary" style="flex:1">Pogoda<span class="maly" id="s-odcz" hidden><br>temperatura odczuwalna</span></span><b id="s-pogoda"></b></div>
</section>

<section class="karta" id="sekcja-wifi" hidden><div class="tresc" style="padding-top:18px">
<h2>Sieć Wi-Fi</h2>
<p class="szary">Wybierz sieć z listy albo wpisz jej nazwę, a potem hasło.</p>
<div id="sieci"></div>
<label for="ssid" class="szary maly">Nazwa sieci</label><input type="text" id="ssid" maxlength="32">
<label for="haslo" class="szary maly">Hasło</label><input type="password" id="haslo" maxlength="64">
<label class="pokaz szary maly"><input type="checkbox" id="pokaz"> Pokaż hasło</label>
<button class="gl" onclick="zapiszWifi()">Zapisz i uruchom zegar ponownie</button>
<div id="wifi-wynik"></div>
</div></section>

<div id="reszta" hidden>
<details class="karta" id="p-jasnosc">
<summary><svg class="i"><use href="#i3"/></svg><h2>Jasność ekranu</h2><span class="opis" id="o-jasnosc"></span>
<div class="pasek" aria-hidden="true"><div class="pasek-tlo" id="pasek"><i id="pasek-teraz"></i></div><div class="osi" style="margin-top:4px"><span>0:00</span><span>12:00</span><span>24:00</span></div></div></summary>
<div class="tresc">
<div class="zakl" id="zakl"><button data-d="0" aria-pressed="true">Dziś</button><button data-d="1" aria-pressed="false">21 czerwca</button><button data-d="2" aria-pressed="false">21 grudnia</button></div>
<div class="wyk-rama"><div class="wyk" role="img" aria-label="Jasność ekranu od północy do północy">
<i class="poz" style="top:6%"></i><i class="poz" style="top:51%"></i><i class="poz" style="top:87%"></i>
<div class="pole" id="pole"></div><i class="sl" id="sl-w"></i><i class="sl" id="sl-z"></i><i id="teraz"></i></div>
<div class="poziomy" aria-hidden="true"><span style="top:6%">pełna</span><span style="top:51%">średnia</span><span style="top:87%">nocna</span></div></div>
<div class="osi" style="margin:-6px 52px 0 0"><span>0:00</span><span>6:00</span><span>12:00</span><span>18:00</span><span>24:00</span></div>
<p class="maly" id="slonce-txt" style="color:#c4c4c8"></p>
<div class="wiersz-j"><h3><svg class="i"><use href="#i0"/></svg>Noc</h3>
<p class="zdanie">Ekran jest zgaszony od <input type="time" id="j-noc" step="900" aria-label="Noc od"> do wschodu słońca.</p></div>
<div class="wiersz-j"><h3><svg class="i"><use href="#i2"/></svg>Rano</h3>
<p class="zdanie">Od wschodu słońca ekran przez <select id="j-rano" aria-label="Czas rozjaśniania"><option value="60">1 godzinę</option><option value="90">1,5 godziny</option><option value="120">2 godziny</option><option value="150">2,5 godziny</option><option value="180">3 godziny</option></select>
płynnie się rozjaśnia. Pełną jasność osiąga nie wcześniej niż o <input type="time" id="j-pelny" step="900" aria-label="Pełna jasność nie wcześniej niż">.</p>
<span class="dzis" id="d-rano"></span></div>
<div class="wiersz-j"><h3><svg class="i"><use href="#i4"/></svg>Wieczór</h3>
<p class="zdanie">Od <select id="j-wieczor" aria-label="Początek przyciemniania"><option value="-60">godziny przed zachodem</option><option value="-30">30 minut przed zachodem</option><option value="0">zachodu słońca</option><option value="30">30 minut po zachodzie</option><option value="60">godziny po zachodzie</option></select>
ekran przez 30 minut płynnie przyciemnia się do jasności średniej.</p>
<span class="dzis" id="d-wieczor"></span></div>
<div class="wiersz-j"><h3><svg class="i"><use href="#i1"/></svg>Przed snem</h3>
<p class="zdanie">Od <input type="time" id="j-sen" step="900" min="12:00" aria-label="Jasność nocna od"> ekran świeci słabo (jasność nocna).</p></div>
<button class="gl" id="zapisz-j" onclick="zapiszJasnosc()" disabled>Zapisz jasność</button>
<div id="jasnosc-wynik"></div>
<p class="maly szary">Wschód i zachód słońca zegar liczy sam. Przy alarmie i po obróceniu pokrętła ekran na chwilę się rozjaśnia.</p>
</div></details>

<details class="karta" id="p-drzemka">
<summary><svg class="i"><use href="#id"/></svg><h2>Drzemka</h2><span class="opis" id="o-drzemka"></span></summary>
<div class="tresc">
<div class="zakl" id="zakl-d"><button data-m="5" aria-pressed="false">5</button><button data-m="6" aria-pressed="false">6</button><button data-m="7" aria-pressed="false">7</button><button data-m="8" aria-pressed="false">8</button><button data-m="9" aria-pressed="false">9</button></div>
<div id="drzemka-wynik"></div>
<p class="maly szary">Tyle minut trwa drzemka po kliknięciu pokrętła podczas alarmu; zmiana działa od następnej drzemki.</p>
</div></details>

<details class="karta" id="p-temp">
<summary><svg class="i"><use href="#it"/></svg><h2>Temperatura</h2><span class="opis" id="o-temp"></span></summary>
<div class="tresc">
<p class="szary maly">Którą temperaturę pokazuje zegar:</p>
<div class="zakl" id="zakl-t"><button data-t="0" aria-pressed="false">Z pomiaru</button><button data-t="1" aria-pressed="false">Odczuwalna</button></div>
<p class="maly" id="temp-teraz" style="color:#c4c4c8"></p>
<div id="temp-wynik"></div>
<p class="maly szary">„Z pomiaru” to temperatura powietrza, taka jak na termometrze za oknem i w prognozach. „Odczuwalna” uwzględnia wiatr, wilgotność i słońce, więc zimą jest zwykle niższa, a latem w słońcu wyższa. Zegar pobiera pogodę z serwisu <a href="https://open-meteo.com" target="_blank" rel="noopener">Open-Meteo</a> co 30 minut, bez konta i klucza.</p>
</div></details>

<details class="karta" id="p-miasto">
<summary><svg class="i"><use href="#im"/></svg><h2>Miejscowość</h2><span class="opis" id="o-miasto"></span></summary>
<div class="tresc">
<label for="szukaj" class="szary maly">Szukaj miejscowości po nazwie</label>
<div class="rzad"><input type="text" id="szukaj" maxlength="60" placeholder="np. Kraków"><button onclick="szukajMiasta()">Szukaj</button></div>
<div id="miasta"></div>
<details><summary class="szary maly" style="padding:8px 0;min-height:0">Wpisz współrzędne ręcznie</summary>
<div style="display:flex;flex-direction:column;gap:8px">
<p class="szary maly" id="miasto-stan"></p>
<label for="m-nazwa" class="szary maly">Nazwa</label><input type="text" id="m-nazwa" maxlength="40">
<label for="m-lat" class="szary maly">Szerokość geograficzna (np. 52.2297)</label><input type="text" id="m-lat" maxlength="11">
<label for="m-lon" class="szary maly">Długość geograficzna (np. 21.0122)</label><input type="text" id="m-lon" maxlength="11">
<button class="gl" onclick="zapiszMiasto(id_('m-nazwa').value,id_('m-lat').value,id_('m-lon').value)">Zapisz</button>
</div></details>
<div id="miasto-wynik"></div>
</div></details>

<details class="karta" id="p-zegar">
<summary><svg class="i"><use href="#is"/></svg><h2>Stan zegara</h2><span class="opis" id="o-zegar"></span></summary>
<div class="tresc"><div>
<div class="wiersz"><span class="szary" style="flex:1">Wersja oprogramowania</span><b id="z-wersja"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Czas pracy</span><b id="z-czas"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Godzina na zegarze</span><b id="z-godzina"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Ostatnie uruchomienie</span><b id="z-restart"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Wolna pamięć</span><b id="z-pamiec"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Sygnał Wi-Fi</span><b id="z-sygnal"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Pogoda pobrana</span><b id="z-pogoda"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Dźwięk</span><b id="z-dzwiek"></b></div>
<div class="wiersz"><span class="szary" style="flex:1">Partycja programu</span><b id="z-partycja"></b></div>
</div>
</div></details>

<details class="karta" id="p-akt">
<summary><svg class="i"><use href="#iu"/></svg><h2>Aktualizacja oprogramowania</h2><span class="opis" id="o-akt"></span></summary>
<div class="tresc">
<div class="uwaga" id="akt-uwaga" hidden></div>
<input type="file" id="plik" accept=".bin" class="ukryty">
<label for="plik" class="plik" id="plik-pole"><svg class="i szary"><use href="#iu"/></svg><span><b id="plik-nazwa"></b><br><span class="szary maly" id="plik-opis"></span></span></label>
<button class="gl" id="akt-wgraj" onclick="wgraj()" disabled>Wgraj i uruchom zegar ponownie</button>
<div class="postep" id="akt-postep" hidden><div></div></div>
<div id="akt-wynik"></div>
<p class="maly szary">Jeśli nowa wersja nie zadziała, zegar sam wróci do poprzedniej. Aktualizacja nie jest możliwa, gdy alarm dzwoni, trwa drzemka albo alarm zadzwoni w ciągu 3 minut.</p>
</div></details>
</div>
<p class="szary maly stopka" id="stopka"></p>
</main>

<script>
// --- Teksty strony w jednym miejscu (słownik): całe zdania z {miejscami} na wartości, bez sklejania z kawałków,
// żeby łatwo było je poprawić albo przetłumaczyć. Teksty stałe są w HTML wyżej, komunikaty zegara w teksty_serwera.h.
// Zasady: komunikat w polu wyniku wielką literą i z kropką; zegar w trzeciej osobie („Zegar…”);
// w zdaniach pełne słowa, skróty (min, h, s, KB) tylko w krótkich polach; jasność w rodzaju żeńskim (pełna, średnia, nocna).
const TXT={
  poziomy:['ekran zgaszony','nocna','średnia','pełna'],
  dzien:['dziś','21 czerwca','21 grudnia'],
  bladHttp:'Błąd {kod}.',
  zegarRestart:'Zegar uruchamia się ponownie...',
  wifiBrakPolaczenia:'brak połączenia z {ssid}',
  wifiBrakSieci:'brak zapisanej sieci',
  zasieg:'sygnał: {n} z 4 kresek',
  sygnalJak:['słaby','średni','dobry'],
  sygnal:'{dbm} dBm ({jak})',
  sygnalBrak:'brak połączenia',
  stopka:'{host} · wersja {w}',
  stopkaIp:'{host} ({ip}) · wersja {w}',
  pogoda:'{temp}, {opis}, {cisn}',
  alarmDzwoni:'dzwoni',
  alarmDrzemkaDo:'drzemka do {g}',
  alarmWylaczony:'wyłączony',
  alarmZa:'za {czas}',
  tempPomiar:'z pomiaru',
  tempOdcz:'odczuwalna',
  tempBrak:'Pogoda nie została jeszcze pobrana.',
  tempTeraz:'Teraz: z pomiaru <b>{p}°C</b>, odczuwalna <b>{o}°C</b>.',
  tempZapisPomiar:'Zapisano. Zegar pokazuje teraz temperaturę z pomiaru.',
  tempZapisOdcz:'Zapisano. Zegar pokazuje teraz temperaturę odczuwalną.',
  sieciSzukam:'Zegar szuka sieci w pobliżu...',
  sieciBrak:'Nie znaleziono żadnej sieci. Wpisz nazwę ręcznie.',
  sieciBlad:'Nie udało się pobrać listy sieci. Wpisz nazwę ręcznie.',
  wifiZapisano:'<h1>Zapisano</h1><p>Zegar uruchamia się ponownie i łączy z siecią <b>{ssid}</b>. Ta strona przestanie działać, a telefon sam wróci do swojej sieci.</p>'+
    '<p>Jeśli po około 2 minutach na zegarze znów pojawi się kod QR z czerwonym napisem „błędna nazwa sieci lub hasło”, zeskanuj kod jeszcze raz i popraw nazwę sieci albo hasło.</p>'+
    '<p>Pozostałe ustawienia (jasność, temperatura, miejscowość) zmienisz na stronie <a href="http://budzik.local">http://budzik.local</a>, gdy telefon wróci do sieci domowej.</p>',
  miastoSzukam:'Zegar szuka miejscowości...',
  miastoBrak:'Nie znaleziono takiej miejscowości.',
  miastoZapisano:'Zapisano: {n}.',
  miastoTeraz:'Teraz: {n} ({lat}, {lon})',
  jasnoscTeraz:'teraz: {p}',
  jasnoscZapisano:'Zapisano. Zegar już używa nowej jasności.',
  przedSnemZakres:'Godzina w polu „Przed snem” musi być między 12:00 a 23:59.',
  nocZakres:'Godzina w polu „Noc” musi być nie wcześniejsza niż „Przed snem” ({sen}) albo między 0:00 a 6:00.',
  minuty:['minuta','minuty','minut'],
  drzemkaZapisana:'Zapisano. Drzemka będzie trwała {n}.',
  slonce:'{dzien}: wschód <b>{w}</b>, zachód <b>{z}</b> <span class="szary">(przerywane linie)</span>',
  slonceBrak:'Wschodu i zachodu słońca nie da się policzyć. Sprawdź miejscowość.',
  rano:'{dzien} rozjaśnia się {od}–{do}',
  wieczor:'{dzien} przyciemnia się {od}–{do}',
  wieczorKrotko:'{dzien} przyciemnia się {od}–{do}, potem od razu jasność nocna',
  wieczorBez:'{dzien} bez przyciemniania: o {g} od razu jasność nocna',
  zegarPoBledzie:'uruchomiony po błędzie',
  zegarDziala:'działa {czas}',
  czasPonizejMinuty:'poniżej 1 min',
  wersjaRobocza:'wersja robocza: {n} po {w}',
  wersjaRoboczaNiezapisane:'wersja robocza: {n} po {w} i niezapisane zmiany',
  wersjaNiezapisane:'z niezapisanymi zmianami',
  wersjaOpis:'z {data}, ESP-IDF {idf}, git {git}',
  zmiany:['zmiana','zmiany','zmian'],
  dni:['dzień','dni','dni'],
  pamiecMin:'najmniej {kb} KB od uruchomienia',
  godzinaNieznana:'nieznana',
  pogodaWiek:'{czas} temu',
  pogodaPrzedChwila:'przed chwilą',
  pogodaNiePobrana:'jeszcze nie pobrana',
  dzwiekOk:'działa',
  dzwiekBrak:'brak (moduł MP3 albo karta SD)',
  partycja:'{p} (ota_{n})',
  wersjaStan:{ok:'wersja zatwierdzona',sprawdzana:'wersja w trakcie sprawdzania',wycofana:'poprzednia wersja przywrócona',restart:'nowa wersja wgrana, ponowne uruchomienie'},
  bezDrugiejPartycji:'bez drugiej partycji',
  aktUwagaMiejsce:'Zegar nie ma miejsca na drugą wersję programu. Wgraj program jeden raz przez kabel USB, potem aktualizacja przez Wi-Fi będzie działać.',
  aktUwagaAlarm:'Alarm dzwoni, trwa drzemka albo alarm zadzwoni w ciągu 3 minut. Wgraj program później.',
  aktUwagaWycofana:'Ostatnia aktualizacja nie zadziałała. Zegar wrócił do poprzedniej wersji.',
  aktOpis:{niedostepna:'niedostępna',sprawdzana:'sprawdzanie nowej wersji',wycofana:'nie zadziałała',restart:'ponowne uruchomienie'},
  aktSprawdza:'Działa nowa wersja {w}. Zegar sprawdza ją przez około minutę...',
  aktWycofana:'Nowa wersja nie zadziałała. Zegar wrócił do wersji {w}.',
  aktGotowe:'Gotowe. Działa nowa wersja {w}.',
  aktNieBin:'Wybierz plik .bin (firmware.bin).',
  aktWysylanie:'Wysyłanie {kb} KB...',
  aktWgrane:'Wgrane. Zegar uruchamia się ponownie...',
  aktPrzerwane:'Przesyłanie zostało przerwane. Sprawdź połączenie i spróbuj jeszcze raz.',
  plikWybierz:'Wybierz plik oprogramowania',
  plikPodpowiedz:'firmware.bin z .pio/build/esp32dev w folderze projektu',
  plikWybrany:'{kb} KB · kliknij, aby wybrać inny plik',
};
// Szablon: t('alarmZa',{czas:'2 h'}). Wartości wstawiane jak są: przy tekście od użytkownika najpierw esc().
function t(k,v){return TXT[k].replace(/\{(\w+)\}/g,(m,x)=>v&&x in v?v[x]:m)}
// Odmiana po polsku: 1 zmiana, 2–4 zmiany (bez 12–14), 5 zmian
function odm(n,f){const d=n%10,s=n%100;return n+' '+(n==1?f[0]:d>=2&&d<=4&&(s<12||s>14)?f[1]:f[2])}
// Krótki czas (pola i podpisy): "2 h 5 min", "2 h", "45 min"
function krotko(min){const h=Math.floor(min/60),m=min%60;return h?h+' h'+(m?' '+m+' min':''):m+' min'}
// Słowna ocena sygnału Wi-Fi (te same progi co kreski zasięgu)
function sygnalJak(dbm){return TXT.sygnalJak[dbm>=-60?2:dbm>=-70?1:0]}
const KOL=[[42,42,46],[90,52,24],[208,109,34],[255,140,58]];  // #2a2a2e #5a3418 #d06d22 #ff8c3a
// Wysokość wykresu: jasność tak, jak ją widzi oko (CIE L*), z PWM zegara 0/2/16/60 z 512 (display.hpp + LovyanGFX)
const LS=[0,9,46,92];  // poziomy 0..3: zgaszony, nocna, średnia, pełna
let J=null,Z=null,M=null,T=null,D=null,szukano=false,zapisano=false,dzien=0;
function id_(x){return document.getElementById(x)}
// Ukrywanie przez atrybut: elementy <svg> nie mają właściwości .hidden (przypisanie nic nie zmienia)
function ukryj(e,h){e.toggleAttribute('hidden',!!h)}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function wynik(el,ok,txt){id_(el).innerHTML='<span class="'+(ok?'ok':'blad')+'">'+esc(txt)+'</span>'}
async function api(url,dane){
  // Każde zapytanie zmieniające ustawienia niesie nagłówek X-Budzik (zegar odrzuca takie bez niego: ochrona przed cudzymi stronami)
  const o=dane===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json','X-Budzik':'1'},body:JSON.stringify(dane)};
  const r=await fetch(url,o);let j={};try{j=await r.json()}catch(e){}
  if(!r.ok)throw new Error(j.blad||t('bladHttp',{kod:r.status}));return j}
function d2(n){return String(n).padStart(2,'0')}
function hm(m){return d2(Math.floor(m/60))+':'+d2(m%60)}
function hk(m){return Math.floor(m/60)+':'+d2(m%60)}
function czas(v){const a=v.split(':');return (+a[0])*60+(+a[1])}
function minutyTeraz(){const d=new Date();return d.getHours()*60+d.getMinutes()}
// Wschód i zachód (minuty doby, czas lokalny telefonu): ten sam wzór NOAA co w zegarze (sun.cpp). null = nie da się policzyć
function slonce(d){
  if(!M)return null;const lat=parseFloat(M.lat),lon=parseFloat(M.lon);if(isNaN(lat)||isNaN(lon))return null;
  const n=Math.round((Date.UTC(d.getFullYear(),d.getMonth(),d.getDate())-Date.UTC(d.getFullYear(),0,1))/864e5);
  const g=2*Math.PI/365*n,r=Math.PI/180,C=Math.cos,S=Math.sin;
  const eq=229.18*(0.000075+0.001868*C(g)-0.032077*S(g)-0.014615*C(2*g)-0.040849*S(2*g));
  const de=0.006918-0.399912*C(g)+0.070257*S(g)-0.006758*C(2*g)+0.000907*S(2*g)-0.002697*C(3*g)+0.00148*S(3*g);
  const c=C(90.833*r)/(C(lat*r)*C(de))-Math.tan(lat*r)*Math.tan(de);if(c<-1||c>1)return null;
  const ha=Math.acos(c)/r,off=-new Date(d.getFullYear(),d.getMonth(),d.getDate(),12).getTimezoneOffset();
  const w=Math.round(720-4*(lon+ha)-eq+off),z=Math.round(720-4*(lon-ha)-eq+off);
  return w<0||z>=1440?null:{w:w,z:z}}
// Dziś wg zegara (gdy podał), inaczej policzone tutaj; bez danych 6:00 i 18:00 jak w zegarze
function dzis(){if(J&&J.wschod>=0)return {w:J.wschod,z:J.zachod};return slonce(new Date())}
// Słońce dla wybranej zakładki: dziś, 21 czerwca, 21 grudnia (tego roku)
function wybrany(){return dzien==0?dzis():slonce(new Date(new Date().getFullYear(),dzien==1?5:11,21))}
// Przepis jasności (settings.hpp): noc, rano, pelny, wieczor, sen. Ważne chwile dnia:
function chwile(p,ws){const r=ws?ws.w:360,z=ws?ws.z:1080,wz=z+p.wieczor;return {r:r,koniec:Math.max(r+p.rano,p.pelny),wz:wz,wk:wz+30}}
// Jasność widziana okiem o minucie m: ta sama kolejność co Settings::brightnessAt w zegarze
function jasn(p,m,ws){const c=chwile(p,ws);
  if(p.noc>=720&&m>=p.noc)return 0;
  if(m>=p.sen)return LS[1];
  if(m<c.r)return (p.noc>0&&p.noc<720&&m<p.noc)?LS[1]:0;
  if(m<c.koniec)return LS[1]+(LS[3]-LS[1])*(m-c.r)/(c.koniec-c.r);
  if(m<c.wz)return LS[3];
  if(m<c.wk)return LS[3]+(LS[2]-LS[3])*(m-c.wz)/30;
  return LS[2]}
function poziom(l){let k=0;for(let i=1;i<4;i++)if(Math.abs(LS[i]-l)<Math.abs(LS[k]-l))k=i;return k}
// Kolor jasności l: płynnie między kolorami poziomów
function kolor(l){let i=0;while(i<2&&l>LS[i+1])i++;const t=Math.max(0,Math.min(1,(l-LS[i])/(LS[i+1]-LS[i]))),a=KOL[i],b=KOL[i+1];
  return 'rgb('+a.map((v,k)=>Math.round(v+(b[k]-v)*t)).join(',')+')'}
// Doba jako pasek kolorów (co 10 min)
function gradient(p,ws){const s=[];for(let m=0;m<=1440;m+=10)s.push(kolor(jasn(p,Math.min(m,1439),ws))+' '+(m/14.4).toFixed(2)+'%');
  return 'linear-gradient(90deg,'+s.join(',')+')'}
function wysokosc(l){return 96-l/92*90}  // % od góry wykresu: pełny 6, średni 51, nocny 87, zgaszony 96
function kopia(x){return JSON.parse(JSON.stringify(x))}
function rowne(a,b){return JSON.stringify(a)==JSON.stringify(b)}

async function wczytaj(){
  let s;
  try{s=await api('/api/stan')}
  catch(e){
    // Po wgraniu nowej wersji brak odpowiedzi to restart zegara, nie awaria
    if(poAkt){poAkt.restart=true;wynik('akt-wynik',true,TXT.zegarRestart);return}
    id_('baner').hidden=false;id_('stan').classList.add('stare');return}
  id_('baner').hidden=true;id_('stan').classList.remove('stare');
  // W sieci zegara tylko Wi-Fi: reszta ustawień potrzebuje internetu, jest pod budzik.local
  id_('sekcja-wifi').hidden=!s.ap;id_('reszta').hidden=s.ap;
  if(s.ap&&!szukano){szukano=true;szukajSieci()}
  id_('s-wifi').textContent=s.wifi.polaczono?s.wifi.ssid:(s.wifi.ssid?t('wifiBrakPolaczenia',{ssid:s.wifi.ssid}):TXT.wifiBrakSieci);
  zasieg(s.wifi.polaczono&&s.zegar?s.zegar.sygnal:undefined);
  opisPogoda(s);
  if(s.ap&&!id_('ssid').value)id_('ssid').value=s.wifi.ssid;
  opisAlarm(s.alarm,s.ap);
  opisTemp(s.temperatura);
  opisDrzemka(s.drzemka_min);
  id_('o-miasto').textContent=s.miasto.nazwa;
  id_('miasto-stan').textContent=t('miastoTeraz',{n:s.miasto.nazwa,lat:s.miasto.lat,lon:s.miasto.lon});
  M=s.miasto;
  if(!J&&!s.ap){Z=s.jasnosc;J=kopia(Z);startJasnosc()}
  opisJasnosc();
  if(s.zegar)opisZegar(s.zegar);
  stopka(s);
}
// Stopka: "budzik.local (192.168.1.50) · wersja 2.1"; gdy strona otwarta pod samym adresem IP, bez powtórki
function stopka(s){const h=location.host||'budzik.local',ip=s.wifi.ip,jestIp=/^[\d.:]+$/.test(h);
  if(s.zegar)id_('stopka').textContent=t(ip&&!jestIp?'stopkaIp':'stopka',{host:h,ip:ip,w:s.zegar.wersja})}
// Kreski zasięgu Wi-Fi: 4 od -60 dBm, 3 od -70, 2 od -80, niżej 1
function zasieg(s){const z=id_('s-zasieg');ukryj(z,s===undefined);if(s===undefined)return;
  const n=s>=-60?4:s>=-70?3:s>=-80?2:1;
  z.querySelectorAll('rect').forEach((e,i)=>e.setAttribute('fill',i<n?'#ededed':'#3a3a40'));
  z.setAttribute('aria-label',t('zasieg',{n:n}))}
// Pogoda z zegara ma postać "0°C | 1027 hPa | mgła": na stronie jednym napisem "0°C, mgła, 1027 hPa";
// przy temperaturze odczuwalnej dopisek pod słowem "Pogoda"
function opisPogoda(s){const c=s.pogoda.split(' | '),p=id_('s-pogoda'),ok=c.length==3;
  p.textContent=ok?t('pogoda',{temp:c[0],opis:c[2],cisn:c[1]}):s.pogoda;
  id_('s-odcz').hidden=!(ok&&s.temperatura&&s.temperatura.odczuwalna);
  p.className=(s.pogoda_kod=='ok'||s.pogoda_kod=='czekam')?'':'blad'}
// Alarm tylko do podglądu: ustawia się pokrętłem
function opisAlarm(a,ap){
  const brak=!a||ap;id_('w-alarm').hidden=brak;if(brak)return;
  const on=a.wlaczony;
  ukryj(id_('a-on'),!on);ukryj(id_('a-off'),on);
  id_('a-godz').textContent=hk(a.godzina);id_('a-godz').className='godz'+(on?'':' szary');
  // Jeden chip: stan i czas razem („za 9 h 59 min” zielony, „wyłączony” szary, „dzwoni” / „drzemka do 7:05” pomarańczowy)
  let chip,kl='';
  if(a.tryb=='dzwoni'){chip=TXT.alarmDzwoni;kl='pom'}
  else if(a.tryb=='drzemka'){const k=new Date(Date.now()+a.drzemka_s*1000);chip=t('alarmDrzemkaDo',{g:k.getHours()+':'+d2(k.getMinutes())});kl='pom'}
  else if(!on){chip=TXT.alarmWylaczony}
  else{let r=(a.godzina-minutyTeraz()+1440)%1440;if(!r)r=1440;chip=t('alarmZa',{czas:krotko(r)});kl='ok'}
  id_('a-chip').textContent=chip;id_('a-chip').className='chip'+(kl?' '+kl:'')}
// Panel "Temperatura" (która jest na ekranie zegara): wybór zapisany w zegarze, obie wartości do porównania
function opisTemp(w){if(!w)return;T=w.odczuwalna;
  id_('zakl-t').querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',(b.dataset.t=='1')==T));
  id_('o-temp').textContent=T?TXT.tempOdcz:TXT.tempPomiar;
  id_('temp-teraz').innerHTML=w.pomiar_st===undefined?'<span class="szary">'+TXT.tempBrak+'</span>':
    t('tempTeraz',{p:w.pomiar_st,o:w.odczuwalna_st})}
async function zapiszTemp(v){if(v===T)return;
  try{await api('/api/temperatura',{odczuwalna:v});wynik('temp-wynik',true,v?TXT.tempZapisOdcz:TXT.tempZapisPomiar);wczytaj()}
  catch(e){wynik('temp-wynik',false,e.message)}}
// Panel "Drzemka": długość w minutach (5-9), zapisana w zegarze; działa od następnej drzemki
function opisDrzemka(m){if(m===undefined)return;D=m;
  id_('zakl-d').querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',+b.dataset.m===m));
  id_('o-drzemka').textContent=odm(m,TXT.minuty)}
async function zapiszDrzemke(m){if(m===D)return;
  try{await api('/api/drzemka',{minuty:m});wynik('drzemka-wynik',true,t('drzemkaZapisana',{n:odm(m,TXT.minuty)}));wczytaj()}
  catch(e){wynik('drzemka-wynik',false,e.message)}}
// Lista sieci wczytuje się sama po otwarciu strony; przy chwilowym błędzie jeszcze 2 próby
async function szukajSieci(proba=0){
  id_('sieci').textContent=TXT.sieciSzukam;
  try{const s=await api('/api/sieci');
    if(!s.sieci.length){id_('sieci').textContent=TXT.sieciBrak;return}
    id_('sieci').innerHTML=s.sieci.map(n=>'<button class="wynik" data-s="'+esc(n.ssid)+'">'+esc(n.ssid)+' <span class="szary maly">('+t('sygnal',{dbm:n.sila,jak:sygnalJak(n.sila)})+')</span></button>').join('');
    id_('sieci').querySelectorAll('button').forEach(b=>b.onclick=()=>{id_('ssid').value=b.dataset.s;id_('haslo').focus()});
  }catch(e){
    if(proba<2){setTimeout(()=>szukajSieci(proba+1),3000);return}
    id_('sieci').textContent=TXT.sieciBlad}
}
async function zapiszWifi(){
  const ssid=id_('ssid').value;
  try{await api('/api/wifi',{ssid:ssid,haslo:id_('haslo').value});}
  catch(e){wynik('wifi-wynik',false,e.message);return}
  zapisano=true;
  document.querySelector('main').innerHTML='<section class="karta"><div class="tresc" style="padding-top:18px">'+t('wifiZapisano',{ssid:esc(ssid)})+'</div></section>';
}
async function szukajMiasta(){
  const q=id_('szukaj').value.trim();if(!q)return;
  id_('miasta').textContent=TXT.miastoSzukam;
  try{const s=await api('/api/szukaj?q='+encodeURIComponent(q));
    if(!s.wyniki.length){id_('miasta').textContent=TXT.miastoBrak;return}
    id_('miasta').innerHTML=s.wyniki.map((m,i)=>'<button class="wynik" data-i="'+i+'"><b>'+esc(m.nazwa)+'</b><br><span class="szary maly">'+esc([m.region,m.kraj].filter(x=>x).join(', '))+' · '+m.lat+', '+m.lon+'</span></button>').join('');
    id_('miasta').querySelectorAll('button').forEach(b=>b.onclick=()=>{const m=s.wyniki[b.dataset.i];zapiszMiasto(m.nazwa,m.lat,m.lon)});
  }catch(e){id_('miasta').innerHTML='<span class="blad">'+esc(e.message)+'</span>'}
}
async function zapiszMiasto(n,lat,lon){
  try{await api('/api/miasto',{nazwa:n,lat:lat,lon:lon});wynik('miasto-wynik',true,t('miastoZapisano',{n:n}));id_('miasta').textContent='';setTimeout(wczytaj,4000);wczytaj()}
  catch(e){wynik('miasto-wynik',false,e.message)}
}

// --- Jasność: przepis trzymany w zegarze (NVS), tu edytowany i wysyłany jednym zapisem ---
// Jeden przepis na cały tydzień. Zegar trzyma dwa (dni robocze i wolne, settings.hpp): strona wysyła ten sam dwa razy.
function startJasnosc(){J.profile[1]=kopia(J.profile[0]);doFormy();rysujWykres()}
function doFormy(){const p=J.profile[0];id_('j-noc').value=hm(p.noc);id_('j-rano').value=p.rano;id_('j-pelny').value=hm(p.pelny);id_('j-wieczor').value=p.wieczor;id_('j-sen').value=hm(p.sen)}
// Noc (zgaszony od): nie wcześniej niż "przed snem" albo po północy, do 6:00 (ta sama zasada w zegarze: Settings::isBrightnessValid)
function nocOk(n,s){return n>=s||n<=360}
// Zwraca tekst błędu (albo ''); błędne pole wraca do ostatniej dobrej wartości
function zFormy(){const p=J.profile[0],v=x=>id_(x).value;let e='';
  p.rano=+v('j-rano');p.pelny=czas(v('j-pelny')||'07:00');p.wieczor=+v('j-wieczor');
  const n=czas(v('j-noc')||'00:00'),s=czas(v('j-sen')||'22:00');
  if(s<720){e=TXT.przedSnemZakres;id_('j-sen').value=hm(p.sen)}
  else if(!nocOk(n,s)){e=t('nocZakres',{sen:hk(s)});id_('j-noc').value=hm(p.noc);id_('j-sen').value=hm(p.sen)}
  else{p.noc=n;p.sen=s}
  J.profile[1]=kopia(p);return e}
// Do wysłania tylko to, co różni się od zapisanego w zegarze (zmiana i powrót do tej samej wartości = nic)
function sprawdzZmiany(){const r=!rowne(J.profile,Z.profile);id_('zapisz-j').disabled=!r;if(r)id_('jasnosc-wynik').textContent=''}
// Wieczorne przyciemnianie a "przed snem": jasność nocna wygrywa, więc przyciemnianie skraca się albo w ogóle go nie ma
function wiecz(c,p,dz){
  if(c.wz>=p.sen)return t('wieczorBez',{dzien:dz,g:hk(p.sen)});
  if(c.wk>p.sen)return t('wieczorKrotko',{dzien:dz,od:hk(c.wz),do:hk(p.sen)});
  return t('wieczor',{dzien:dz,od:hk(c.wz),do:hk(c.wk)})}
// Wykres wybranego dnia: kolorowe pole przycięte do kształtu jasności, przerywane linie wschodu i zachodu
function rysujWykres(){
  const p=J.profile[0],ws=wybrany(),c=chwile(p,ws),pole=id_('pole'),pk=['0% 100%'];
  for(let m=0;m<=1440;m+=5)pk.push((m/14.4).toFixed(2)+'% '+wysokosc(jasn(p,Math.min(m,1439),ws)).toFixed(1)+'%');
  pk.push('100% 100%');
  pole.style.background=gradient(p,ws);pole.style.clipPath=pole.style.webkitClipPath='polygon('+pk.join(',')+')';
  [['sl-w','w'],['sl-z','z']].forEach(x=>{const e=id_(x[0]);e.hidden=!ws;if(ws)e.style.left=(ws[x[1]]/14.4)+'%'});
  const dz=TXT.dzien[dzien];
  id_('slonce-txt').innerHTML=ws?t('slonce',{dzien:dz,w:hk(ws.w),z:hk(ws.z)}):'<span class="szary">'+TXT.slonceBrak+'</span>';
  id_('d-rano').textContent=t('rano',{dzien:dz,od:hk(c.r),do:hk(c.koniec)});
  id_('d-wieczor').textContent=wiecz(c,p,dz);
  id_('zakl').querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',b.dataset.d==dzien));
  sprawdzZmiany();opisJasnosc();
}
// Nagłówek panelu, pasek doby pod nagłówkiem i biała kreska "teraz" (wg zegarka telefonu, tylko dziś)
function opisJasnosc(){
  if(!J)return;
  const m=minutyTeraz(),ws=dzis(),p=J.profile[0],l=jasn(p,m,ws),x=(m/14.4)+'%';
  id_('o-jasnosc').textContent=t('jasnoscTeraz',{p:TXT.poziomy[l?poziom(l):0]});
  id_('pasek').style.background=gradient(p,ws);id_('pasek-teraz').style.left=x;
  const k=id_('teraz');k.hidden=dzien!=0;k.style.left=x;
}
// Panel "Stan zegara"
function czasPracy(s){const m=Math.floor(s/60),h=Math.floor(m/60),d=Math.floor(h/24);
  return d?odm(d,TXT.dni)+(h%24?' '+(h%24)+' h':''):m?krotko(m):TXT.czasPonizejMinuty}
function opisZegar(z){
  id_('o-zegar').textContent=z.restart_zly?TXT.zegarPoBledzie:t('zegarDziala',{czas:czasPracy(z.czas_s)});
  id_('o-zegar').className='opis'+(z.restart_zly?' blad':'');
  // "2.1+3": wersja robocza, 3 zmiany po 2.1; "*" = niezapisane zmiany
  const pr=/^([\d.]+)\+(\d+)/.exec(z.wersja),nz=z.wersja.endsWith('*');
  const robocza=pr?t(nz?'wersjaRoboczaNiezapisane':'wersjaRobocza',{n:odm(+pr[2],TXT.zmiany),w:esc(pr[1])}):nz?TXT.wersjaNiezapisane:'';
  id_('z-wersja').innerHTML=esc(z.wersja)+'<br><span class="szary maly">'+(robocza?robocza+'<br>':'')+
    t('wersjaOpis',{data:esc(z.data||'?'),idf:esc(z.idf),git:esc(z.git||'?')})+'</span>';
  id_('z-czas').textContent=czasPracy(z.czas_s);
  id_('z-restart').textContent=z.restart;id_('z-restart').className=z.restart_zly?'blad':'';
  id_('z-pamiec').innerHTML=Math.round(z.sterta/1024)+' KB<br><span class="szary maly">'+t('pamiecMin',{kb:Math.round(z.sterta_min/1024)})+'</span>';
  id_('z-sygnal').textContent=z.sygnal===undefined?TXT.sygnalBrak:t('sygnal',{dbm:z.sygnal,jak:sygnalJak(z.sygnal)});
  // Godzina, którą zegar zna (bez internetu po zaniku prądu jej nie ma), i wiek ostatniej udanej pogody (od 2 h na czerwono)
  id_('z-godzina').textContent=z.godzina||TXT.godzinaNieznana;id_('z-godzina').className=z.godzina?'':'blad';
  const pw=z.pogoda_wiek_s,pm=Math.floor(pw/60);
  id_('z-pogoda').textContent=pw===undefined?TXT.pogodaNiePobrana:pm<1?TXT.pogodaPrzedChwila:t('pogodaWiek',{czas:krotko(pm)});
  id_('z-pogoda').className=(pw===undefined?z.czas_s:pw)>=7200?'blad':'';
  // Moduł MP3 z kartą SD (zegar pyta go raz na godzinę): bez niego alarm nie zagra
  id_('z-dzwiek').textContent=z.dzwiek===false?TXT.dzwiekBrak:TXT.dzwiekOk;id_('z-dzwiek').className=z.dzwiek===false?'blad':'';
  // Stan działającej wersji (ota.hpp): po wgraniu przez minutę sprawdzana, potem zatwierdzona albo powrót do poprzedniej
  const st=TXT.wersjaStan[z.ota_stan]||z.ota_stan;
  id_('z-partycja').innerHTML=t('partycja',{p:esc(z.miejsce),n:esc(z.miejsce.slice(-1))})+'<br><span class="szary maly">'+(z.ota?esc(st):TXT.bezDrugiejPartycji)+'</span>';
  opisAkt(z);
}
// Panel "Aktualizacja oprogramowania": plik idzie w całości jako treść zapytania, postęp z XMLHttpRequest
let Zg=null,aktTrwa=false,poAkt=null,ostCzas=0;  // poAkt: po udanym wgraniu, do czasu odpowiedzi nowej wersji
function opisAkt(z){Zg=z;
  // Ostrzeżenia tylko wtedy, gdy coś blokuje albo poszło źle; wersję i partycję pokazuje "Stan zegara"
  const u=!z.ota?TXT.aktUwagaMiejsce:z.alarm_zajety?TXT.aktUwagaAlarm:z.ota_stan=='wycofana'?TXT.aktUwagaWycofana:'';
  id_('akt-uwaga').textContent=u;id_('akt-uwaga').hidden=!u;
  const k=!z.ota?'niedostepna':z.ota_stan,o=id_('o-akt');
  o.textContent=TXT.aktOpis[k]||'';o.className='opis'+(k=='wycofana'?' blad':'');
  if(poAkt){
    if(z.czas_s<ostCzas)poAkt.restart=true;  // czas pracy od nowa: zegar się uruchomił ponownie
    if(poAkt.restart&&z.ota_stan!='restart'){
      if(z.ota_stan=='sprawdzana')wynik('akt-wynik',true,t('aktSprawdza',{w:z.wersja}));
      else{if(z.ota_stan=='wycofana')wynik('akt-wynik',false,t('aktWycofana',{w:z.wersja}));
        else wynik('akt-wynik',true,t('aktGotowe',{w:z.wersja}));
        poAkt=null;id_('akt-postep').hidden=true;id_('plik').value='';opisPliku()}}}
  ostCzas=z.czas_s;przyciskAkt()}
function przyciskAkt(){const z=Zg;
  id_('akt-wgraj').disabled=aktTrwa||poAkt!==null||!id_('plik').files.length||!z||!z.ota||z.alarm_zajety||z.ota_stan=='restart'}
function szybko(){if(poAkt)setTimeout(async()=>{await wczytaj();szybko()},3000)}
function wgraj(){const f=id_('plik').files[0];if(!f)return;
  if(!/\.bin$/i.test(f.name)){wynik('akt-wynik',false,TXT.aktNieBin);return}
  const x=new XMLHttpRequest(),p=id_('akt-postep'),b=p.firstElementChild;
  aktTrwa=true;przyciskAkt();p.hidden=false;b.style.width='0';id_('akt-wynik').textContent=t('aktWysylanie',{kb:Math.round(f.size/1024)});
  x.upload.onprogress=e=>{if(e.lengthComputable)b.style.width=(e.loaded/e.total*100)+'%'};
  x.onload=()=>{aktTrwa=false;let j={};try{j=JSON.parse(x.responseText)}catch(e){}
    if(x.status==200){b.style.width='100%';poAkt={restart:false};wynik('akt-wynik',true,TXT.aktWgrane);szybko()}
    else{p.hidden=true;wynik('akt-wynik',false,j.blad||t('bladHttp',{kod:x.status}))}
    przyciskAkt()};
  x.onerror=()=>{aktTrwa=false;p.hidden=true;wynik('akt-wynik',false,TXT.aktPrzerwane);przyciskAkt()};
  x.open('POST','/api/aktualizacja');x.setRequestHeader('X-Budzik','1');x.send(f)}
// Pole pliku: nazwa i rozmiar wybranego pliku zamiast podpowiedzi
function opisPliku(){const f=id_('plik').files[0];id_('plik-pole').classList.toggle('wybrany',!!f);
  id_('plik-nazwa').textContent=f?f.name:TXT.plikWybierz;
  id_('plik-opis').textContent=f?t('plikWybrany',{kb:Math.round(f.size/1024)}):TXT.plikPodpowiedz}
id_('plik').addEventListener('change',()=>{id_('akt-wynik').textContent='';opisPliku();przyciskAkt()});
async function zapiszJasnosc(){
  try{await api('/api/jasnosc',{profile:J.profile,dni:J.dni});Z=kopia(J);rysujWykres();wynik('jasnosc-wynik',true,TXT.jasnoscZapisano)}
  catch(e){wynik('jasnosc-wynik',false,e.message)}
}
['j-noc','j-rano','j-pelny','j-wieczor','j-sen'].forEach(x=>id_(x).addEventListener('change',()=>{const e=zFormy();rysujWykres();if(e)wynik('jasnosc-wynik',false,e)}));
id_('zakl').querySelectorAll('button').forEach(b=>b.onclick=()=>{dzien=+b.dataset.d;rysujWykres()});
id_('zakl-t').querySelectorAll('button').forEach(b=>b.onclick=()=>zapiszTemp(b.dataset.t=='1'));
id_('zakl-d').querySelectorAll('button').forEach(b=>b.onclick=()=>zapiszDrzemke(+b.dataset.m));
id_('pokaz').addEventListener('change',()=>{id_('haslo').type=id_('pokaz').checked?'text':'password'});
opisPliku();wczytaj();setInterval(()=>{if(!document.hidden&&!zapisano)wczytaj()},15000);
</script></body></html>)rawliteral";

#endif // _INCLUDE_WEB_PAGE_H_
