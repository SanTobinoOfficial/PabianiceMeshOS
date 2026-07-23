// Klient Web Bluetooth - rozmawia z jednym wezlem mesh po BLE (patrz
// firmware/components/ble/README.md po format ramek i UUID). Zero zaleznosci,
// zwykly JS w przegladarce - zadnego builda, tak jak reszta panelu pabianice-os.

const SERVICE_UUID = '2b195cc7-9a84-4b1a-8f6e-112233440001';
const CHR_RX_UUID = '2b195cc7-9a84-4b1a-8f6e-112233440002'; // telefon -> wezel
const CHR_TX_UUID = '2b195cc7-9a84-4b1a-8f6e-112233440003'; // wezel -> telefon
const NODE_ID_LEN = 8;

const connectBtn = document.getElementById('connect-btn');
const sendBtn = document.getElementById('send-btn');
const statusEl = document.getElementById('status');
const appPanel = document.getElementById('app-panel');
const dstInput = document.getElementById('dst-id');
const msgInput = document.getElementById('msg');
const logEl = document.getElementById('log');
const unsupportedEl = document.getElementById('unsupported');

let rxChar = null;
let device = null;

function setStatus(text, cls) {
  statusEl.textContent = text;
  statusEl.className = `status status-${cls}`;
}

function hexToBytes(hex) {
  if (!/^[0-9a-fA-F]{16}$/.test(hex)) {
    throw new Error('ID musi miec dokladnie 16 znakow hex (8 bajtow)');
  }
  const bytes = new Uint8Array(NODE_ID_LEN);
  for (let i = 0; i < NODE_ID_LEN; i++) {
    bytes[i] = parseInt(hex.substr(i * 2, 2), 16);
  }
  return bytes;
}

function bytesToHex(bytes) {
  return Array.from(bytes).map((b) => b.toString(16).padStart(2, '0')).join('');
}

function appendLog(srcHex, text) {
  const li = document.createElement('li');
  const time = new Date().toLocaleTimeString('pl-PL');
  li.innerHTML = `<span class="from">${srcHex}</span><span class="time">${time}</span><br>${text}`;
  logEl.prepend(li);
}

function onNotify(event) {
  const value = new Uint8Array(event.target.value.buffer);
  if (value.length <= NODE_ID_LEN) {
    return; // pusta ramka, ignorujemy
  }
  const srcId = value.slice(0, NODE_ID_LEN);
  const text = new TextDecoder().decode(value.slice(NODE_ID_LEN));
  appendLog(bytesToHex(srcId), text);
}

function onDisconnected() {
  rxChar = null;
  device = null;
  appPanel.hidden = true;
  setStatus('rozłączony', 'disconnected');
  connectBtn.disabled = false;
  connectBtn.textContent = 'Połącz z węzłem';
}

async function connect() {
  connectBtn.disabled = true;
  setStatus('łączę...', 'connecting');
  try {
    device = await navigator.bluetooth.requestDevice({
      filters: [{ name: 'pabianice-node' }],
      optionalServices: [SERVICE_UUID],
    });
    device.addEventListener('gattserverdisconnected', onDisconnected);

    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    rxChar = await service.getCharacteristic(CHR_RX_UUID);
    const txChar = await service.getCharacteristic(CHR_TX_UUID);

    await txChar.startNotifications();
    txChar.addEventListener('characteristicvaluechanged', onNotify);

    setStatus(`połączony (${device.name})`, 'connected');
    appPanel.hidden = false;
    connectBtn.textContent = 'Rozłącz';
    connectBtn.disabled = false;
  } catch (err) {
    console.error(err);
    setStatus('błąd połączenia', 'disconnected');
    connectBtn.disabled = false;
    alert(`Nie udało się połączyć: ${err.message}`);
  }
}

async function send() {
  if (!rxChar) {
    return;
  }
  let dstBytes;
  try {
    dstBytes = hexToBytes(dstInput.value.trim());
  } catch (err) {
    alert(err.message);
    return;
  }
  const text = msgInput.value;
  if (!text) {
    return;
  }

  const textBytes = new TextEncoder().encode(text);
  const frame = new Uint8Array(NODE_ID_LEN + textBytes.length);
  frame.set(dstBytes, 0);
  frame.set(textBytes, NODE_ID_LEN);

  sendBtn.disabled = true;
  try {
    if (rxChar.writeValueWithResponse) {
      await rxChar.writeValueWithResponse(frame);
    } else {
      await rxChar.writeValue(frame);
    }
    msgInput.value = '';
  } catch (err) {
    console.error(err);
    alert(`Wysyłka nie wyszła: ${err.message}`);
  } finally {
    sendBtn.disabled = false;
  }
}

connectBtn.addEventListener('click', () => {
  if (device && device.gatt.connected) {
    device.gatt.disconnect();
    return;
  }
  connect();
});

sendBtn.addEventListener('click', send);

if (!navigator.bluetooth) {
  connectBtn.disabled = true;
  unsupportedEl.hidden = false;
}
