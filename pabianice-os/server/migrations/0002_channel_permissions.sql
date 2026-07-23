-- uprawnienia per-kanal - minimalna rola do pisania (rozdz. 10.4.1 planu, "system rol
-- i uprawnien konfigurowany przez operatora"). Wczesniej kazdy zalogowany mogl pisac
-- na kazdym kanale - domyslnie zostaje tak samo (member), operator moze podniesc np.
-- kanal ogloszen do moderator/admin.
alter table channels add column min_post_role user_role not null default 'member';
