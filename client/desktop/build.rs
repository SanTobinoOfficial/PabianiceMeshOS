// Kompiluje na hosta dokladnie te same pliki C co firmware (crypto + cala biblioteke
// libsignal-protocol-c z third_party jako git submodule), plus host_shim zastepujacy
// waskie API ESP-IDF/FreeRTOS ktorego te pliki uzywaja. Zero duplikacji/reimplementacji
// kryptografii - patrz README.md w tym katalogu po pelne wyjasnienie.

use std::path::{Path, PathBuf};

fn main() {
    let manifest_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let repo_root = manifest_dir
        .parent() // client/
        .and_then(Path::parent) // repo root
        .expect("client/desktop powinno byc dwa poziomy pod korzeniem repo")
        .to_path_buf();

    let crypto_dir = repo_root.join("firmware/components/crypto");
    let crypto_include = crypto_dir.join("include");
    let signal_src = repo_root.join("firmware/third_party/libsignal-protocol-c/src");
    let curve25519_dir = signal_src.join("curve25519");
    let ed25519_dir = curve25519_dir.join("ed25519");
    let protobuf_c_dir = signal_src.join("protobuf-c");
    let shim_include = manifest_dir.join("csrc/host_shim/include");

    for dir in [&crypto_dir, &signal_src, &shim_include] {
        if !dir.exists() {
            panic!(
                "brakuje katalogu {} - zly checkout / submodule nie zainicjalizowany?",
                dir.display()
            );
        }
    }

    let mut build = cc::Build::new();
    build
        .warnings(false) // firmware/third_party nie jest nasze, nie czyscimy jego warningow
        .include(&shim_include)
        .include(&crypto_include)
        .include(&crypto_dir) // internal.h siedzi w korzeniu komponentu, nie w include/
        .include(&signal_src)
        .include(&curve25519_dir)
        .include(&ed25519_dir)
        .include(ed25519_dir.join("nacl_includes"))
        .include(ed25519_dir.join("additions"))
        .include(ed25519_dir.join("additions/generalized"))
        .include(&protobuf_c_dir);

    // host shim - implementacja "NVS"/RNG/timera/semafora dla powyzszych plikow
    build.file(manifest_dir.join("csrc/host_shim/shim.c"));

    // dokladnie te same zrodla co firmware/components/crypto/CMakeLists.txt
    build.file(crypto_dir.join("provider.c"));
    build.file(crypto_dir.join("store.c"));
    build.file(crypto_dir.join("pcrypto.c"));

    // libsignal-protocol-c - kanoniczna lista z jego wlasnego src/CMakeLists.txt,
    // src/curve25519/CMakeLists.txt i src/protobuf-c/CMakeLists.txt (nie z ESP-IDF-owego
    // component CMakeLists firmware, ktory kompiluje przez add_subdirectory a nie glob)
    let protobuf_srcs = [
        "LocalStorageProtocol.pb-c.c",
        "WhisperTextProtocol.pb-c.c",
        "FingerprintProtocol.pb-c.c",
    ];
    for f in protobuf_srcs {
        build.file(signal_src.join(f));
    }

    let signal_protocol_srcs = [
        "vpool.c",
        "signal_protocol.c",
        "curve.c",
        "hkdf.c",
        "ratchet.c",
        "protocol.c",
        "session_state.c",
        "session_record.c",
        "session_pre_key.c",
        "session_builder.c",
        "session_cipher.c",
        "key_helper.c",
        "sender_key.c",
        "sender_key_state.c",
        "sender_key_record.c",
        "group_session_builder.c",
        "group_cipher.c",
        "fingerprint.c",
        "device_consistency.c",
    ];
    for f in signal_protocol_srcs {
        build.file(signal_src.join(f));
    }

    build.file(curve25519_dir.join("curve25519-donna.c"));

    let ed25519_srcs = [
        "fe_0.c",
        "fe_1.c",
        "fe_add.c",
        "fe_cmov.c",
        "fe_copy.c",
        "fe_frombytes.c",
        "fe_invert.c",
        "fe_isnegative.c",
        "fe_isnonzero.c",
        "fe_mul.c",
        "fe_neg.c",
        "fe_pow22523.c",
        "fe_sq.c",
        "fe_sq2.c",
        "fe_sub.c",
        "fe_tobytes.c",
        "ge_add.c",
        "ge_double_scalarmult.c",
        "ge_frombytes.c",
        "ge_madd.c",
        "ge_msub.c",
        "ge_p1p1_to_p2.c",
        "ge_p1p1_to_p3.c",
        "ge_p2_0.c",
        "ge_p2_dbl.c",
        "ge_p3_0.c",
        "ge_p3_dbl.c",
        "ge_p3_to_cached.c",
        "ge_p3_to_p2.c",
        "ge_p3_tobytes.c",
        "ge_precomp_0.c",
        "ge_scalarmult_base.c",
        "ge_sub.c",
        "ge_tobytes.c",
        "open.c",
        "sc_muladd.c",
        "sc_reduce.c",
        "sign.c",
        "additions/compare.c",
        "additions/curve_sigs.c",
        "additions/elligator.c",
        "additions/fe_isequal.c",
        "additions/fe_isreduced.c",
        "additions/fe_mont_rhs.c",
        "additions/fe_montx_to_edy.c",
        "additions/fe_sqrt.c",
        "additions/ge_isneutral.c",
        "additions/ge_montx_to_p3.c",
        "additions/ge_neg.c",
        "additions/ge_p3_to_montx.c",
        "additions/ge_scalarmult.c",
        "additions/ge_scalarmult_cofactor.c",
        "additions/keygen.c",
        "additions/open_modified.c",
        "additions/sc_clamp.c",
        "additions/sc_cmov.c",
        "additions/sc_neg.c",
        "additions/sign_modified.c",
        "additions/utility.c",
        "additions/generalized/ge_p3_add.c",
        "additions/generalized/gen_eddsa.c",
        "additions/generalized/gen_labelset.c",
        "additions/generalized/gen_veddsa.c",
        "additions/generalized/gen_x.c",
        "additions/generalized/point_isreduced.c",
        "additions/generalized/sc_isreduced.c",
        "additions/xeddsa.c",
        "additions/zeroize.c",
        "nacl_sha512/blocks.c",
        "nacl_sha512/hash.c",
        "tests/internal_fast_tests.c",
    ];
    for f in ed25519_srcs {
        build.file(ed25519_dir.join(f));
    }

    build.file(protobuf_c_dir.join("protobuf-c.c"));

    build.compile("pcrypto_host");

    println!("cargo:rustc-link-lib=mbedcrypto");
    println!("cargo:rerun-if-changed={}", crypto_dir.display());
    println!("cargo:rerun-if-changed={}", signal_src.display());
    println!(
        "cargo:rerun-if-changed={}",
        manifest_dir.join("csrc").display()
    );
}
