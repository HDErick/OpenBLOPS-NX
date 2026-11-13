import { readFile } from 'node:fs/promises';
import { readRelease } from './release.mjs';

const archive = await readFile(process.argv[2]);
const programs = await readRelease(archive.buffer.slice(archive.byteOffset, archive.byteOffset + archive.byteLength));
for (const [name, bytes] of programs) console.log(`${name} (${bytes.length} bytes)`);
