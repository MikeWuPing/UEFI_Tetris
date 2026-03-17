#include "types.h"

// External function declaration
extern VOID TetrisMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable);

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  // Debug: Print immediately at entry
  Print(L"\r\n>>> UefiMain ENTRY <<<\r\n");
  Print(L"ImageHandle: 0x%x, SystemTable: 0x%x\r\n", ImageHandle, SystemTable);

  // Call game main function
  TetrisMain(ImageHandle, SystemTable);

  Print(L"\r\n>>> UefiMain EXIT <<<\r\n");
  return EFI_SUCCESS;
}