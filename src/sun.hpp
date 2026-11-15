#ifndef _INCLUDE_SUN_HPP_
#define _INCLUDE_SUN_HPP_

// Wschód i zachód słońca dla miejscowości z ustawień (lat/lon w NVS), w czasie lokalnym (z czasem letnim).
// Wzór NOAA (dokładność ok. 1-2 min), liczony raz na dobę i po zmianie miejscowości, wynik w pamięci.
// Potrzebny do harmonogramu jasności: zmiana "przy wschodzie / zachodzie" (settings.hpp).

namespace Sun {
    // Dzisiejszy wschód i zachód: minuty doby (0..1439). -1, gdy nieznane
    // (brak godziny z NTP, błędne lat/lon, dzień albo noc polarna).
    void today(int *sunrise, int *sunset);
}

#endif // _INCLUDE_SUN_HPP_
