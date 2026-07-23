# 3. Pierwszy test sieci

Odkąd doszło szyfrowanie end-to-end (Signal Protocol), test wygląda nieco inaczej niż
na samym routingu: wiadomości nie da się już wysłać "w eter" do wszystkich - Double
Ratchet działa parami, więc każdy węzeł musi najpierw wymienić się kluczami z konkretnym
adresatem. Poniżej pełny przepływ: beacon obecności, wymiana kluczy, zaszyfrowana
wiadomość.

## Krok 0: poznaj ID obu węzłów

Wgraj identyczne firmware na dwie płytki, podłącz obie i odczytaj ich lokalne ID z logu:

```bash
idf.py -p /dev/ttyUSB0 monitor   # węzeł A
idf.py -p /dev/ttyUSB1 monitor   # węzeł B
```

Zobaczysz coś w stylu:

```
I (330) app: wezel wystartowal, lokalne ID: a4cf12b8aabb0000
```

Skopiuj ID węzła B, wklej do `main.c` węzła A jako `PEER_UNDER_TEST` (i odwrotnie),
przebuduj i wgraj ponownie na obie płytki. Domyślnie `PEER_UNDER_TEST` to same zera -
dopóki go nie ustawisz, firmware nic nie wyśle (dostaniesz ostrzeżenie w logu przy
starcie).

## Krok 1: wymiana kluczy

Po starcie każdy węzeł, dla którego `PEER_UNDER_TEST` jest ustawiony, spróbuje co 15s
wysłać testową wiadomość - a skoro sesji jeszcze nie ma, zamiast tego poleci prośba
o komplet kluczy (`PKT_TYPE_KEY_BUNDLE`). W logu zobaczysz:

```
I (15234) app: brak sesji z peerem jeszcze, wyslano prosbe o klucze
I (15890) mesh: prosba o bundle kluczy, odsylam
I (16510) mesh: przetworzono bundle kluczy od sasiada, sesja OK
```

Ta wymiana idzie tylko jednym skokiem (bez floodingu) - węzły musza się słyszeć
bezpośrednio. To ograniczenie zniknie dopiero z katalogiem kluczy na serwerze (krok 3
planu), na razie to świadomy skrót.

## Krok 2: zaszyfrowana wiadomość

Po udanej wymianie kolejne próby wysyłki powinny przechodzić:

```
I (30234) app: wyslano zaszyfrowana wiadomosc testowa
I (30980) app: odebrano 16 B (odszyfrowane) od a4cf12b8...: "czesc z Pabianic"
```

To co leci przez radio w tym momencie to ciphertext Double Ratchet - jeśli podsłuchasz
transmisję (np. tanim SDR-em), zobaczysz nieczytelne bajty, nie tekst wiadomości.

## Test routingu przez pośredni węzeł (trzy płytki)

Wymiana kluczy działa tylko na jeden skok, ale **wiadomości DATA po ustanowieniu sesji
już floodują normalnie** (TTL=8 domyślnie) - więc jeśli A i B ustanowiły sesję będąc
blisko siebie, a potem B oddali się poza bezpośredni zasięg A, wiadomości powinny wciąż
docierać przez pośredni węzeł C, o ile C je przekazuje dalej (patrz TTL w logu).

1. Ustanów sesję A↔B w tym samym pomieszczeniu (krok 1-2 wyżej).
2. Rozstaw A i B tak, żeby przestały się słyszeć bezpośrednio, z węzłem C pomiędzy.
3. Wiadomości od A powinny nadal docierać do B, tym razem przekazywane przez C.

Jeśli nic nie dochodzi - sprawdź czy to nie kwestia zasięgu (rozdz. 12.3 planu) zanim
zaczniesz podejrzewać routing.

## Czego na tym etapie NIE testujemy

Warstwy transportowej AES na poziomie radiowym (rozdz. 6.3 planu) - jeszcze jej nie ma,
tylko E2E encryption treści. Metadane routingu (kto z kim, kiedy, ile) są nadal jawne
dla każdego kto podsłuchuje transmisję - to znane i udokumentowane ograniczenie
(rozdz. 6.5 planu), nie błąd tego etapu prac.
