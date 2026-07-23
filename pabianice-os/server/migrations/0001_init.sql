-- Pabianice OS - serwer kanalow tekstowych do samodzielnego hostingu (rozdz. 10.3-10.4
-- pelnego planu). Odrebna baza od /server (ktory jest DM store-and-forward + katalog
-- kluczy dla glownej sieci mesh) - to jest produkt do postawienia przez operatora dla
-- swojej spolecznosci, analogicznie do wlasnej instancji Discorda/Mattermosta.
--
-- WAZNE roznica modelu zaufania: DM w /server to zawsze ciphertext Signal Protocol,
-- serwer nigdy nie widzi tresci. Kanaly grupowe tutaj NIE sa (jeszcze) szyfrowane E2E -
-- grupowe szyfrowanie (Signal Sender Keys czy podobne) to osobny, wiekszy kawalek
-- kryptografii, ktorego libsignal-protocol-c uzywany w firmware nie dostarcza gotowego.
-- Model zaufania kanalow jest wiec na razie taki jak Discord/Matrix: operator serwera
-- widzi tresc wiadomosci na swoim serwerze. Patrz pabianice-os/README.md.

create type user_role as enum ('admin', 'moderator', 'member');

create table users (
    id            uuid primary key default gen_random_uuid(),
    username      text not null unique,
    password_hash text not null,
    role          user_role not null default 'member',
    -- identity key Signal Protocol tego uzytkownika - ta sama tozsamosc kryptograficzna
    -- co w bezposrednim mesh (rozdz. 10.4.1 planu). Opcjonalne dopoki klient tego nie
    -- publikuje, nieuzywane jeszcze do niczego w tym szkielecie poza przechowaniem.
    identity_pub  bytea,
    created_at    timestamptz not null default now()
);

create table sessions (
    token      bytea primary key,
    user_id    uuid not null references users(id) on delete cascade,
    created_at timestamptz not null default now(),
    expires_at timestamptz not null
);

create index sessions_user_idx on sessions (user_id);

create table categories (
    id         uuid primary key default gen_random_uuid(),
    name       text not null,
    position   integer not null default 0,
    created_at timestamptz not null default now()
);

create table channels (
    id          uuid primary key default gen_random_uuid(),
    category_id uuid references categories(id) on delete set null,
    name        text not null,
    topic       text,
    position    integer not null default 0,
    created_at  timestamptz not null default now()
);

create index channels_category_idx on channels (category_id);

create table channel_messages (
    id         uuid primary key default gen_random_uuid(),
    channel_id uuid not null references channels(id) on delete cascade,
    author_id  uuid not null references users(id),
    body       text not null,
    created_at timestamptz not null default now()
);

create index channel_messages_channel_idx on channel_messages (channel_id, created_at);

-- jeden wiersz - ustawienia calego serwera operatora. retencja niezalezna od
-- retencji serwera glownego /server (rozdz. 10.4.1 planu), 0 = bez automatycznego
-- czyszczenia, decyzja nalezy do operatora. federacja domyslnie wylaczona (rozdz. 10.4.2).
create table server_settings (
    id                      boolean primary key default true check (id),
    message_retention_days integer not null default 0,
    federation_enabled     boolean not null default false
);

insert into server_settings (id) values (true);
