#! /usr/bin/make -f
# -*- makefile -*-
# ex: set tabstop=4 noexpandtab:
# -*- coding: utf-8 -*-
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################


extra/commit:
	git add ${configs_dir}/${machine}/${image_type} ||:
	git commit -sam "WIP: ${image_type}" ||:

extra/demo: commit cleanall menuconfig all deploy console
	${MAKE} commit

extra/run: commit done/deploy console
	${MAKE} commit

configs: build/configs/${machine}/
	ls $<
#

reset: reset/${machine}
	sync

ls: ${image}
	ls $^

configure: ${configs_dir} ${kernel}/configure
	ls $<

console/screen: ${tty}
	-ls -l /dev/ttyUSB*
	screen $< ${baudrate}

console/picocom: ${tty}
	picocom -b ${baudrate} --omap crcrlf --imap crcrlf --echo ${tty}

console: console/screen
	sync

${tty}:
	ls /dev/ttyUSB*

${udev}:
	lsusb | grep "${vendor_id}:${product_id}"
	@echo "SUBSYSTEMS==\"usb\",ATTRS{idVendor}==\"${vendor_id}\",ATTRS{idProduct}==\"${product_id}\",MODE=\"0666\" RUN+=\"/sbin/modprobe ftdi_sio RUN+=\"/bin/sh -c 'echo ${vendor_id} ${product_id} > /sys/bus/usb-serial/drivers/ftdi_sio/new_id' " \
  | ${sudo} tee $@

rule/udev: ${udev}
	cat $<
	sudo udevadm control --reload
	@echo "#TODO: su -l ${USER}"
	@echo "#TODO: replug usb : ${vendor_id}:${product_id}"
