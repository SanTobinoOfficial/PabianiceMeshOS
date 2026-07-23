# 2. Budowanie i flashowanie firmware

## Instalacja ESP-IDF

Jeśli jeszcze nie masz ESP-IDF na dysku (wersja 5.x):

```bash
mkdir -p ~/esp && cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

`export.sh` trzeba odpalać w każdej nowej sesji terminala (albo dodać do `.bashrc` -
ja tak mam, wygodniej).

## Build

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

Build jest zweryfikowany w CI na czystym środowisku (`.github/workflows/firmware-ci.yml`) -
jeśli mimo to coś się wywali lokalnie na brakującej zależności w komponencie
(`REQUIRES` w CMakeLists.txt), dopisz brakujący komponent.

## Flashowanie

Podłącz płytkę kablem USB-C, sprawdź który port się pojawił:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

i wgraj:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

(zamień `/dev/ttyUSB0` na to co faktycznie zobaczyłeś wyżej - na niektórych devboardach
z natywnym USB będzie to `/dev/ttyACM0`).

`monitor` zostawia otwarty podgląd logów - powinieneś zobaczyć coś w stylu:

```
I (327) sx1262: SX1262 gotowy: 868100000 Hz, SF7
I (330) app: wezel wystartowal, lokalne ID: a4cf12b8aabb
```

Jak zamiast tego widzisz same krzaki albo nic - sprawdź prędkość transmisji monitora
(domyślnie 115200) i czy dobrze wpiąłeś RESET/BUSY (najczęstsza pomyłka przy montażu
z gołymi przewodami: zamienione miejscami BUSY i DIO1).

Wyjście z monitora: `Ctrl+]`.

Dalej: [3. Pierwszy test sieci](03-pierwszy-test-sieci.md)
