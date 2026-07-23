// Panel admina Pabianice OS - zero zaleznosci, zero build stepu, jeden plik.
// Rozmyslnie prosciej niz React z rozdz. 10.2/10.4.4 planu - ta sama filozofia
// co reszta repo (website/), patrz pabianice-os/README.md.

const state = {
    token: localStorage.getItem("pos_token") || null,
    role: localStorage.getItem("pos_role") || null,
    username: localStorage.getItem("pos_username") || null,
    channels: [],
    categories: [],
    activeChannel: null,
};

async function api(path, options = {}) {
    const headers = options.headers || {};
    if (state.token) {
        headers["Authorization"] = `Bearer ${state.token}`;
    }
    if (options.body) {
        headers["Content-Type"] = "application/json";
    }
    const res = await fetch(path, { ...options, headers });
    if (res.status === 401) {
        logout();
        throw new Error("sesja wygasla");
    }
    if (!res.ok) {
        const body = await res.json().catch(() => ({}));
        throw new Error(body.error || `blad ${res.status}`);
    }
    if (res.status === 204 || res.headers.get("content-length") === "0") {
        return null;
    }
    return res.json();
}

function logout() {
    // token lokalny czyscimy od razu (UI ma zareagowac natychmiast); usuniecie
    // sesji po stronie serwera jest best-effort w tle, poza wspoldzielonym api()
    // helperem - api() sam wola logout() na 401, wiec zapetlilby sie w kolko
    const token = state.token;
    state.token = null;
    state.role = null;
    state.username = null;
    localStorage.removeItem("pos_token");
    localStorage.removeItem("pos_role");
    localStorage.removeItem("pos_username");
    document.getElementById("app-screen").hidden = true;
    document.getElementById("login-screen").hidden = false;

    if (token) {
        fetch("/v1/auth/logout", {
            method: "POST",
            headers: { Authorization: `Bearer ${token}` },
        }).catch(() => {});
    }
}

function roleAtLeast(min) {
    const order = { member: 0, moderator: 1, admin: 2 };
    return order[state.role] >= order[min];
}

document.getElementById("login-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const username = document.getElementById("login-username").value;
    const password = document.getElementById("login-password").value;
    const errEl = document.getElementById("login-error");
    errEl.textContent = "";
    try {
        const resp = await api("/v1/auth/login", {
            method: "POST",
            body: JSON.stringify({ username, password }),
        });
        state.token = resp.token;
        state.role = resp.role;
        state.username = username;
        localStorage.setItem("pos_token", resp.token);
        localStorage.setItem("pos_role", resp.role);
        localStorage.setItem("pos_username", username);
        enterApp();
    } catch (err) {
        errEl.textContent = "Nie udało się zalogować.";
    }
});

document.getElementById("logout-btn").addEventListener("click", logout);

async function enterApp() {
    document.getElementById("login-screen").hidden = true;
    document.getElementById("app-screen").hidden = false;
    document.getElementById("whoami").textContent = `${state.username} (${state.role})`;
    document.getElementById("new-channel-btn").hidden = !roleAtLeast("admin");
    document.getElementById("new-category-btn").hidden = !roleAtLeast("admin");
    document.getElementById("show-users-btn").hidden = !roleAtLeast("admin");
    document.getElementById("show-settings-btn").hidden = !roleAtLeast("moderator");
    await loadChannels();
}

async function loadChannels() {
    state.categories = await api("/v1/categories");
    state.channels = await api("/v1/channels");
    renderChannelList();
}

function renderChannelList() {
    const list = document.getElementById("channel-list");
    list.innerHTML = "";

    const byCategory = new Map();
    for (const ch of state.channels) {
        const key = ch.category_id || "_none";
        if (!byCategory.has(key)) byCategory.set(key, []);
        byCategory.get(key).push(ch);
    }

    const renderGroup = (label, channels) => {
        if (label) {
            const heading = document.createElement("li");
            heading.className = "cat-label";
            heading.textContent = label;
            list.appendChild(heading);
        }
        for (const ch of channels) {
            const li = document.createElement("li");
            li.textContent = `# ${ch.name}`;
            if (state.activeChannel && state.activeChannel.id === ch.id) {
                li.classList.add("active");
            }
            li.addEventListener("click", () => openChannel(ch));
            list.appendChild(li);
        }
    };

    for (const cat of state.categories) {
        renderGroup(cat.name, byCategory.get(cat.id) || []);
    }
    renderGroup(null, byCategory.get("_none") || []);
}

async function openChannel(channel) {
    state.activeChannel = channel;
    renderChannelList();

    const main = document.getElementById("main-panel");
    main.innerHTML = `
        <div class="channel-header">
            <strong># ${channel.name}</strong>
            ${channel.topic ? `<div class="topic">${escapeHtml(channel.topic)}</div>` : ""}
        </div>
        <div class="message-list" id="message-list"></div>
        <form class="post-form" id="post-form">
            <textarea id="post-body" placeholder="napisz wiadomość..." required></textarea>
            <button type="submit">Wyślij</button>
        </form>
    `;

    document.getElementById("post-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        const body = document.getElementById("post-body");
        if (!body.value.trim()) return;
        await api(`/v1/channels/${channel.id}/messages`, {
            method: "POST",
            body: JSON.stringify({ body: body.value }),
        });
        body.value = "";
        await loadMessages(channel.id);
    });

    await loadMessages(channel.id);
}

async function loadMessages(channelId) {
    const messages = await api(`/v1/channels/${channelId}/messages`);
    const list = document.getElementById("message-list");
    list.innerHTML = "";
    for (const m of messages) {
        const row = document.createElement("div");
        row.className = "message-row";
        const ts = new Date(m.created_at).toLocaleString();
        row.innerHTML = `<span class="author">${escapeHtml(m.author)}</span><span class="ts">${ts}</span><div class="body">${escapeHtml(m.body)}</div>`;
        list.appendChild(row);
    }
    list.scrollTop = list.scrollHeight;
}

document.getElementById("new-channel-btn").addEventListener("click", async () => {
    const name = prompt("Nazwa kanału:");
    if (!name) return;

    let categoryId = null;
    if (state.categories.length > 0) {
        const names = state.categories.map((c) => c.name).join(", ");
        const choice = prompt(`Kategoria (Enter = bez kategorii). Istniejące: ${names}`);
        if (choice) {
            const match = state.categories.find((c) => c.name === choice);
            categoryId = match ? match.id : null;
        }
    }

    await api("/v1/channels", {
        method: "POST",
        body: JSON.stringify({ name, category_id: categoryId }),
    });
    await loadChannels();
});

document.getElementById("new-category-btn").addEventListener("click", async () => {
    const name = prompt("Nazwa kategorii:");
    if (!name) return;
    await api("/v1/categories", { method: "POST", body: JSON.stringify({ name }) });
    await loadChannels();
});

document.getElementById("show-users-btn").addEventListener("click", showUsers);

async function showUsers() {
    const users = await api("/v1/users");
    const main = document.getElementById("main-panel");
    main.innerHTML = `
        <h2>Użytkownicy</h2>
        <table>
            <thead><tr><th>Nazwa</th><th>Rola</th><th></th></tr></thead>
            <tbody>${users
                .map(
                    (u) => `<tr>
                        <td>${escapeHtml(u.username)}</td>
                        <td>${u.role}</td>
                        <td><button class="secondary reset-password-btn" data-user-id="${u.id}" data-username="${escapeHtml(u.username)}">Resetuj hasło</button></td>
                    </tr>`,
                )
                .join("")}</tbody>
        </table>
    `;
    for (const btn of main.querySelectorAll(".reset-password-btn")) {
        btn.addEventListener("click", async () => {
            const newPassword = prompt(`Nowe hasło dla ${btn.dataset.username} (min. 8 znaków):`);
            if (!newPassword) return;
            await api(`/v1/users/${btn.dataset.userId}/password`, {
                method: "PUT",
                body: JSON.stringify({ password: newPassword }),
            });
            alert("Hasło zmienione, poprzednie sesje tego użytkownika zostały zakończone.");
        });
    }
}

document.getElementById("show-settings-btn").addEventListener("click", async () => {
    const settings = await api("/v1/admin/settings");
    const main = document.getElementById("main-panel");
    main.innerHTML = `
        <h2>Ustawienia serwera</h2>
        <p class="hint">Federacja to na razie tylko zapisana decyzja operatora - sam protokół
        federacji między serwerami jeszcze nie jest zaimplementowany.</p>
        <label>Retencja wiadomości (dni, 0 = bez limitu)<br>
            <input type="number" id="retention-input" value="${settings.message_retention_days}">
        </label><br><br>
        <label><input type="checkbox" id="federation-input" ${settings.federation_enabled ? "checked" : ""}> Federacja włączona</label><br><br>
        <button id="save-settings-btn">Zapisz</button>
    `;
    document.getElementById("save-settings-btn").addEventListener("click", async () => {
        await api("/v1/admin/settings", {
            method: "PUT",
            body: JSON.stringify({
                message_retention_days: Number(document.getElementById("retention-input").value),
                federation_enabled: document.getElementById("federation-input").checked,
            }),
        });
    });
});

function escapeHtml(s) {
    const div = document.createElement("div");
    div.textContent = s;
    return div.innerHTML;
}

if (state.token) {
    enterApp().catch(logout);
}
