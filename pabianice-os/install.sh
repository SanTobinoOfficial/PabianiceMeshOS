#!/usr/bin/env bash
# Instalator Pabianice OS dla Debiana (rozdz. 10.3-10.4 planu) - prowadzi operatora
# przez postawienie wlasnego serwera kanalow, analogicznie do modelu YunoHost.
# Alpine na razie nie obsluzone (inny menedzer pakietow/init) - patrz README.
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
    echo "Odpal jako root (albo przez sudo) - trzeba zainstalowac pakiety i systemd unit."
    exit 1
fi

if ! command -v apt-get >/dev/null; then
    echo "Ten skrypt zaklada Debiana/Ubuntu (apt-get). Inna dystrybucja - patrz README, instalacja recznie."
    exit 1
fi

echo "== Pabianice OS - instalator =="
echo

read -rp "Nazwa uzytkownika administratora [admin]: " ADMIN_USER
ADMIN_USER=${ADMIN_USER:-admin}

read -rsp "Haslo administratora: " ADMIN_PASS
echo
if [[ -z "$ADMIN_PASS" ]]; then
    echo "Puste haslo - przerywam."
    exit 1
fi

read -rp "Port na ktorym ma nasluchiwac serwer [8081]: " BIND_PORT
BIND_PORT=${BIND_PORT:-8081}

INSTALL_DIR=/opt/pabianice-os
DB_NAME=pabianice_os
DB_USER=pabianice_os
DB_PASS=$(openssl rand -hex 16)

echo
echo "-- Pakiety systemowe (postgresql, build tools) --"
apt-get update -qq
apt-get install -y -qq postgresql postgresql-contrib build-essential pkg-config libssl-dev curl

if ! command -v cargo >/dev/null; then
    echo "-- Rust (rustup) --"
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    # shellcheck disable=SC1091
    source "$HOME/.cargo/env"
fi

echo "-- Baza danych --"
sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='${DB_USER}'" | grep -q 1 || \
    sudo -u postgres psql -c "CREATE ROLE ${DB_USER} LOGIN PASSWORD '${DB_PASS}';"
sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='${DB_NAME}'" | grep -q 1 || \
    sudo -u postgres psql -c "CREATE DATABASE ${DB_NAME} OWNER ${DB_USER};"

echo "-- Kopiowanie plikow do ${INSTALL_DIR} --"
mkdir -p "${INSTALL_DIR}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cp -r "${SCRIPT_DIR}/server" "${SCRIPT_DIR}/admin-panel" "${INSTALL_DIR}/"

cat > "${INSTALL_DIR}/server/.env" <<EOF
DATABASE_URL=postgres://${DB_USER}:${DB_PASS}@localhost:5432/${DB_NAME}
BIND_ADDR=0.0.0.0:${BIND_PORT}
RUST_LOG=info
ADMIN_BOOTSTRAP_USERNAME=${ADMIN_USER}
ADMIN_BOOTSTRAP_PASSWORD=${ADMIN_PASS}
ADMIN_PANEL_DIR=${INSTALL_DIR}/admin-panel
EOF
chmod 600 "${INSTALL_DIR}/server/.env"

echo "-- Budowanie (cargo build --release, moze chwile potrwac) --"
(cd "${INSTALL_DIR}/server" && cargo build --release)

echo "-- systemd unit --"
cat > /etc/systemd/system/pabianice-os.service <<EOF
[Unit]
Description=Pabianice OS - serwer kanalow
After=network.target postgresql.service

[Service]
Type=simple
WorkingDirectory=${INSTALL_DIR}/server
EnvironmentFile=${INSTALL_DIR}/server/.env
ExecStart=${INSTALL_DIR}/server/target/release/pabianice-os-server
Restart=on-failure
User=root

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now pabianice-os

echo
echo "== Gotowe =="
echo "Panel: http://$(hostname -I | awk '{print $1}'):${BIND_PORT}/"
echo "Zaloguj sie jako '${ADMIN_USER}' haslem ktore podales wyzej."
echo "Status: systemctl status pabianice-os"
