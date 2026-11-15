#ifndef _INCLUDE_MEM_LOG_HPP_
#define _INCLUDE_MEM_LOG_HPP_

// Pomiar pamięci w trakcie działania.
// Co MEM_LOG_PERIOD_MS do logu: wolna sterta, najniższa wolna sterta od startu,
// największy wolny blok, zapas stosu każdego zadania (najmniej wolnego od startu) i zajętość NVS.
// MEM_LOG 0 = wyłączone (zadanie nie powstaje).
#define MEM_LOG            1
#define MEM_LOG_PERIOD_MS  60000  // co minutę

void memLogStart(void);

#endif // _INCLUDE_MEM_LOG_HPP_
