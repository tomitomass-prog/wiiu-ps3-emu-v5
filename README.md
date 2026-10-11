# DS3 v2 experimental — ESP32-S2

Prototyp USB HID do testowania zgodności z Nintendont. **Nie jest pełną emulacją DualShock 3**.
Wykorzystuje działający format UART 20 bajtów, bez zmian w firmware LOLIN32 Lite.

## Co jest nowe
- Zapis i odczyt wybranych raportów HID Feature `0xF4`, `0xF5` oraz testowy odczyt `0xF2`.
- Raport wejściowy 01 zawierający 24 przyciski i 4 osie Windows HID.
- VID/PID `054C:0268` wyłącznie do eksperymentu, a nie jako dowód zgodności.
- Obsługa Output 01 jako bezpieczne ignorowanie (bez wibracji).

**Braki:** deskryptor nadal różni się od oryginalnego DS3; odpowiedzi `0xF2` są tylko danymi testowymi; brak obsługi `0x01`, `0xEF`, `0xF7`, `0xF8` i pełnej sekwencji inicjalizacji. Nintendont może odmówić obsługi.

## Instalacja w istniejącym repozytorium GitHub
Skopiuj zawartość ZIP do głównego katalogu repozytorium, zachowując podfoldery.
Nie usuwaj starego firmware `usb-s2-ps3` ani starego workflow.
Workflow `.github/workflows/ds3-v2.yml` wygeneruje artefakt `lolin-s2-ds3-v2-experimental`.

Na S2, po sprawdzeniu adresów w plikach buildu, wgrywaj **komplet trzech obrazów** (w poprzednim eksperymencie niezgodność bootloadera/partycji powodowała brak enumeracji USB).
Standardowe adresy dla użytego układu to bootloader `0x1000`, partitions `0x8000`, firmware `0x10000`; zawsze najpierw sprawdź faktyczne pliki.

## Bezpieczeństwo testów
Najpierw testuj na komputerze z oddzielnymi źródłami zasilania obu ESP32 i wspólną masą UART. Nie łącz między płytkami 5 V ani 3V3.
Ponieważ przy poprzednich próbach Wii U wyłączało się po wspólnym zasilaniu, **nie zalecamy testowania tego wariantu na Wii U bez wcześniejszego sprawdzenia okablowania i poboru prądu**.

## Dokumentacja referencyjna
- https://github.com/nefarius/DsHidMini/blob/master/docs/PS3_USB_STARTUP.md
- https://github.com/FIX94/Nintendont
