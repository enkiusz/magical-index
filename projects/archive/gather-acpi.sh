#!/bin/sh

echo "Gathering ACPI information ..."

# Exclude 'event' special file which blocks cp on read
find /proc/acpi -maxdepth 1 -type d -not -name 'acpi' -exec cp -r -v {} `uname -n`-`uname -r`-proc-acpi \;
find /proc/acpi -maxdepth 1 -type f -not -name 'event' -exec cp -v {} `uname -n`-`uname -r`-proc-acpi \;

cp -v -r /sys/firmware/acpi/namespace `uname -n`-`uname -r`-sys-acpi-namespace
echo "Module list ..."
lsmod > `uname -n`-`uname -r`-module-list
tar -czvf `uname -n`-`uname -r`-acpi-info.tar.gz `uname -n`-`uname -r`-proc-acpi `uname -n`-`uname -r`-sys-acpi-namespace `uname -n`-`uname -r`-module-list
rm -r `uname -n`-`uname -r`-proc-acpi `uname -n`-`uname -r`-sys-acpi-namespace `uname -n`-`uname -r`-module-list
