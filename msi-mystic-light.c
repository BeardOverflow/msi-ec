// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard backlight driver for MSI MysticLight MS-1606.
 *
 * The device is a four-zone RGB controller.  MSI Center implements
 * brightness by scaling the RGB values sent in its steady-effect packet.
 * The standard LED interface uses a configurable fixed color; richer RGB and
 * effect control belongs in a userspace lighting application.
 */

#include <linux/hid.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define MSI_MYSTIC_VID		0x0db0
#define MSI_MYSTIC_MS1606_PID	0x1606
#define MSI_MYSTIC_REPORT_SIZE	64

static ushort color_red = 255;
static ushort color_green = 50;
static ushort color_blue;
module_param(color_red, ushort, 0644);
module_param(color_green, ushort, 0644);
module_param(color_blue, ushort, 0644);
MODULE_PARM_DESC(color_red, "Red channel at maximum brightness (0-255)");
MODULE_PARM_DESC(color_green, "Green channel at maximum brightness (0-255)");
MODULE_PARM_DESC(color_blue, "Blue channel at maximum brightness (0-255)");

struct msi_mystic {
	struct hid_device *hdev;
	struct led_classdev led;
	/* Serializes HID transfers and cached brightness updates. */
	struct mutex lock;
	enum led_brightness brightness;
};

static int msi_mystic_set_feature(struct hid_device *hdev, u8 *data)
{
	int ret;

	ret = hid_hw_raw_request(hdev, data[0], data,
				 MSI_MYSTIC_REPORT_SIZE,
				 HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	if (ret < 0)
		return ret;
	if (ret != MSI_MYSTIC_REPORT_SIZE)
		return -EIO;
	return 0;
}

static int msi_mystic_set_brightness(struct led_classdev *led,
				     enum led_brightness brightness)
{
	struct msi_mystic *mystic =
		container_of(led, struct msi_mystic, led);
	u8 *select;
	u8 *effect;
	u8 red;
	u8 green;
	u8 blue;
	int ret;

	if (brightness > 4)
		return -EINVAL;
	if (color_red > 255 || color_green > 255 || color_blue > 255)
		return -EINVAL;

	select = kzalloc(MSI_MYSTIC_REPORT_SIZE, GFP_KERNEL);
	effect = kzalloc(MSI_MYSTIC_REPORT_SIZE, GFP_KERNEL);
	if (!select || !effect) {
		ret = -ENOMEM;
		goto out_free;
	}

	select[0] = 0x02;
	select[1] = 0x01;
	select[2] = 0x0f;
	effect[0] = 0x02;
	effect[1] = 0x02;
	effect[2] = 0x01;

	/*
	 * Four evenly spaced visible levels, plus off.  Scale the configured
	 * fixed color so changing brightness does not change its hue.
	 */
	red = DIV_ROUND_CLOSEST((unsigned int)color_red * brightness, 4);
	green = DIV_ROUND_CLOSEST((unsigned int)color_green * brightness, 4);
	blue = DIV_ROUND_CLOSEST((unsigned int)color_blue * brightness, 4);

	/* Two identical keyframes produce MSI's steady effect. */
	effect[10] = 0;
	effect[11] = red;
	effect[12] = green;
	effect[13] = blue;
	effect[14] = 100;
	effect[15] = red;
	effect[16] = green;
	effect[17] = blue;

	mutex_lock(&mystic->lock);
	ret = msi_mystic_set_feature(mystic->hdev, select);
	if (!ret)
		ret = msi_mystic_set_feature(mystic->hdev, effect);
	if (!ret)
		mystic->brightness = brightness;
	mutex_unlock(&mystic->lock);

out_free:
	kfree(effect);
	kfree(select);
	return ret;
}

static enum led_brightness
msi_mystic_get_brightness(struct led_classdev *led)
{
	struct msi_mystic *mystic =
		container_of(led, struct msi_mystic, led);

	return mystic->brightness;
}

static int msi_mystic_probe(struct hid_device *hdev,
			    const struct hid_device_id *id)
{
	struct msi_mystic *mystic;
	int ret;

	mystic = devm_kzalloc(&hdev->dev, sizeof(*mystic), GFP_KERNEL);
	if (!mystic)
		return -ENOMEM;

	mystic->hdev = hdev;
	mystic->brightness = 4;
	mutex_init(&mystic->lock);
	hid_set_drvdata(hdev, mystic);

	ret = hid_parse(hdev);
	if (ret)
		return ret;
	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret)
		return ret;

	mystic->led.name = "msi::kbd_backlight";
	mystic->led.max_brightness = 4;
	mystic->led.brightness_get = msi_mystic_get_brightness;
	mystic->led.brightness_set_blocking = msi_mystic_set_brightness;

	ret = devm_led_classdev_register(&hdev->dev, &mystic->led);
	if (ret) {
		hid_hw_stop(hdev);
		return ret;
	}

	hid_info(hdev, "registered MS-1606 four-zone keyboard backlight\n");
	return 0;
}

static void msi_mystic_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
}

static const struct hid_device_id msi_mystic_devices[] = {
	{ HID_USB_DEVICE(MSI_MYSTIC_VID, MSI_MYSTIC_MS1606_PID) },
	{ }
};
MODULE_DEVICE_TABLE(hid, msi_mystic_devices);

static struct hid_driver msi_mystic_driver = {
	.name = "msi-mystic-light",
	.id_table = msi_mystic_devices,
	.probe = msi_mystic_probe,
	.remove = msi_mystic_remove,
};
module_hid_driver(msi_mystic_driver);

MODULE_AUTHOR("msi-ec contributors");
MODULE_DESCRIPTION("MSI MysticLight MS-1606 keyboard backlight");
MODULE_LICENSE("GPL");
