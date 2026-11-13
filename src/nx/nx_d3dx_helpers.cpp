// Developer-only D3DX entrypoints; shipped SM3 bytecode is handled by DXVK.
#include "compat/d3dx9.h"
void *ID3DXBuffer::GetBufferPointer() { return NULL; }
DWORD ID3DXBuffer::GetBufferSize() { return 0; }
ULONG ID3DXBuffer::AddRef() { return 1; }
ULONG ID3DXBuffer::Release() { return 0; }

void *ID3DXConstantTable::GetBufferPointer() { return NULL; }
DWORD ID3DXConstantTable::GetBufferSize() { return 0; }
ULONG ID3DXConstantTable::AddRef() { return 1; }
ULONG ID3DXConstantTable::Release() { return 0; }

HRESULT D3DXCompileShader(LPCSTR, UINT, const D3DXMACRO *, LPD3DXINCLUDE, LPCSTR, LPCSTR,
                          DWORD, LPD3DXBUFFER *shader, LPD3DXBUFFER *errorMsgs, LPD3DXCONSTANTTABLE *table)
{
    if (shader) *shader = NULL;
    if (errorMsgs) *errorMsgs = NULL;
    if (table) *table = NULL;
    return E_FAIL; // runtime shader compilation unavailable; fastfiles carry prebuilt shaders
}

HRESULT D3DXCreateBuffer(DWORD, LPD3DXBUFFER *buffer)
{
    if (buffer) *buffer = NULL;
    return E_FAIL;
}

HRESULT D3DXGetShaderConstantTable(const DWORD *, LPD3DXCONSTANTTABLE *table)
{
    if (table) *table = NULL;
    return E_FAIL;
}

HRESULT D3DXGetShaderInputSemantics(const DWORD *, D3DXSEMANTIC *, UINT *count)
{
    if (count) *count = 0;
    return E_FAIL;
}

HRESULT D3DXGetShaderOutputSemantics(const DWORD *, D3DXSEMANTIC *, UINT *count)
{
    if (count) *count = 0;
    return E_FAIL;
}

HRESULT D3DXSaveSurfaceToFileA(LPCSTR, D3DXIMAGE_FILEFORMAT, IDirect3DSurface9 *, const void *, const RECT *)
{
    return E_FAIL;
}
