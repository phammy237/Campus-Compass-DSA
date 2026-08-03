const map = L.map('map', { zoomControl: false }).setView([29.6465, -82.3440], 15);
L.control.zoom({ position: 'bottomright' }).addTo(map);

// CartoDB Positron: a minimal light basemap (fewer labels, muted colors) for
// a cleaner look than default OSM raster tiles
L.tileLayer('https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png', {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
}).addTo(map);

const locations = {}; // id -> {id, name, lat, lng}
const classes = {};   // code -> {code, locationId, start, end, startMinutes, endMinutes}
const markers = {};   // id -> Leaflet marker

// ids below 1000 are the assignment's graded locations (data/edges.csv /
// data/classes.csv, backed by CampusCompass's Dijkstra/MST/etc). ids 1000+
// are the expanded synthetic campus (data/synthetic_edges.csv /
// data/course_sections.csv, backed by ScheduleBuilder) used only by the
// Schedule Builder tab — Explore/Students only work with the graded set.
const GRADED_ID_THRESHOLD = 1000;
const isGraded = id => Number(id) < GRADED_ID_THRESHOLD;

let highlightLayer = null;   // Explore tab: shortest-path polyline
let zoneMarkersLayer = null; // Students tab: highlighted zone markers
let scheduleRouteLayer = null; // Schedule Builder tab: recommended route polyline
let lastRecommendation = null; // cached response from the last /recommend call

function escapeHtml(s) {
    const div = document.createElement('div');
    div.textContent = s;
    return div.innerHTML;
}

function locationName(id) {
    return locations[id] ? locations[id].name : `#${id}`;
}

async function fetchJSON(url, opts) {
    const res = await fetch(url, opts);
    if (!res.ok) {
        const body = await res.json().catch(() => ({}));
        throw new Error(body.error || `Request failed (${res.status})`);
    }
    return res.json();
}

function postForm(url, params) {
    return fetchJSON(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams(params).toString(),
    });
}

function del(url) {
    return fetchJSON(url, { method: 'DELETE' });
}

/* ---------- searchable select (combobox) ---------- */

// Replaces a plain <select> with a type-to-filter text input + dropdown list,
// for pickers with too many options (250+ buildings) to scan by eye.
// Mirrors a <select>'s .value getter/setter so call sites barely change.
function createSearchableSelect(container) {
    container.innerHTML = '';
    const input = document.createElement('input');
    input.type = 'text';
    input.autocomplete = 'off';
    input.placeholder = 'Search…';
    const optionsEl = document.createElement('div');
    optionsEl.className = 'ss-options';
    container.appendChild(input);
    container.appendChild(optionsEl);

    let allOptions = []; // [{id, label}]
    let committedId = '';
    let committedLabel = '';
    let changeHandler = null;

    function commit(id, label, fromUser) {
        committedId = id;
        committedLabel = label;
        input.value = label;
        if (fromUser && changeHandler) changeHandler(id);
    }

    function render(filterText) {
        const q = (filterText || '').trim().toLowerCase();
        const matches = q ? allOptions.filter(o => o.label.toLowerCase().includes(q)) : allOptions;
        const capped = matches.slice(0, 50);
        optionsEl.innerHTML = '';
        if (!capped.length) {
            const empty = document.createElement('div');
            empty.className = 'ss-empty';
            empty.textContent = 'No matches';
            optionsEl.appendChild(empty);
            return;
        }
        capped.forEach(o => {
            const item = document.createElement('div');
            item.className = 'ss-option';
            item.textContent = o.label;
            item.addEventListener('mousedown', ev => {
                ev.preventDefault(); // fires before blur, so the pick registers before the list closes
                commit(String(o.id), o.label, true);
                close();
            });
            optionsEl.appendChild(item);
        });
    }

    function open() {
        container.classList.add('open');
        render(input.value);
    }
    function close() {
        container.classList.remove('open');
    }

    input.addEventListener('focus', () => { input.select(); open(); });
    input.addEventListener('input', () => open());
    input.addEventListener('blur', () => {
        setTimeout(() => {
            input.value = committedLabel; // revert to the last confirmed pick if nothing new was chosen
            close();
        }, 150);
    });
    input.addEventListener('keydown', ev => {
        if (ev.key === 'Enter') {
            const first = optionsEl.querySelector('.ss-option');
            if (first) first.dispatchEvent(new MouseEvent('mousedown'));
            ev.preventDefault();
        } else if (ev.key === 'Escape') {
            input.blur();
        }
    });

    return {
        setOptions(options) {
            allOptions = options.map(o => ({ id: o.id, label: o.label }));
        },
        get value() {
            return committedId;
        },
        set value(id) {
            const match = allOptions.find(o => String(o.id) === String(id));
            if (match) commit(String(match.id), match.label);
            else commit('', '');
        },
        onChange(fn) {
            changeHandler = fn;
        },
    };
}

const fromSelect = createSearchableSelect(document.getElementById('from-select'));
const toSelect = createSearchableSelect(document.getElementById('to-select'));
const residenceSelect = createSearchableSelect(document.getElementById('student-residence'));
const homeSelect = createSearchableSelect(document.getElementById('schedule-home'));
const finderSelect = createSearchableSelect(document.getElementById('finder-select'));

finderSelect.onChange(id => {
    const loc = locations[id];
    if (!loc) return;
    map.setView([loc.lat, loc.lng], 18, { animate: true });
    const marker = markers[id];
    if (marker) marker.openPopup();
    finderSelect.value = ''; // ready for the next search without showing a stale committed pick
});

const courseFinderSelect = createSearchableSelect(document.getElementById('course-finder-select'));
const courseCatalog = {}; // code -> {code, sections: [{sectionId, locationId, locationName, start, end}]}
let courseFinderLayer = null;

function clearCourseFinder() {
    if (courseFinderLayer) {
        map.removeLayer(courseFinderLayer);
        courseFinderLayer = null;
    }
    document.getElementById('course-finder-result').innerHTML = '';
}

courseFinderSelect.onChange(code => {
    clearCourseFinder();
    const course = courseCatalog[code];
    if (!course || !course.sections.length) return;

    const resultEl = document.getElementById('course-finder-result');
    resultEl.innerHTML = course.sections.map(s => `
        <div class="class-row" data-location-id="${s.locationId}">
            <span>${escapeHtml(s.locationName)}</span>
            <span class="time">${s.start}-${s.end}</span>
        </div>
    `).join('');
    resultEl.querySelectorAll('.class-row').forEach(row => {
        row.addEventListener('click', () => {
            const loc = locations[row.dataset.locationId];
            if (!loc) return;
            map.setView([loc.lat, loc.lng], 18, { animate: true });
            const marker = markers[loc.id];
            if (marker) marker.openPopup();
        });
    });

    const bounds = [];
    const markerList = course.sections
        .map(s => locations[s.locationId])
        .filter(Boolean)
        .map(loc => {
            bounds.push([loc.lat, loc.lng]);
            return L.circleMarker([loc.lat, loc.lng], {
                radius: 9,
                color: '#fa4616',
                fillColor: '#fa4616',
                fillOpacity: 0.6,
                weight: 2,
            }).bindPopup(`<strong>${escapeHtml(course.code)}</strong> @ ${escapeHtml(loc.name)}`);
        });
    if (markerList.length) {
        courseFinderLayer = L.layerGroup(markerList).addTo(map);
        map.fitBounds(bounds, { padding: [60, 60] });
    }
});

/* ---------- real walking routes (OSRM foot routing, public demo server) ---------- */

const walkingRouteCache = new Map();

async function fetchWalkingLeg(a, b) {
    const key = `${a[0].toFixed(6)},${a[1].toFixed(6)}|${b[0].toFixed(6)},${b[1].toFixed(6)}`;
    if (walkingRouteCache.has(key)) return walkingRouteCache.get(key);
    try {
        const url = `https://router.project-osrm.org/route/v1/foot/${a[1]},${a[0]};${b[1]},${b[0]}?overview=full&geometries=geojson`;
        const res = await fetch(url);
        if (!res.ok) throw new Error('routing request failed');
        const data = await res.json();
        if (data.code !== 'Ok' || !data.routes || !data.routes.length) throw new Error('no route found');
        const points = data.routes[0].geometry.coordinates.map(([lon, lat]) => [lat, lon]);
        walkingRouteCache.set(key, points);
        return points;
    } catch (err) {
        const fallback = [a, b]; // OSRM unreachable/no route: fall back to a straight segment for this leg only
        walkingRouteCache.set(key, fallback);
        return fallback;
    }
}

// The graded graph's edge weights are the assignment's own arbitrary data, not
// real walking times, so its "shortest" path can hop through buildings that
// aren't on the geographically direct way. The stop list still shows every
// hop (accurate to what Dijkstra actually computed); for the drawn line we
// only care about true start/end, so a single direct route reads cleanly.
function routeEndpoints(coordinates) {
    if (coordinates.length < 2) return coordinates;
    return [coordinates[0], coordinates[coordinates.length - 1]];
}

// stitches real-road walking geometry between each consecutive pair of waypoints
async function fetchWalkingRoute(waypoints) {
    if (waypoints.length < 2) return waypoints;
    const legs = await Promise.all(
        waypoints.slice(0, -1).map((point, i) => fetchWalkingLeg(point, waypoints[i + 1]))
    );

    const full = [];
    legs.forEach(segment => {
        if (full.length && segment.length &&
            full[full.length - 1][0] === segment[0][0] && full[full.length - 1][1] === segment[0][1]) {
            full.push(...segment.slice(1)); // drop the duplicate joint point between legs
        } else {
            full.push(...segment);
        }
    });
    return full;
}

/* ---------- tabs ---------- */

document.querySelectorAll('.tab').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.tab').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
    });
});

/* ---------- map / campus data (Explore tab) ---------- */

function populateSelects(locs) {
    // Explore/Students only operate on the graded location set
    const sorted = [...locs].filter(l => isGraded(l.id)).sort((a, b) => a.name.localeCompare(b.name));
    const options = sorted.map(loc => ({ id: loc.id, label: loc.name }));

    fromSelect.setOptions(options);
    toSelect.setOptions(options);
    residenceSelect.setOptions(options);

    if (sorted.length > 1) {
        fromSelect.value = sorted[0].id;
        toSelect.value = sorted[1].id;
    }
}

function populateScheduleHome(locs) {
    const sorted = [...locs].sort((a, b) => a.name.localeCompare(b.name));
    homeSelect.setOptions(sorted.map(loc => ({ id: loc.id, label: loc.name })));
    if (sorted.length) homeSelect.value = sorted[0].id;
}

function populateFinder(locs) {
    const sorted = [...locs].sort((a, b) => a.name.localeCompare(b.name));
    finderSelect.setOptions(sorted.map(loc => ({ id: loc.id, label: loc.name })));
}

function onMarkerClick(id) {
    fromSelect.value = id;
}

function buildPopupContent(loc, graded) {
    const div = document.createElement('div');

    const title = document.createElement('strong');
    title.textContent = loc.name;
    div.appendChild(title);

    const meta = document.createElement('div');
    meta.className = 'popup-meta';
    meta.textContent = `${loc.lat.toFixed(5)}, ${loc.lng.toFixed(5)}` + (graded ? '' : ' · not in the graded campus graph');
    div.appendChild(meta);

    if (graded) {
        const actions = document.createElement('div');
        actions.className = 'popup-actions';

        const setFrom = document.createElement('button');
        setFrom.type = 'button';
        setFrom.textContent = 'Set as From';
        setFrom.addEventListener('click', () => { fromSelect.value = loc.id; });

        const setTo = document.createElement('button');
        setTo.type = 'button';
        setTo.textContent = 'Set as To';
        setTo.addEventListener('click', () => { toSelect.value = loc.id; });

        actions.appendChild(setFrom);
        actions.appendChild(setTo);
        div.appendChild(actions);
    }

    return div;
}

async function loadCampus() {
    const [locs, edges] = await Promise.all([
        fetchJSON('/api/locations'),
        fetchJSON('/api/edges'),
    ]);

    locs.forEach(loc => { locations[loc.id] = loc; });

    const bounds = [];
    locs.forEach(loc => {
        bounds.push([loc.lat, loc.lng]);
        const graded = isGraded(loc.id);
        // towers are campus landmarks — call them out with the same blue
        // treatment as graded buildings even when they aren't routable
        const landmark = graded || /\btower/i.test(loc.name);
        const marker = L.circleMarker([loc.lat, loc.lng], {
            radius: landmark ? 6 : 3,
            color: landmark ? '#0021a5' : '#8a8a8a',
            fillColor: landmark ? '#0021a5' : '#8a8a8a',
            fillOpacity: landmark ? 0.85 : 0.5,
            weight: 1,
        }).addTo(map);
        marker.bindPopup(buildPopupContent(loc, graded));
        if (graded) marker.on('click', () => onMarkerClick(loc.id));
        markers[loc.id] = marker;
    });
    if (bounds.length) map.fitBounds(bounds, { padding: [40, 40] });

    edges.forEach(e => {
        const from = locations[e.from];
        const to = locations[e.to];
        if (!from || !to) return;
        L.polyline([[from.lat, from.lng], [to.lat, to.lng]], {
            color: e.open ? '#999' : '#c0392b',
            weight: 2,
            opacity: e.open ? 0.6 : 0.8,
            dashArray: e.open ? null : '6,6',
        }).addTo(map);
    });

    populateSelects(locs);
    populateScheduleHome(locs);
    populateFinder(locs);
}

async function findPath() {
    const fromId = fromSelect.value;
    const toId = toSelect.value;
    const statusEl = document.getElementById('explore-status');
    const resultEl = document.getElementById('explore-result');
    const btn = document.getElementById('find-path-btn');

    statusEl.textContent = '';
    statusEl.classList.remove('error');
    resultEl.classList.add('hidden');
    if (highlightLayer) {
        map.removeLayer(highlightLayer);
        highlightLayer = null;
    }

    if (!fromId || !toId) return;
    if (fromId === toId) {
        statusEl.textContent = 'Pick two different locations.';
        statusEl.classList.add('error');
        return;
    }

    btn.disabled = true;
    statusEl.textContent = 'Finding route…';
    try {
        const data = await fetchJSON(`/api/shortest-path?from=${fromId}&to=${toId}`);
        if (!data.path.length || data.totalTime < 0) {
            statusEl.textContent = 'No open path between these locations.';
            statusEl.classList.add('error');
            return;
        }
        statusEl.textContent = 'Drawing route…';

        const routeLine = await fetchWalkingRoute(routeEndpoints(data.coordinates));
        highlightLayer = L.polyline(routeLine, {
            color: '#fa4616',
            weight: 5,
            opacity: 0.95,
        }).addTo(map);
        map.fitBounds(highlightLayer.getBounds(), { padding: [60, 60] });

        statusEl.textContent = '';
        document.getElementById('result-time').textContent = data.totalTime;
        const stepsEl = document.getElementById('result-steps');
        stepsEl.innerHTML = '';
        data.path.forEach(id => {
            const li = document.createElement('li');
            li.textContent = locationName(id);
            stepsEl.appendChild(li);
        });
        resultEl.classList.remove('hidden');
    } catch (err) {
        statusEl.textContent = err.message;
        statusEl.classList.add('error');
    } finally {
        btn.disabled = false;
    }
}

document.getElementById('find-path-btn').addEventListener('click', findPath);

/* ---------- students tab ---------- */

async function loadConnectivity() {
    const badge = document.getElementById('connectivity-badge');
    try {
        const data = await fetchJSON('/api/isConnected');
        badge.textContent = data.connected ? 'Campus fully connected' : 'Campus has disconnected pockets';
        badge.className = 'badge ' + (data.connected ? 'ok' : 'warn');
    } catch (err) {
        badge.textContent = 'Connectivity check failed';
        badge.className = 'badge warn';
    }
}

async function loadClassCatalog() {
    const list = await fetchJSON('/api/classes');
    list.forEach(c => { classes[c.code] = c; });
    const container = document.getElementById('student-classes');
    container.innerHTML = '';
    [...list].sort((a, b) => a.code.localeCompare(b.code)).forEach(c => {
        const label = document.createElement('label');
        const input = document.createElement('input');
        input.type = 'checkbox';
        input.value = c.code;
        input.name = 'class-code';
        label.appendChild(input);
        label.appendChild(document.createTextNode(
            `${c.code} — ${locationName(c.locationId)} (${c.start}-${c.end})`
        ));
        container.appendChild(label);
    });
}

async function loadStudentSelect(selectUfid) {
    const select = document.getElementById('student-select');
    const students = await fetchJSON('/api/students');
    select.innerHTML = '<option value="">— select a student —</option>';
    [...students].sort((a, b) => a.name.localeCompare(b.name)).forEach(s => {
        select.appendChild(new Option(`${s.name} (${s.ufid})`, s.ufid));
    });
    if (selectUfid) {
        select.value = selectUfid;
        renderStudentDetails(selectUfid);
    }
}

function clearZoneHighlight() {
    if (zoneMarkersLayer) {
        map.removeLayer(zoneMarkersLayer);
        zoneMarkersLayer = null;
    }
}

document.getElementById('student-select').addEventListener('change', e => {
    clearZoneHighlight();
    const ufid = e.target.value;
    if (!ufid) {
        document.getElementById('student-details').innerHTML = '';
        return;
    }
    renderStudentDetails(ufid);
});

async function renderStudentDetails(ufid) {
    const detailsEl = document.getElementById('student-details');
    detailsEl.innerHTML = '<p class="status">Loading…</p>';

    try {
        const [info, edges] = await Promise.all([
            fetchJSON(`/api/students/${ufid}`),
            fetchJSON(`/api/students/${ufid}/shortest-edges`),
        ]);

        const card = document.createElement('div');
        card.className = 'student-card';

        const residence = locations[info.residenceId];
        card.innerHTML = `
            <h3>${escapeHtml(info.name)}</h3>
            <div class="meta">UFID ${escapeHtml(info.ufid)} &middot; lives at ${escapeHtml(residence ? residence.name : '#' + info.residenceId)}</div>
        `;

        const classList = document.createElement('div');
        edges.edges.forEach(e => {
            const row = document.createElement('div');
            row.className = 'class-row';
            const reachable = e.time >= 0;
            row.innerHTML = `
                <span>${escapeHtml(e.code)}</span>
                <span class="time ${reachable ? '' : 'unreachable'}">${reachable ? e.time + ' min' : 'unreachable'}</span>
            `;
            if (reachable && info.residenceId != null && e.locationId >= 0) {
                row.addEventListener('click', async () => {
                    const path = await fetchJSON(`/api/shortest-path?from=${info.residenceId}&to=${e.locationId}`);
                    const routeLine = await fetchWalkingRoute(routeEndpoints(path.coordinates));
                    if (highlightLayer) map.removeLayer(highlightLayer);
                    highlightLayer = L.polyline(routeLine, { color: '#fa4616', weight: 5, opacity: 0.95 }).addTo(map);
                    map.fitBounds(highlightLayer.getBounds(), { padding: [60, 60] });
                });
            }
            classList.appendChild(row);
        });
        card.appendChild(classList);

        const actions = document.createElement('div');
        actions.className = 'action-row';
        actions.innerHTML = `
            <button id="zone-btn">Zone cost</button>
            <button id="verify-btn">Verify schedule</button>
            <button id="remove-student-btn" class="danger">Remove</button>
        `;
        card.appendChild(actions);

        const subResult = document.createElement('div');
        subResult.id = 'student-sub-result';
        card.appendChild(subResult);

        detailsEl.innerHTML = '';
        detailsEl.appendChild(card);

        document.getElementById('zone-btn').addEventListener('click', () => showZone(ufid));
        document.getElementById('verify-btn').addEventListener('click', () => showVerifySchedule(ufid));
        document.getElementById('remove-student-btn').addEventListener('click', () => removeStudent(ufid));
    } catch (err) {
        detailsEl.innerHTML = `<p class="status error">${escapeHtml(err.message)}</p>`;
    }
}

async function showZone(ufid) {
    const subResult = document.getElementById('student-sub-result');
    subResult.innerHTML = '<p class="status">Computing zone…</p>';
    clearZoneHighlight();
    try {
        const zone = await fetchJSON(`/api/students/${ufid}/zone`);
        subResult.innerHTML = `<div class="sub-result">Zone cost: <strong>${zone.cost}</strong> (${zone.vertices.length} locations)</div>`;

        zoneMarkersLayer = L.layerGroup(
            zone.vertices
                .filter(id => locations[id])
                .map(id => L.circleMarker([locations[id].lat, locations[id].lng], {
                    radius: 8,
                    color: '#1a7a3d',
                    fillColor: '#1a7a3d',
                    fillOpacity: 0.5,
                    weight: 2,
                }).bindPopup(escapeHtml(locationName(id))))
        ).addTo(map);
    } catch (err) {
        subResult.innerHTML = `<p class="status error">${escapeHtml(err.message)}</p>`;
    }
}

async function showVerifySchedule(ufid) {
    const subResult = document.getElementById('student-sub-result');
    subResult.innerHTML = '<p class="status">Checking schedule…</p>';
    try {
        const data = await fetchJSON(`/api/students/${ufid}/verify-schedule`);
        if (!data.pairs.length) {
            subResult.innerHTML = '<div class="sub-result">Not enough classes to check (need 2+).</div>';
            return;
        }
        const rows = data.pairs.map(p => `
            <div class="schedule-pair">
                <span>${escapeHtml(p.from)} &rarr; ${escapeHtml(p.to)}</span>
                <span class="${p.ok ? 'ok' : 'fail'}">${p.ok ? 'OK' : 'TOO TIGHT'}</span>
            </div>
        `).join('');
        subResult.innerHTML = `<div class="sub-result">${rows}</div>`;
    } catch (err) {
        subResult.innerHTML = `<p class="status error">${escapeHtml(err.message)}</p>`;
    }
}

async function removeStudent(ufid) {
    if (!confirm('Remove this student?')) return;
    try {
        await del(`/api/students/${ufid}`);
        clearZoneHighlight();
        document.getElementById('student-details').innerHTML = '';
        await loadStudentSelect();
    } catch (err) {
        alert('Failed to remove student: ' + err.message);
    }
}

/* ---------- add student form ---------- */

const addForm = document.getElementById('add-student-form');

document.getElementById('show-add-student-btn').addEventListener('click', () => {
    addForm.classList.toggle('hidden');
});
document.getElementById('cancel-add-student-btn').addEventListener('click', () => {
    addForm.reset();
    residenceSelect.value = ''; // native form reset only clears the visible text, not the widget's committed value
    document.getElementById('add-student-status').textContent = '';
    addForm.classList.add('hidden');
});

addForm.addEventListener('submit', async e => {
    e.preventDefault();
    const statusEl = document.getElementById('add-student-status');
    const name = document.getElementById('student-name').value.trim();
    const ufid = document.getElementById('student-ufid').value.trim();
    const residence = residenceSelect.value;
    const codes = [...document.querySelectorAll('#student-classes input:checked')].map(i => i.value);

    if (!name || !ufid || !residence) {
        statusEl.textContent = 'Fill in all fields.';
        statusEl.className = 'status error';
        return;
    }
    if (codes.length < 1 || codes.length > 6) {
        statusEl.textContent = 'Select between 1 and 6 classes.';
        statusEl.className = 'status error';
        return;
    }

    statusEl.textContent = 'Adding…';
    statusEl.className = 'status';
    try {
        const data = await postForm('/api/students', { name, ufid, residence, codes: codes.join(',') });
        if (data.result !== 'successful') {
            statusEl.textContent = 'Could not add student (duplicate UFID or invalid data).';
            statusEl.className = 'status error';
            return;
        }
        statusEl.textContent = 'Added.';
        statusEl.className = 'status ok';
        addForm.reset();
        residenceSelect.value = '';
        addForm.classList.add('hidden');
        await loadStudentSelect(ufid);
    } catch (err) {
        statusEl.textContent = err.message;
        statusEl.className = 'status error';
    }
});

/* ---------- schedule builder tab ---------- */

async function loadScheduleCourses() {
    const courses = await fetchJSON('/api/schedule-builder/courses');
    courses.forEach(c => { courseCatalog[c.code] = c; });
    courseFinderSelect.setOptions(
        [...courses].sort((a, b) => a.code.localeCompare(b.code))
            .map(c => ({ id: c.code, label: `${c.code} (${c.sections.length} sections)` }))
    );

    const container = document.getElementById('schedule-courses');
    container.innerHTML = '';
    courses.forEach(c => {
        const label = document.createElement('label');
        const input = document.createElement('input');
        input.type = 'checkbox';
        input.value = c.code;
        input.name = 'schedule-course';
        label.appendChild(input);
        label.appendChild(document.createTextNode(`${c.code} (${c.sections.length} sections)`));
        container.appendChild(label);
    });
}

function clearScheduleRoute() {
    if (scheduleRouteLayer) {
        map.removeLayer(scheduleRouteLayer);
        scheduleRouteLayer = null;
    }
}

document.getElementById('recommend-btn').addEventListener('click', async () => {
    const statusEl = document.getElementById('schedule-status');
    const resultEl = document.getElementById('schedule-result');
    const timelineEl = document.getElementById('schedule-timeline');
    const btn = document.getElementById('recommend-btn');

    const home = homeSelect.value;
    const codes = [...document.querySelectorAll('#schedule-courses input:checked')].map(i => i.value);

    statusEl.textContent = '';
    statusEl.classList.remove('error');
    resultEl.classList.add('hidden');
    timelineEl.classList.add('hidden');
    clearScheduleRoute();
    lastRecommendation = null;

    if (!home) return;
    if (codes.length < 1 || codes.length > 6) {
        statusEl.textContent = 'Select between 1 and 6 courses.';
        statusEl.classList.add('error');
        return;
    }

    btn.disabled = true;
    statusEl.textContent = 'Optimizing…';
    try {
        const data = await postForm('/api/schedule-builder/recommend', { home, codes: codes.join(',') });
        if (!data.found) {
            statusEl.textContent = 'No conflict-free combination of sections exists for these courses.';
            statusEl.classList.add('error');
            return;
        }
        statusEl.textContent = '';
        lastRecommendation = data;

        const sectionsEl = document.getElementById('schedule-sections');
        sectionsEl.innerHTML = data.sections.map(s => `
            <div class="class-row" style="cursor:default">
                <span>${escapeHtml(s.code)} @ ${escapeHtml(s.locationName)}</span>
                <span class="time">${s.start}-${s.end}</span>
            </div>
        `).join('');
        document.getElementById('schedule-total-time').textContent = data.totalTravelMinutes;

        const homeLoc = locations[home];
        const routePoints = [];
        if (homeLoc) routePoints.push([homeLoc.lat, homeLoc.lng]);
        data.sections.forEach(s => {
            const loc = locations[s.locationId];
            if (loc) routePoints.push([loc.lat, loc.lng]);
        });
        if (routePoints.length > 1) {
            statusEl.textContent = 'Drawing route…';
            const routeLine = await fetchWalkingRoute(routePoints);
            scheduleRouteLayer = L.polyline(routeLine, {
                color: '#1a7a3d',
                weight: 4,
                opacity: 0.9,
            }).addTo(map);
            map.fitBounds(scheduleRouteLayer.getBounds(), { padding: [60, 60] });
            statusEl.textContent = '';
        }

        resultEl.classList.remove('hidden');
    } catch (err) {
        statusEl.textContent = err.message;
        statusEl.classList.add('error');
    } finally {
        btn.disabled = false;
    }
});

document.getElementById('finalize-schedule-btn').addEventListener('click', () => {
    const timelineEl = document.getElementById('schedule-timeline');
    if (!lastRecommendation) return;
    if (!timelineEl.classList.contains('hidden')) {
        timelineEl.classList.add('hidden');
        return;
    }
    timelineEl.innerHTML = lastRecommendation.legs.map(leg => `
        <div class="schedule-pair">
            <span>${escapeHtml(leg.fromLabel)} &rarr; ${escapeHtml(leg.toCode)} (${leg.travelMinutes} min)</span>
            <span class="ok">leave by ${leg.leaveBy}</span>
        </div>
    `).join('');
    timelineEl.classList.remove('hidden');
});

/* ---------- boot ---------- */

async function init() {
    await loadCampus();
    await loadClassCatalog();
    await Promise.all([loadConnectivity(), loadStudentSelect(), loadScheduleCourses()]);
}

init().catch(err => {
    const statusEl = document.getElementById('explore-status');
    statusEl.textContent = 'Failed to load campus data: ' + err.message;
    statusEl.classList.add('error');
});
