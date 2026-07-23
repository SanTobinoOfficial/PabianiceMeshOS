-- Do tej pory tylko *pisanie* mialo prog roli per-kanal (min_post_role) - kazdy
-- zalogowany, niezaleznie od roli, widzial kazdy kanal i jego wiadomosci. Dodajemy
-- analogiczny prog na *czytanie* - kanal ponizej progu roli uzytkownika jest dla niego
-- calkowicie niewidoczny (znika z listy kanalow), nie tylko zablokowany do pisania.
-- Domyslnie 'member' (kazdy zalogowany) - zero zmiany zachowania dla istniejacych
-- kanalow dopoki operator sam nie podniesie progu.
alter table channels add column min_read_role user_role not null default 'member';
