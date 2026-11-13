import createConverter from './generated/convert.mjs';

let wasm;

self.onmessage = async ({ data: file }) => {
  try {
    if (!wasm) {
      const response = await fetch('./generated/convert.wasm');
      if (!response.ok) throw new Error('The WebAssembly converter is missing.');
      wasm = await response.arrayBuffer();
    }
    if (!file) {
      postMessage({ ready: true });
      return;
    }
    const diagnostics = [];
    const converter = await createConverter({
      wasmBinary: new Uint8Array(wasm),
      noInitialRun: true,
      print: message => { if (diagnostics.length < 8) diagnostics.push(message); },
      printErr: message => { if (diagnostics.length < 8) diagnostics.push(message); },
    });
    const input = new Uint8Array(await file.arrayBuffer());
    converter.FS.writeFile('/input.ff', input, { canOwn: true });
    const code = converter.callMain(['/input.ff', '/output.kbz']);
    if (code !== 0) throw new Error(diagnostics.join(' ').slice(0, 400) || `Converter exited with status ${code}.`);
    const output = converter.FS.readFile('/output.kbz');
    if (output[0] !== 75 || output[1] !== 66 || output[2] !== 90 || output[3] !== 49) {
      throw new Error('The converter did not produce a valid KBZ header.');
    }
    postMessage({ output }, [output.buffer]);
  } catch (error) {
    postMessage({ error: error.message || String(error) });
  }
};
