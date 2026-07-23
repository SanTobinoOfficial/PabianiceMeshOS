-- Sesje trzymaly dotad surowy bearer token w bazie - wyciek bazy/backupu bylby
-- rownowazny wyciekowi wszystkich aktywnych hasel naraz (kazdy token dawal pelny
-- dostep bez znajomosci hasla). Trzymamy teraz SHA-256 z tokena, jak API keye/PAT-y
-- w wiekszosci powaznych serwisow - sam token istnieje tylko u klienta i w locie
-- przez naglowek Authorization, nigdy w spoczynku w bazie.
alter table sessions rename column token to token_hash;
