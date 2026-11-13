import createConverter from './generated/convert.mjs';
import createPacker from './generated/kbzpack.mjs';

let convertWasm, packWasm;

async function load(name) {
  const response = await fetch(`./generated/${name}.wasm`);
  if (!response.ok) throw new Error(`The WebAssembly ${name} is missing.`);
  return response.arrayBuffer();
}

// One message per zone: { file, pack }. pack compresses the converted zone
// (kbzpack: zstd, read by the Switch build as it loads the zone).
self.onmessage = async ({ data }) => {
  try {
    if (!convertWasm) convertWasm = await load('convert');
    if (!packWasm) packWasm = await load('kbzpack');
    if (!data || !data.file) {
      postMessage({ ready: true });
      return;
    }
    const diagnostics = [];
    const print = message => { if (diagnostics.length < 8) diagnostics.push(message); };
    const converter = await createConverter({
      wasmBinary: new Uint8Array(convertWasm), noInitialRun: true, print, printErr: print,
    });
    converter.FS.writeFile('/input.ff', new Uint8Array(await data.file.arrayBuffer()), { canOwn: true });
    const code = converter.callMain(['/input.ff', '/output.kbz']);
    if (code !== 0) throw new Error(diagnostics.join(' ').slice(0, 400) || `Converter exited with status ${code}.`);
    let output = converter.FS.readFile('/output.kbz');
    if (output[0] !== 75 || output[1] !== 66 || output[2] !== 90 || output[3] !== 49) {
      throw new Error('The converter did not produce a valid KBZ header.');
    }
    if (data.pack) {
      const packer = await createPacker({
        wasmBinary: new Uint8Array(packWasm), noInitialRun: true, print, printErr: print,
      });
      packer.FS.writeFile('/in.kbz', output, { canOwn: true });
      const packed = packer.callMain(['-l', '9', '/in.kbz', '/out.kbz']);
      if (packed !== 0) throw new Error(diagnostics.join(' ').slice(0, 400) || `kbzpack exited with status ${packed}.`);
      output = packer.FS.readFile('/out.kbz');
      if (output[0] !== 75 || output[1] !== 66 || output[2] !== 90 || output[3] !== 90) {
        throw new Error('kbzpack did not produce a packed zone.');
      }
    }
    postMessage({ output }, [output.buffer]);
  } catch (error) {
    postMessage({ error: error.message || String(error) });
  }
};
