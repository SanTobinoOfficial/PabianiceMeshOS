# 3. Pierwszy test sieci

Na tym etapie (bez szyfrowania, sam routing) chodzi o sprawdzenie dwóch rzeczy:
że węzły w ogóle się widzą po radiu (beacon obecności) oraz że wiadomość faktycznie
wędruje przez sieć, a nie tylko leci do najbliższego sąsiada.

## Test 1: dwa węzły w tym samym pokoju

Wgraj identyczne firmware na dwie płytki i podłącz obie do komputera (albo do dwóch
oddzielnych terminali, jeśli masz dwa porty USB).

```bash
idf.py -p /dev/ttyUSB0 monitor   # węzeł A
idf.py -p /dev/ttyUSB1 monitor   # węzeł B
```

Każdy węzeł wysyła testową wiadomość broadcastową co 30 sekund (`main.c`, na razie
zaszyte na sztywno - to testowy kod, nie docelowa aplikacja). Po chwili w logu węzła A
powinieneś zobaczyć wiadomość od B i na odwrót:

```
I (30412) app: odebrano 24 B od ffffffff...: "test zasiegu z Pabianic"
```

Jeśli nic nie przychodzi - najpierw sprawdź, czy oba węzły w ogóle się beaconują
(`presence_touch` loguje się tylko na poziomie debug, więc jeśli chcesz to zobaczyć
wprost, chwilowo podnieś poziom logowania w `sdkconfig` albo dodaj `ESP_LOGI` w
`on_radio_rx` w `mesh.c`). Druga rzecz do sprawdzenia - czy oba moduły są ustawione
na tę samą częstotliwość/SF/BW (domyślnie tak, jeśli nie zmieniałeś `mesh_init()`).

## Test 2: routing przez pośredni węzeł (trzy płytki)

To jest właściwy test mesh, nie tylko radia. Potrzebujesz trzeciej płytki.

1. Ustaw węzeł A i węzeł C w takiej odległości (albo w takich pomieszczeniach), żeby
   się **nie słyszały bezpośrednio** - np. dwa piętra budynku albo dwa końce mieszkania
   z betonową ścianą pomiędzy.
2. Węzeł B (pośredni) postaw tak, żeby słyszał oba.
3. Poobserwuj logi - wiadomości broadcastowe od A powinny docierać do C wyłącznie
   za pośrednictwem B (TTL w nagłówku spada o 1 przy każdym przeskoku, domyślnie
   startuje z wartością 8, więc dwa skoki to i tak margines).

Jeśli węzeł C w ogóle nie widzi ruchu od A - to nie musi być bug w routingu, może
po prostu żaden z węzłów faktycznie nie jest w zasięgu drugiego (przy SF7/BW125
w budynku to realistycznie kilkadziesiąt-kilkaset metrów, patrz rozdz. 12.3 planu
dla realistycznych szacunków zasięgu). Test zasięgu w terenie to osobny temat -
patrz rozdz. 12.4 planu, warto to zrobić zanim zaczniesz planować rozstawienie
większej liczby węzłów po mieście.

## Czego na tym etapie NIE testujemy

Poufności treści - jej jeszcze nie ma, payload leci jawnym tekstem. To normalne na
tym etapie prac i nie jest błędem w kodzie, tylko świadomą kolejnością (najpierw
transport i routing, szyfrowanie w kroku 2). Nie testuj tego firmware w sieci, gdzie
komuś obcemu mogłoby zależeć na podsłuchaniu treści.
