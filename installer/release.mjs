export const executables = ['OpenBLOPS-NX.nro', 'OpenBLOPS-NX-MP.nro'];

async function inflate(payload, size) {
  const output = new Uint8Array(size);
  const stream = new Blob([payload]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  let offset = 0;
  for await (const chunk of stream) {
    if (offset + chunk.length > size) throw new Error('A release entry expands beyond its declared size.');
    output.set(chunk, offset);
    offset += chunk.length;
  }
  if (offset !== size) throw new Error('A release executable is truncated.');
  return output;
}

function titleOf(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (bytes.length < 32 || view.getUint32(16, false) !== 0x4e524f30) {
    throw new Error('The release contains an invalid Switch executable.');
  }
  const assets = view.getUint32(24, true);
  if (assets + 40 > bytes.length || view.getUint32(assets, false) !== 0x41534554) {
    throw new Error('A release executable is missing its Switch title.');
  }
  const title = assets + Number(view.getBigUint64(assets + 24, true));
  if (!Number.isSafeInteger(title) || title < assets + 40 || title + 512 > bytes.length) {
    throw new Error('A release executable has an invalid Switch title.');
  }
  const name = bytes.subarray(title, title + 512);
  return new TextDecoder().decode(name.subarray(0, name.indexOf(0) >>> 0));
}

export async function readRelease(buffer) {
  const view = new DataView(buffer);
  const bytes = new Uint8Array(buffer);
  let end = buffer.byteLength - 22;
  while (end >= Math.max(0, buffer.byteLength - 65557)) {
    if (view.getUint32(end, true) === 0x06054b50 &&
        end + 22 + view.getUint16(end + 20, true) === buffer.byteLength) break;
    end--;
  }
  if (end < 0 || end < buffer.byteLength - 65557) throw new Error('The release is not a ZIP archive.');
  const count = view.getUint16(end + 10, true);
  let entry = view.getUint32(end + 16, true);
  if (view.getUint16(end + 4, true) || view.getUint16(end + 6, true) || entry > end) {
    throw new Error('The release ZIP must be a single archive.');
  }
  const files = new Map();
  for (let i = 0; i < count; i++) {
    if (entry + 46 > end || view.getUint32(entry, true) !== 0x02014b50) {
      throw new Error('The release ZIP directory is damaged.');
    }
    const nameLength = view.getUint16(entry + 28, true);
    const extraLength = view.getUint16(entry + 30, true);
    const commentLength = view.getUint16(entry + 32, true);
    const next = entry + 46 + nameLength + extraLength + commentLength;
    if (next > end) throw new Error('The release ZIP entry is truncated.');
    const path = new TextDecoder().decode(bytes.subarray(entry + 46, entry + 46 + nameLength));
    const name = path.split('/').at(-1);
    if (executables.includes(name)) {
      const flags = view.getUint16(entry + 8, true);
      const method = view.getUint16(entry + 10, true);
      const compressed = view.getUint32(entry + 20, true);
      const size = view.getUint32(entry + 24, true);
      const local = view.getUint32(entry + 42, true);
      if (files.has(name) || flags & 1 || size > 128 * 1024 * 1024 ||
          local + 30 > entry || view.getUint32(local, true) !== 0x04034b50) {
        throw new Error(`Invalid ZIP entry for ${name}.`);
      }
      const start = local + 30 + view.getUint16(local + 26, true) + view.getUint16(local + 28, true);
      if (start + compressed > entry || (method !== 0 && method !== 8)) {
        throw new Error(`Unsupported ZIP entry for ${name}.`);
      }
      const payload = bytes.subarray(start, start + compressed);
      const nro = method === 0 ? payload : await inflate(payload, size);
      if (nro.length !== size || titleOf(nro) !== name.slice(0, -4)) {
        throw new Error(`${name} is missing its matching build. Download a corrected release ZIP.`);
      }
      files.set(name, nro);
    }
    entry = next;
  }
  if (files.size !== executables.length) throw new Error('The release ZIP must contain both Switch executables.');
  return files;
}
