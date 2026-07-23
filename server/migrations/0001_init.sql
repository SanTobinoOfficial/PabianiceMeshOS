-- node_id to te same 8 bajtow co PKT_NODE_ID_LEN w firmware/components/mesh/include/pkt.h
-- (na razie pochodna MAC, docelowo hash klucza publicznego - patrz TODO w mesh.c)

create table nodes (
    node_id           bytea primary key,
    identity_pub      bytea not null,
    registration_id   integer not null,
    signed_prekey_id  integer not null,
    signed_prekey_pub bytea not null,
    signed_prekey_sig bytea not null,
    updated_at        timestamptz not null default now()
);

-- pula one-time prekeys per wezel - kazdy wydawany co najwyzej raz (rozdz. 6.2 planu)
create table one_time_prekeys (
    id         bigserial primary key,
    node_id    bytea not null references nodes(node_id) on delete cascade,
    prekey_id  integer not null,
    prekey_pub bytea not null,
    used_at    timestamptz,
    unique (node_id, prekey_id)
);

create index one_time_prekeys_available_idx
    on one_time_prekeys (node_id)
    where used_at is null;

-- zaszyfrowane wiadomosci czekajace na odbiorce bedacego offline (store-and-forward,
-- rozdz. 5 i 10.1 planu). Serwer nie wie i nie musi wiedziec co jest w ciphertext.
create table messages (
    id         uuid primary key default gen_random_uuid(),
    dst_id     bytea not null,
    src_id     bytea not null,
    ciphertext bytea not null,
    created_at timestamptz not null default now(),
    expires_at timestamptz not null
);

create index messages_dst_idx on messages (dst_id);
create index messages_expires_idx on messages (expires_at);

-- blokowanie urzadzen po HWID - dziala tylko w tym trybie scentralizowanym,
-- patrz rozdz. 7.3.1-7.3.2 planu (w trybie czysto lokalnym nie da sie tego zrobic bezpiecznie)
create table blocked_devices (
    hwid       bytea primary key,
    reason     text,
    blocked_at timestamptz not null default now()
);
