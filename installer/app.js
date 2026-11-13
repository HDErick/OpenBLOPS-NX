import { readRelease } from './release.mjs';
import { ZipWriter } from './zip-writer.mjs';

const element = id => document.getElementById(id);
const sourceButton = element('source');
const sourceInput = element('source-files');
const destinationButton = element('destination');
const installButton = element('install');
const zipButton = element('zip');
const progress = element('progress');
const errors = element('errors');
const controls = [sourceButton, destinationButton, installButton, zipButton];
const canPickFolders = typeof showDirectoryPicker === 'function';
const canSaveFile = typeof showSaveFilePicker === 'function';
const canStreamDownload = 'serviceWorker' in navigator && typeof ReadableStream === 'function';
// The chosen Black Ops folder as a flat list: { parts: ['zone', 'x.ff'], getFile }.
// sourceHandle is set only when it came from showDirectoryPicker (Chrome, Edge).
let sourceEntries, sourceHandle, destination;

if (canStreamDownload) navigator.serviceWorker.register('./sw.js').catch(() => {});

function status(message) {
  element('result').hidden = false;
  element('status').textContent = message;
}

function updateButtons() {
  installButton.disabled = !(sourceEntries && destination);
  zipButton.disabled = !sourceEntries || !(canSaveFile || canStreamDownload);
  // Say what each greyed-out button still needs.
  installButton.title = installButton.disabled
    ? (sourceEntries ? 'Choose a destination first.' : 'Choose the Black Ops folder and a destination first.') : '';
  zipButton.title = zipButton.disabled ? 'Choose the Black Ops folder first.' : '';
}

function sourceReady(name) {
  element('source-label').textContent = name;
  const zones = sourceEntries.filter(({ parts }) => parts[0].toLowerCase() === 'zone' && /\.ff$/i.test(parts.at(-1))).length;
  status(`Game folder ready (${zones} zones). ${destination ? 'Choose Save as ZIP or Install to folder.'
    : 'Choose Save as ZIP, or a destination to install to a folder.'}`);
}

function checkSource(entries) {
  const has = test => entries.some(({ parts }) => test(parts));
  if (!has(parts => parts[0].toLowerCase() === 'main' && parts.length > 1) ||
      !has(parts => parts[0].toLowerCase() === 'zone' && parts.length > 1) ||
      !has(parts => parts.length === 1 && parts[0].toLowerCase() === 'localization.txt')) {
    throw new Error('Choose the Black Ops folder that contains main, zone, and localization.txt.');
  }
  return entries;
}

async function listFiles(folder, path) {
  const files = [];
  for await (const [name, handle] of folder.entries()) {
    const parts = [...path, name];
    if (handle.kind === 'directory') files.push(...await listFiles(handle, parts));
    else files.push({ parts, getFile: () => handle.getFile() });
  }
  return files;
}

async function pickSource() {
  if (!canPickFolders) {
    sourceInput.click();   // <input webkitdirectory>: Firefox and Safari
    return;
  }
  try {
    const folder = await showDirectoryPicker({ id: 'openblops-read', mode: 'read' });
    status('Reading the game folder…');
    const entries = [];
    for (const name of ['main', 'zone']) {
      try { entries.push(...await listFiles(await folder.getDirectoryHandle(name), [name])); } catch {}
    }
    try {
      const file = await folder.getFileHandle('localization.txt');
      entries.push({ parts: ['localization.txt'], getFile: () => file.getFile() });
    } catch {}
    sourceEntries = checkSource(entries);
    sourceHandle = folder;
    sourceReady(folder.name);
  } catch (error) {
    if (error.name !== 'AbortError') status(error.message);
  }
  updateButtons();
}

sourceInput.addEventListener('change', () => {
  try {
    const files = [...sourceInput.files];
    if (!files.length) return;
    // webkitRelativePath is "<chosen folder>/main/..."; drop the folder itself.
    const entries = files.map(file => ({
      parts: file.webkitRelativePath.split('/').slice(1),
      getFile: async () => file,
    })).filter(({ parts }) => parts.length);
    sourceEntries = checkSource(entries);
    sourceHandle = null;
    sourceReady(files[0].webkitRelativePath.split('/')[0]);
  } catch (error) {
    sourceEntries = null;
    status(error.message);
  }
  updateButtons();
});

async function pickDestination() {
  try {
    destination = await showDirectoryPicker({ id: 'openblops-readwrite', mode: 'readwrite' });
    element('destination-label').textContent = `${destination.name}/switch/OpenBLOPS-NX/`;
    status(sourceEntries ? 'Ready. Choose Install to folder.' : 'Destination set. Now choose the Black Ops folder.');
  } catch (error) {
    if (error.name !== 'AbortError') status(error.message);
  }
  updateButtons();
}

// Where the install goes: a folder (SD card or staging), or a ZIP holding
// switch/OpenBLOPS-NX/ to extract onto the SD card's root.
function folderSink(root) {
  return {
    async save(parts, stream) {
      let folder = root;
      for (const part of parts.slice(0, -1)) {
        folder = await folder.getDirectoryHandle(part, { create: true });
      }
      const file = await folder.getFileHandle(parts.at(-1), { create: true });
      await stream.pipeTo(await file.createWritable());
    },
    async close() {},
  };
}

function zipSink(writable) {
  const zip = new ZipWriter(writable);
  return {
    save: (parts, stream) => zip.add(['switch', 'OpenBLOPS-NX', ...parts].join('/'), stream),
    close: () => zip.close(),
  };
}

// A writable that feeds sw.js, which serves it as the download. The download
// starts through a hidden iframe: a navigation always reaches the service
// worker (an <a download> may go straight to the network in Chrome).
async function serviceWorkerWritable(name) {
  const registration = await navigator.serviceWorker.ready;
  const worker = registration.active;
  if (!worker) throw new Error('The download helper is not ready yet. Reload the page and try again.');
  const channel = new MessageChannel();
  const port = channel.port1;
  let credits = 0, cancelled = false, wake = null;
  const ready = new Promise(resolve => {
    port.onmessage = ({ data }) => {
      if (data.type === 'ready') resolve();
      else if (data.type === 'pull') credits++;
      else if (data.type === 'cancel') cancelled = true;
      if (wake) { const w = wake; wake = null; w(); }
    };
  });
  const id = crypto.randomUUID ? crypto.randomUUID() : String(Date.now()) + Math.random().toString(16).slice(2);
  worker.postMessage({ type: 'download', id, name }, [channel.port2]);
  await ready;
  const keepAlive = setInterval(() => worker.postMessage({ type: 'ping' }), 10000);
  const frame = document.createElement('iframe');
  frame.hidden = true;
  frame.src = `./download/${id}/${encodeURIComponent(name)}`;
  document.body.append(frame);

  const CHUNK = 1 << 20;
  let buffer = new Uint8Array(CHUNK), used = 0;
  const flush = async () => {
    if (!used) return;
    while (!credits && !cancelled) await new Promise(resolve => { wake = resolve; });
    if (cancelled) throw new DOMException('The download was cancelled.', 'AbortError');
    credits--;
    const chunk = buffer.slice(0, used);
    port.postMessage({ type: 'chunk', chunk: chunk.buffer }, [chunk.buffer]);
    used = 0;
  };
  return {
    async write(bytes) {
      for (let at = 0; at < bytes.length;) {
        const n = Math.min(CHUNK - used, bytes.length - at);
        buffer.set(bytes.subarray(at, at + n), used);
        used += n; at += n;
        if (used === CHUNK) await flush();
      }
    },
    async close() {
      await flush();
      port.postMessage({ type: 'close' });
      clearInterval(keepAlive);
      setTimeout(() => frame.remove(), 60000);
    },
  };
}

function convert(worker, file, pack = false) {
  return new Promise((resolve, reject) => {
    worker.onmessage = ({ data }) => data.error ? reject(new Error(data.error)) : resolve(data.output);
    worker.onerror = event => reject(new Error(event.message || 'The converter worker stopped.'));
    worker.postMessage({ file, pack });
  });
}

// Executables that load packed (zstd) zones carry this marker
// (src/nx/nx_kbz_storage.cpp); zones are packed only for those.
const PACKED_ZONE_MARKER = new TextEncoder().encode('openblops-kbzz-1');

function contains(bytes, needle) {
  const first = needle[0];
  for (let i = bytes.indexOf(first); i !== -1 && i <= bytes.length - needle.length; i = bytes.indexOf(first, i + 1)) {
    let j = 1;
    while (j < needle.length && bytes[i + j] === needle[j]) j++;
    if (j === needle.length) return true;
  }
  return false;
}

// The latest GitHub release, which the Pages workflow (and serve.py locally)
// publishes next to the installer: GitHub's own release downloads do not
// allow cross-origin reads, so the browser cannot fetch them directly.
async function downloadRelease() {
  const response = await fetch('./release/OpenBLOPS-NX.zip', { cache: 'no-cache' });
  if (!response.ok) {
    throw new Error('The latest OpenBLOPS-NX release could not be downloaded. Reload the page and try again.');
  }
  return response.arrayBuffer();
}

function conversionJobs(zoneFiles) {
  const jobs = new Map();
  for (const entry of zoneFiles) {
    const name = entry.parts.at(-1);
    if (!/\.ff$/i.test(name)) continue;
    const output = name.replace(/\.ff$/i, '.kbz').toLowerCase();
    if (jobs.has(output)) {
      throw new Error(`Multiple zones would write ${output}. Select an installation with one copy of each zone.`);
    }
    jobs.set(output, entry);
  }
  if (!jobs.size) throw new Error('No .ff zones were found in the Black Ops zone folder.');
  return jobs;
}

async function install(toZip) {
  for (const control of controls) control.disabled = true;
  progress.value = 0;
  errors.replaceChildren();
  element('issues').hidden = true;
  let worker, sink;
  const zipName = 'OpenBLOPS-NX-Switch.zip';
  try {
    let zipFile;
    if (toZip && canSaveFile) {
      zipFile = await showSaveFilePicker({
        id: 'openblops-zip', suggestedName: zipName,
        types: [{ description: 'ZIP archive', accept: { 'application/zip': ['.zip'] } }],
      });
    } else if (!toZip && sourceHandle) {
      const sourceFromDestination = await destination.resolve(sourceHandle);
      if (await sourceHandle.resolve(destination) !== null ||
          sourceFromDestination?.slice(0, 2).join('/').toLowerCase() === 'switch/openblops-nx') {
        throw new Error('Choose an SD card or staging folder outside the Black Ops installation.');
      }
    }
    status('Checking the release ZIP and game folders…');
    const programs = await downloadRelease().then(readRelease);
    const under = name => sourceEntries.filter(({ parts }) => parts.length > 1 && parts[0].toLowerCase() === name)
      .map(({ parts, getFile }) => ({ parts: [name, ...parts.slice(1)], getFile }));
    const zoneFiles = under('zone');
    const mainFiles = under('main');
    const localization = sourceEntries.find(({ parts }) => parts.length === 1 && parts[0].toLowerCase() === 'localization.txt');
    const jobs = conversionJobs(zoneFiles);
    worker = new Worker('./converter-worker.js', { type: 'module' });
    await convert(worker, null);
    if (toZip) {
      sink = zipSink(canSaveFile ? await zipFile.createWritable() : await serviceWorkerWritable(zipName));
    } else {
      const switchFolder = await destination.getDirectoryHandle('switch', { create: true });
      sink = folderSink(await switchFolder.getDirectoryHandle('OpenBLOPS-NX', { create: true }));
    }
    const gameFiles = mainFiles.filter(({ parts }) => !/\.(log|dmp|mdmp)$/i.test(parts.at(-1)));
    const assets = [...gameFiles, { parts: ['localization.txt'], getFile: localization.getFile }];
    // The Switch build loads every zone from kbz/; the .ff only has to exist
    // (where it sits picks the zone's language, and DLC maps are found by it).
    // An empty file in its place saves the 3+ GB of zone data.
    const placeholders = zoneFiles.filter(({ parts }) => /\.ff$/i.test(parts.at(-1)));
    const otherZoneFiles = zoneFiles.filter(({ parts }) => !/\.ff$/i.test(parts.at(-1)));
    assets.push(...otherZoneFiles);
    const pack = [...programs.values()].every(program => contains(program, PACKED_ZONE_MARKER));
    let finished = 0;
    const total = assets.length + placeholders.length + jobs.size + programs.size;
    const advance = () => { progress.value = ++finished / total; };
    for (const { parts, getFile } of assets) {
      status(`Copying ${parts.join('/')}…`);
      await sink.save(parts, (await getFile()).stream());
      advance();
    }
    for (const { parts } of placeholders) {
      await sink.save(parts, new Blob([]).stream());
      advance();
    }
    let failed = 0;
    for (const [name, { parts, getFile }] of jobs) {
      status(`${pack ? 'Converting and compressing' : 'Converting'} ${parts.join('/')}…`);
      let converted;
      try {
        converted = await convert(worker, await getFile(), pack);
      } catch (error) {
        const issue = document.createElement('li');
        issue.textContent = `${parts.join('/')} — ${error.message}`;
        errors.append(issue);
        element('issues').hidden = false;
        failed++;
      }
      if (converted) await sink.save(['kbz', name], new Blob([converted]).stream());
      advance();
    }
    for (const [name, program] of programs) {
      await sink.save([name], new Blob([program]).stream());
      advance();
    }
    if (toZip) status('Finishing the ZIP…');
    await sink.close();
    sink = null;
    const where = toZip
      ? 'Extract the ZIP to the root of the SD card (it holds switch/OpenBLOPS-NX/).'
      : 'Launch in Homebrew Menu application mode.';
    status(failed
      ? `Packaged both Switch builds. Converted ${jobs.size - failed} of ${jobs.size} zones; check the report for zones that could affect gameplay. ${where}`
      : `Packaged both Switch builds and converted all ${jobs.size} zones. ${where}`);
  } catch (error) {
    if (error.name === 'AbortError') status('Cancelled.');
    else status(error.message || String(error));
  } finally {
    worker?.terminate();
    for (const control of controls) control.disabled = false;
    if (!canPickFolders) destinationButton.disabled = true;
    updateButtons();
  }
}

sourceButton.addEventListener('click', pickSource);
installButton.addEventListener('click', () => install(false));
zipButton.addEventListener('click', () => install(true));
if (canPickFolders) {
  destinationButton.addEventListener('click', pickDestination);
} else {
  destinationButton.disabled = true;
  element('destination-label').textContent = 'Folder install needs desktop Chrome or Edge. Use Save as ZIP here.';
}
updateButtons();
