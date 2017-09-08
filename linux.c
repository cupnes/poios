#include "efi.h"
#include "common.h"
#include "linux.h"

void boot_linux(void)
{
	struct EFI_DEVICE_PATH_PROTOCOL *dev_path;
	struct EFI_DEVICE_PATH_PROTOCOL *dev_node;
	struct EFI_DEVICE_PATH_PROTOCOL *dev_merged;
	unsigned long long status;
	void *image;
	unsigned short options[] = L"root=/dev/sdb2 init=/bin/sh rootwait";
	struct EFI_LOADED_IMAGE_PROTOCOL *lip_image;

	status = ST->BootServices->OpenProtocol(LIP->DeviceHandle, &dpp_guid,
						(void **)&dev_path, IH, NULL,
						EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	assert(status, L"OpenProtocol");

	dev_node = DPFTP->ConvertTextToDeviceNode(L"\\bzImage.efi");

	dev_merged = DPUP->AppendDeviceNode(dev_path, dev_node);

	puts(L"dev_merged: ");
	puts(DPTTP->ConvertDevicePathToText(dev_merged, FALSE, FALSE));
	puts(L"\r\n");

	status = ST->BootServices->LoadImage(FALSE, IH, dev_merged, NULL, 0,
					     &image);
	assert(status, L"LoadImage");
	puts(L"LoadImage: Success!\r\n");

	status = ST->BootServices->OpenProtocol(image, &lip_guid,
						(void **)&lip_image, IH, NULL,
						EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	assert(status, L"OpenProtocol(lip_image)");
	lip_image->LoadOptions = options;
	lip_image->LoadOptionsSize =
		(strlen(options) + 1) * sizeof(unsigned short);

	status = ST->BootServices->StartImage(image, NULL, NULL);
	assert(status, L"StartImage");
	puts(L"StartImage: Success!\r\n");
}
