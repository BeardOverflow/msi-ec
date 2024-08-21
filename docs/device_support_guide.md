# Intro
There are two main methods to get your MSI laptop supported: the recommended method requires Windows to be installed, and the other works directly on Linux.

If there are any BIOS/firmware updates available for your laptop, follow this guide for your current firmware before installing any of them. Then repeat the process for each new firmware version you install from the official MSI website. This is required to obtain support for older firmware as the EC configuration may vary across the versions.

## Windows method (recommended):

1. Install Windows 10/11 normally, booting directly from a live usb or
any other trick won't work, however windows activation is not
needed.

2. After a successful installation, make sure to download/install all
Windows updates from the settings
(windows can't update bios/firmware automatically on MSI
laptops but that doesn't mean you have to update them yourself,
instead continue following the guide).

3. Install the MSI app designed for your laptop: MSI dragon center /
MSI creator center / MSI center / MSI center pro (the correct app
can be found on the support page for your laptop).

4. If you fail at step 3, then you most likely need another app. usually
its AMD Adrenalin software / GeForce Experience (nvidia)/ or intel
equivalent.

5. Once the MSI app is installed, you can test the functions it offers,
like user scenario and battery charge limit.

6. If everything works as expected, download RWEverything:
https://rweverything.com/download/ ![download RWEverything](pics/support_guide/dl_rwe.png)

7.  Launch it as administrator:
![run as admin](pics/support_guide/run_as_admin.png)

8. Navigate to the EC tab (page):
![open ec tab](pics/support_guide/open_ec_tab.png)

9. Here you should see a table of all the values your Embedded Chip has in its memory.\
The values you see can be changed manually (by writing to them)\
\
DO NOT DO THAT: writing the wrong the values to the wrong address might brick the laptop completely and EC/BIOS RESET CAN'T FIX THAT!<br/>
![not apply changes](pics/support_guide/not_apply_changes.png)

10. Change the refresh rate to 500-600ms,
this makes it easier to see how the values react to the changes in the MSI app settings:

![refresh rate button](pics/support_guide/refresh_menu.png)

11. Reading addresses: lets say you are looking for a specific address
0x54 (0xFirstNumberSecondNumber) FirstNumber can be found
on the left side of the table and SecondNumber can be found on
the top side of the table:
![hex editor how to](pics/support_guide/hex_editor_how_to.png)

Each **Address** contains some **Value**. When you locate an **Address** inside the
table you can take a note of its content: 0x54 = 00 (**Address** = **Value**)
This **Value** will change depending on the parameters related to it.

Some parameters will be changing automatically without your intervention (e.g. the CPU temperature), others will change in response to different settings in the MSI app (e.g. battery charge limits)

Example: To figure out which **Addresses** are used by user scenarios (or shift
modes) go to the user scenario page in the MSI app and keep switching the scenarios
while looking at the EC table.

Eventually you'll notice one or more values that change each time you change the
setting, once you find them, you can start writing down the addresses and values
corresponding to *each* user scenario, so you can report them.

### Linux method

***

This method has limited results compared to the Windows method, because most
MSI laptop features are tied to software toggles that can be only found in their apps
installed on windows.

If you are lucky, your laptop model will have similar addresses to another laptop
that is already supported by the driver, but usually this only happens on some
features like cooler boost and battery charge limit.

To start, You need to load a module called `ec_sys`:

* `sudo modprobe ec_sys`

if you need to write to a specific address (but you really shouldn’t) you can enable
Read/Write mode for this module:

* `sudo modprobe ec_sys write_support=1`

After that you can extract the EC table and print it on the terminal:

* `hexdump -C /sys/kernel/debug/ec/ec0/io`

or you can put it in a .txt file in your home folder directly:

* `cat /sys/kernel/debug/ec/ec0/io > ec.dump`

then you can send us the output, and wish us luck.