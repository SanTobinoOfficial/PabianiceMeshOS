// Desktopowy klient Pabianice Comms - laczy sie z /server przez zwykly internet/ethernet
// (HTTP), tym samym Signal Protocol (X3DH + Double Ratchet) co firmware, bo uzywa
// dokladnie tych samych plikow C (patrz build.rs + README.md w tym katalogu). Osobna
// sprawa to telefon bez internetu - to idzie przez BLE bezposrednio do wezla mesh, nie
// przez ten klient (patrz firmware/components/ble + client/web-ble).

mod api;
mod ffi;
mod pcrypto;

use anyhow::Result;
use clap::{Parser, Subcommand};
use std::path::PathBuf;

use api::Api;
use pcrypto::LocalIdentity;

#[derive(Parser)]
#[command(name = "pabianice", about = "Desktopowy klient sieci Pabianice Comms")]
struct Cli {
    /// Katalog na tozsamosc (klucze, sesje) - jeden na osobe/instalacje, nigdy nie kopiuj
    /// miedzy maszynami bo to zepsuje stan Double Ratchet
    #[arg(long, global = true, default_value = ".pcrypto")]
    data_dir: PathBuf,

    /// Adres /server
    #[arg(long, global = true, default_value = "http://127.0.0.1:8080")]
    server: String,

    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Generuje (jesli trzeba) tozsamosc i wlasny node_id, wypisuje node_id
    Init,
    /// Publikuje komplet kluczy publicznych na serwerze pod wlasnym node_id
    Publish,
    /// Wysyla wiadomosc do peera (pobiera jego bundle z serwera jesli brak sesji)
    Send {
        #[arg(long)]
        to: String,
        message: String,
    },
    /// Odbiera i odszyfrowuje czekajace wiadomosci, opcjonalnie potwierdza (kasuje z serwera)
    Poll {
        #[arg(long)]
        ack: bool,
    },
    /// Jak `poll`, ale w petli co --interval sekund, az do Ctrl-C - zamiast recznie
    /// odpalac `poll` w kolko. To zwykle odpytywanie HTTP, nie push - serwer nie
    /// wystawia zadnego kanalu powiadomien na zewnatrz (Redis pub/sub uzywany
    /// wewnetrznie przez /server nie jest publicznym API)
    Listen {
        #[arg(long, default_value_t = 5)]
        interval: u64,
        #[arg(long)]
        ack: bool,
    },
}

fn own_node_id(data_dir: &std::path::Path) -> Result<[u8; 8]> {
    let path = data_dir.join("node_id");
    if let Ok(hex_str) = std::fs::read_to_string(&path) {
        let bytes = hex::decode(hex_str.trim())?;
        return bytes
            .try_into()
            .map_err(|_| anyhow::anyhow!("zly node_id w {}", path.display()));
    }

    std::fs::create_dir_all(data_dir)?;
    let mut id = [0u8; 8];
    {
        use std::io::Read;
        let mut f = std::fs::File::open("/dev/urandom")?;
        f.read_exact(&mut id)?;
    }
    std::fs::write(&path, hex::encode(id))?;
    Ok(id)
}

fn parse_peer_id(hex_str: &str) -> Result<[u8; 8]> {
    let bytes = hex::decode(hex_str)?;
    bytes
        .try_into()
        .map_err(|_| anyhow::anyhow!("peer id musi miec 16 znakow hex (8 bajtow)"))
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    let identity = LocalIdentity::init(&cli.data_dir)?;
    let own_id = own_node_id(&cli.data_dir)?;
    let own_id_hex = hex::encode(own_id);

    match cli.command {
        Command::Init => {
            println!("wlasny node_id: {own_id_hex}");
            println!("dane w: {}", cli.data_dir.display());
        }

        Command::Publish => {
            let bundle = identity.local_bundle()?;
            let api = Api::new(cli.server.clone());
            api.publish_bundle(&own_id_hex, &bundle)?;
            println!("opublikowano bundle dla {own_id_hex} na {}", cli.server);
        }

        Command::Send { to, message } => {
            let peer_id = parse_peer_id(&to)?;
            let api = Api::new(cli.server.clone());

            if !identity.has_session(&peer_id) {
                println!("brak sesji z {to}, pobieram jego bundle z serwera...");
                let peer_bundle = api.fetch_bundle(&to)?;
                if peer_bundle.pre_key_pub.is_empty() {
                    println!(
                        "uwaga: {to} nie ma juz opublikowanego one-time prekey - \
                         sesja X3DH bez OPK (slabszy forward secrecy pierwszej wiadomosci)"
                    );
                }
                identity.process_peer_bundle(&peer_id, &peer_bundle)?;
                println!("sesja X3DH ustanowiona");
            }

            let ciphertext = identity.encrypt(&peer_id, message.as_bytes())?;
            let id = api.submit_message(&to, &own_id_hex, &ciphertext)?;
            println!("wyslano (id={id})");
        }

        Command::Poll { ack } => {
            let api = Api::new(cli.server.clone());
            let found = poll_once(&api, &identity, &own_id_hex, ack)?;
            if !found {
                println!("brak nowych wiadomosci");
            }
        }

        Command::Listen { interval, ack } => {
            let api = Api::new(cli.server.clone());
            println!("nasluchuje jako {own_id_hex} (co {interval}s, Ctrl-C zeby przerwac)...");
            loop {
                if let Err(e) = poll_once(&api, &identity, &own_id_hex, ack) {
                    eprintln!("blad przy odpytywaniu serwera, probuje dalej: {e}");
                }
                std::thread::sleep(std::time::Duration::from_secs(interval));
            }
        }
    }

    Ok(())
}

// Wspolna sciezka dla `poll` i `listen`. Zwraca true jesli byla przynajmniej jedna
// wiadomosc (do decydowania czy `poll` ma wypisac "brak nowych wiadomosci" - `listen`
// samo w sobie tego nie potrzebuje, milczy w cichych cyklach zeby nie zasmiecac terminala)
fn poll_once(api: &Api, identity: &LocalIdentity, own_id_hex: &str, ack: bool) -> Result<bool> {
    let pending = api.poll_messages(own_id_hex)?;
    let found = !pending.is_empty();
    for msg in pending {
        let src_id = parse_peer_id(&msg.src_id)?;
        let ciphertext = hex::decode(&msg.ciphertext)?;
        match identity.decrypt(&src_id, &ciphertext) {
            Ok(plaintext) => {
                let text = String::from_utf8_lossy(&plaintext);
                println!("[{}] od {}: {}", msg.created_at, msg.src_id, text);
                if ack {
                    api.ack_message(own_id_hex, &msg.id)?;
                }
            }
            Err(e) => {
                eprintln!(
                    "nie udalo sie odszyfrowac wiadomosci {} od {}: {e}",
                    msg.id, msg.src_id
                );
            }
        }
    }
    Ok(found)
}
