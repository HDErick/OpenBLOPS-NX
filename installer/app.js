import { readRelease } from './release.mjs';

const element = id => document.getElementById(id);
const sourceButton = element('source');
const destinationButton = element('destination');
const releaseInput = element('release');
const installButton = element('install');
const progress = element('progress');
const errors = element('errors');
let source, destination, releaseFile;

function status(message) {
  element('result').hidden = false;
  element('status').textContent = message;
}

async function pick(mode) {
  try {
    const folder = await showDirectoryPicker({ id: `openblops-${mode}`, mode });
    if (mode === 'read') {
      await folder.getDirectoryHandle('main');
      await folder.getDirectoryHandle('zone');
      await folder.getFileHandle('localization.txt');
      source = folder;
      element('source-label').textContent = folder.name;
    } else {
      destination = folder;
      element('destination-label').textContent = `${folder.name}/switch/OpenBLOPS-NX/`;
    }
    installButton.disabled = !(source && destination);
  } catch (error) {
    if (error.name !== 'AbortError') {
      status(mode === 'read' && error.name === 'NotFoundError'
        ? 'Choose the Black Ops folder that contains main, zone, and localization.txt.'
        : error.message);
    }
  }
}

async function listFiles(folder, path) {
  const files = [];
  for await (const [name, handle] of folder.entries()) {
    const parts = [...path, name];
    if (handle.kind === 'directory') files.push(...await listFiles(handle, parts));
    else files.push({ parts, handle });
  }
  return files;
}

async function save(root, parts, stream) {
  let folder = root;
  for (const part of parts.slice(0, -1)) {
    folder = await folder.getDirectoryHandle(part, { create: true });
  }
  const file = await folder.getFileHandle(parts.at(-1), { create: true });
  await stream.pipeTo(await file.createWritable());
}

function convert(worker, file) {
  return new Promise((resolve, reject) => {
    worker.onmessage = ({ data }) => data.error ? reject(new Error(data.error)) : resolve(data.output);
    worker.onerror = event => reject(new Error(event.message || 'The converter worker stopped.'));
    worker.postMessage(file);
  });
}

async function downloadRelease() {
  if (releaseFile) return releaseFile.arrayBuffer();
  const response = await fetch('./release/OpenBLOPS-NX.zip', { cache: 'no-cache' });
  if (!response.ok) {
    throw new Error('Download OpenBLOPS-NX.zip from the latest release, then select it with Choose release ZIP.');
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

async function install() {
  for (const control of [sourceButton, destinationButton, releaseInput, installButton]) control.disabled = true;
  progress.value = 0;
  errors.replaceChildren();
  element('issues').hidden = true;
  let worker;
  try {
    const sourceFromDestination = await destination.resolve(source);
    if (await source.resolve(destination) !== null ||
        sourceFromDestination?.slice(0, 2).join('/').toLowerCase() === 'switch/openblops-nx') {
      throw new Error('Choose an SD card or staging folder outside the Black Ops installation.');
    }
    status('Checking the release ZIP and game folders…');
    const [programs, zoneFiles, mainFiles] = await Promise.all([
      downloadRelease().then(readRelease),
      source.getDirectoryHandle('zone').then(folder => listFiles(folder, ['zone'])),
      source.getDirectoryHandle('main').then(folder => listFiles(folder, ['main'])),
    ]);
    const jobs = conversionJobs(zoneFiles);
    worker = new Worker('./converter-worker.js', { type: 'module' });
    await convert(worker, null);
    const switchFolder = await destination.getDirectoryHandle('switch', { create: true });
    const root = await switchFolder.getDirectoryHandle('OpenBLOPS-NX', { create: true });
    const gameFiles = mainFiles.filter(({ parts }) => !/\.(log|dmp|mdmp)$/i.test(parts.at(-1)));
    const assets = [...gameFiles, ...zoneFiles, {
      parts: ['localization.txt'], handle: await source.getFileHandle('localization.txt'),
    }];
    let finished = 0;
    const total = assets.length + jobs.size + programs.size;
    const advance = () => { progress.value = ++finished / total; };
    for (const { parts, handle } of assets) {
      status(`Copying ${parts.join('/')}…`);
      await save(root, parts, (await handle.getFile()).stream());
      advance();
    }
    let failed = 0;
    for (const [name, { parts, handle }] of jobs) {
      status(`Converting ${parts.join('/')}…`);
      let converted;
      try {
        converted = await convert(worker, await handle.getFile());
      } catch (error) {
        const issue = document.createElement('li');
        issue.textContent = `${parts.join('/')} — ${error.message}`;
        errors.append(issue);
        element('issues').hidden = false;
        failed++;
      }
      if (converted) await save(root, ['kbz', name], new Blob([converted]).stream());
      advance();
    }
    for (const [name, program] of programs) {
      await save(root, [name], new Blob([program]).stream());
      advance();
    }
    status(failed
      ? `Copied both Switch builds. Converted ${jobs.size - failed} of ${jobs.size} zones. Check the report for zones that could affect gameplay.`
      : `Installed both Switch builds and converted all ${jobs.size} zones. Launch in Homebrew Menu application mode.`);
  } catch (error) {
    status(error.message || String(error));
  } finally {
    worker?.terminate();
    for (const control of [sourceButton, destinationButton, releaseInput, installButton]) control.disabled = false;
  }
}

if (typeof showDirectoryPicker === 'function') {
  sourceButton.addEventListener('click', () => pick('read'));
  destinationButton.addEventListener('click', () => pick('readwrite'));
  installButton.addEventListener('click', install);
  releaseInput.addEventListener('change', () => {
    releaseFile = releaseInput.files[0];
    element('release-label').textContent = releaseFile?.name || 'Use the latest release packaged with this installer.';
  });
} else {
  sourceButton.disabled = destinationButton.disabled = releaseInput.disabled = true;
  status('Open the installer in desktop Chrome or Edge over HTTPS or localhost to choose folders.');
}
