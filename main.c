#include "efi.h"
#include "shell.h"

void efi_main(void *ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
	efi_init(ImageHandle, SystemTable);

	shell();
}
