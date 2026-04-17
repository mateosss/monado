// Copyright 2026, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  HID implementation based on libusb.
 * @author
 * @ingroup st_prober
 */

#include "p_prober.h"

#ifdef XRT_HAVE_LIBUSB

#include "os/os_hid.h"
#include "util/u_misc.h"

#include <errno.h>
#include <string.h>

/*!
 * @implements os_hid_device
 */
struct hid_libusb
{
	struct os_hid_device base;

	libusb_device_handle *dev_handle;
	int interface_number;
	uint8_t ep_in;
	uint8_t ep_out;
};

static inline int
timeout_from_milliseconds(int milliseconds)
{
	// libusb: 0 means block forever.
	return milliseconds < 0 ? 0 : milliseconds;
}

static int
os_hid_libusb_read(struct os_hid_device *ohdev, uint8_t *data, size_t length, int milliseconds)
{
	struct hid_libusb *lhdev = (struct hid_libusb *)ohdev;
	int transferred = 0;

	if (lhdev->ep_in == 0) {
		return -1;
	}

	int ret = libusb_interrupt_transfer(lhdev->dev_handle, lhdev->ep_in, data, (int)length, &transferred,
	                                    timeout_from_milliseconds(milliseconds));
	if (ret == LIBUSB_SUCCESS) {
		return transferred;
	}
	if (ret == LIBUSB_ERROR_TIMEOUT || ret == LIBUSB_ERROR_INTERRUPTED) {
		return 0;
	}

	return -1;
}

static int
os_hid_libusb_write(struct os_hid_device *ohdev, const uint8_t *data, size_t length)
{
	struct hid_libusb *lhdev = (struct hid_libusb *)ohdev;
	int transferred = 0;

	if (lhdev->ep_out != 0) {
		int ret = libusb_interrupt_transfer(lhdev->dev_handle, lhdev->ep_out, (unsigned char *)data, (int)length,
		                                    &transferred, 1000);
		if (ret == LIBUSB_SUCCESS) {
			return transferred;
		}

		return -1;
	}

	if (length == 0) {
		return 0;
	}

	uint16_t wValue = (2u << 8u) | data[0];
	int ret = libusb_control_transfer(
	    lhdev->dev_handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT, 0x09, wValue,
	    (uint16_t)lhdev->interface_number, (unsigned char *)data, (uint16_t)length, 1000);
	if (ret < 0) {
		return -1;
	}

	return ret;
}

static int
os_hid_libusb_get_feature_timeout(struct os_hid_device *ohdev, void *data, size_t length, uint32_t timeout)
{
	struct hid_libusb *lhdev = (struct hid_libusb *)ohdev;
	uint8_t report_num = ((uint8_t *)data)[0];
	uint16_t wValue = (3u << 8u) | report_num;

	int ret = libusb_control_transfer(
	    lhdev->dev_handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN, 0x01, wValue,
	    (uint16_t)lhdev->interface_number, data, (uint16_t)length, timeout);
	if (ret < 0) {
		return -1;
	}

	return ret;
}

static int
os_hid_libusb_get_feature(struct os_hid_device *ohdev, uint8_t report_num, uint8_t *data, size_t length)
{
	data[0] = report_num;
	return os_hid_libusb_get_feature_timeout(ohdev, data, length, 1000);
}

static int
os_hid_libusb_set_feature(struct os_hid_device *ohdev, const uint8_t *data, size_t length)
{
	struct hid_libusb *lhdev = (struct hid_libusb *)ohdev;

	if (length == 0) {
		return -1;
	}

	uint16_t wValue = (3u << 8u) | data[0];
	int ret = libusb_control_transfer(
	    lhdev->dev_handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT, 0x09, wValue,
	    (uint16_t)lhdev->interface_number, (unsigned char *)data, (uint16_t)length, 1000);
	if (ret < 0) {
		return -1;
	}

	return ret;
}

static int
os_hid_libusb_get_physical_address(struct os_hid_device *ohdev, uint8_t *data, size_t size)
{
	(void)ohdev;
	(void)data;
	(void)size;
	return -ENOSYS;
}

static void
os_hid_libusb_destroy(struct os_hid_device *ohdev)
{
	struct hid_libusb *lhdev = (struct hid_libusb *)ohdev;

	libusb_release_interface(lhdev->dev_handle, lhdev->interface_number);
	libusb_close(lhdev->dev_handle);
	free(lhdev);
}

static int
find_hid_endpoints(libusb_device *dev, int hid_iface, int *out_interface_number, uint8_t *out_ep_in, uint8_t *out_ep_out)
{
	struct libusb_config_descriptor *cfg = NULL;
	int ret = libusb_get_active_config_descriptor(dev, &cfg);
	if (ret != 0) {
		return ret;
	}

	int interface_number = -1;
	uint8_t ep_in = 0;
	uint8_t ep_out = 0;

	for (uint8_t i = 0; i < cfg->bNumInterfaces; i++) {
		const struct libusb_interface *iface = &cfg->interface[i];
		for (int j = 0; j < iface->num_altsetting; j++) {
			const struct libusb_interface_descriptor *alt = &iface->altsetting[j];
			if (alt->bInterfaceNumber != hid_iface) {
				continue;
			}

			interface_number = alt->bInterfaceNumber;

			for (uint8_t k = 0; k < alt->bNumEndpoints; k++) {
				const struct libusb_endpoint_descriptor *ep = &alt->endpoint[k];
				if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_INTERRUPT) {
					continue;
				}

				if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
					ep_in = ep->bEndpointAddress;
				} else {
					ep_out = ep->bEndpointAddress;
				}
			}
		}
	}

	libusb_free_config_descriptor(cfg);

	if (interface_number < 0 || ep_in == 0) {
		return -1;
	}

	*out_interface_number = interface_number;
	*out_ep_in = ep_in;
	*out_ep_out = ep_out;

	return 0;
}

int
p_libusb_open_hid_interface(struct prober_device *pdev, int hid_iface, struct os_hid_device **out_hid_dev)
{
	int interface_number = -1;
	uint8_t ep_in = 0;
	uint8_t ep_out = 0;
	int ret = 0;

	if (pdev == NULL || out_hid_dev == NULL || pdev->usb.dev == NULL) {
		return -EINVAL;
	}

	ret = find_hid_endpoints(pdev->usb.dev, hid_iface, &interface_number, &ep_in, &ep_out);
	if (ret != 0) {
		return ret;
	}

	struct hid_libusb *lhdev = U_TYPED_CALLOC(struct hid_libusb);
	lhdev->base.read = os_hid_libusb_read;
	lhdev->base.write = os_hid_libusb_write;
	lhdev->base.get_feature = os_hid_libusb_get_feature;
	lhdev->base.get_feature_timeout = os_hid_libusb_get_feature_timeout;
	lhdev->base.set_feature = os_hid_libusb_set_feature;
	lhdev->base.get_physical_address = os_hid_libusb_get_physical_address;
	lhdev->base.destroy = os_hid_libusb_destroy;

	ret = libusb_open(pdev->usb.dev, &lhdev->dev_handle);
	if (ret != 0) {
		free(lhdev);
		return ret;
	}

	(void)libusb_set_auto_detach_kernel_driver(lhdev->dev_handle, 1);

	ret = libusb_claim_interface(lhdev->dev_handle, interface_number);
	if (ret != 0) {
		libusb_close(lhdev->dev_handle);
		free(lhdev);
		return ret;
	}

	lhdev->interface_number = interface_number;
	lhdev->ep_in = ep_in;
	lhdev->ep_out = ep_out;

	*out_hid_dev = &lhdev->base;

	return 0;
}

#endif // XRT_HAVE_LIBUSB
