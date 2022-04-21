### How it works (files/userscripts/uscripts)

* run_function1
```sh
If found by stage1, it will run the function. This can be used to make edits to the
$P_VALUE directory before chroot. You could in theory re-write or replace stage2.
```

* run_function2
```sh
This function is run inside the chroot. Use your imagination.
```

Example:
* files/userscripts/uscripts
```sh
#!/bin/bash
# this file gets sourced and run_function1/2 executed

run_function1 (){
# remove all related user information from stage2
if [ -f ${P_VALUE}/root/stage2 ]; then
	sed -i 's/adduser ${USERNAME} --gecos ${NAME} --disabled-password/#adduser ${USERNAME} --gecos ${NAME} --disabled-password/g' ${P_VALUE}/root/stage2
	sed -i 's/echo "${USERNAME}:${PASSWORD}" | chpasswd/#echo "${USERNAME}:${PASSWORD}" | chpasswd/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} sudo/#adduser ${USERNAME} sudo/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} audio/#adduser ${USERNAME} audio/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} dialout/#adduser ${USERNAME} dialout/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} video/#adduser ${USERNAME} video/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} disk/#adduser ${USERNAME} disk/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} plugdev/#adduser ${USERNAME} plugdev/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} netdev/#adduser ${USERNAME} netdev/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} bluetooth/#adduser ${USERNAME} bluetooth/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} input/#adduser ${USERNAME} input/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} tty/#adduser ${USERNAME} tty/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} i2c/#adduser ${USERNAME} i2c/g' ${P_VALUE}/root/stage2
	sed -i 's/groupadd gpio/#groupadd gpio/g' ${P_VALUE}/root/stage2
	sed -i 's/groupadd spi/#groupadd spi/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} gpio/#adduser ${USERNAME} gpio/g' ${P_VALUE}/root/stage2
	sed -i 's/adduser ${USERNAME} spi/#adduser ${USERNAME} spi/g' ${P_VALUE}/root/stage2
	sed -i 's/mkdir -p \/home\/${USERNAME}\/.config\/mc/#mkdir -p \/home\/${USERNAME}\/.config\/mc/g' ${P_VALUE}/root/stage2
	sed -i 's/mv -f user-ini \/home\/${USERNAME}\/.config\/mc\/\ini/#mv -f user-ini \/home\/${USERNAME}\/.config\/mc\/ini/g' ${P_VALUE}/root/stage2
	sed -i 's/mv -f nanorc-user \/home\/${USERNAME}\/.nanorc/#mv -f nanorc-user \/home\/${USERNAME}\/.nanorc/g' ${P_VALUE}/root/stage2
	sed -i 's/chown -R ${USERNAME}:${USERNAME} \/home\/${USERNAME}/#chown -R ${USERNAME}:${USERNAME} \/home\/${USERNAME}/g' ${P_VALUE}/root/stage2
	sed -i 's/tee \/etc\/sudoers.d\/010_${USERNAME}-nopasswd <<EOF/#tee \/etc\/sudoers.d\/010_${USERNAME}-nopasswd <<EOF/g' ${P_VALUE}/root/stage2
	sed -i 's/${USERNAME} ALL=(ALL) NOPASSWD: ALL/#${USERNAME} ALL=(ALL) NOPASSWD: ALL/g' ${P_VALUE}/root/stage2
	sed -i '372d' ${P_VALUE}/root/stage2
fi
}

run_function2 (){
# add user account service
if [ -f /root/userscripts/useraccount ]; then
	mkdir -p /usr/local/sbin;
	mv -f /root/userscripts/useraccount /usr/local/sbin/;
	chmod +x /usr/local/sbin/useraccount;
	mv -f /root/{nanorc-user,user-ini} /etc/opt/;
fi
if [ -f /root/userscripts/useraccount.service ]; then
	mkdir -p /etc/systemd/system/;
	mv -f /root/userscripts/useraccount.service /etc/systemd/system/;
	systemctl enable useraccount;
fi
if [ -f /root/userscripts/useraccount.txt ]; then
	mkdir -p /boot;
	mv -f /root/userscripts/useraccount.txt /boot/rename_to_useraccount.txt;
fi
}
```
* files/userscripts/useraccount
```sh
#!/bin/bash
source /etc/opt/board.txt

create_user(){
adduser ${USERNAME} --gecos ${NAME} --disabled-password
echo "${USERNAME}:${PASSWORD}" | chpasswd
adduser ${USERNAME} sudo
adduser ${USERNAME} audio
adduser ${USERNAME} dialout
adduser ${USERNAME} video
adduser ${USERNAME} disk
adduser ${USERNAME} plugdev
adduser ${USERNAME} netdev
adduser ${USERNAME} bluetooth
adduser ${USERNAME} input
adduser ${USERNAME} tty
adduser ${USERNAME} i2c
if [[ `grep -w "gpio" "/etc/group"` ]]; then
	adduser ${USERNAME} gpio;
else
	groupadd gpio;
	adduser ${USERNAME} gpio;
fi
if [[ `grep -w "spi" "/etc/group"` ]]; then
	adduser ${USERNAME} spi;
else
	groupadd spi;
	adduser ${USERNAME} spi;
fi
}

create_sudoers(){
rm -f /etc/sudoers.d/010_pi-nopasswd
tee /etc/sudoers.d/010_${USERNAME}-nopasswd <<EOF
${USERNAME} ALL=(ALL) NOPASSWD: ALL
EOF
}

account_prompt(){
echo -en "${FAMILY}: " | sed -e 's/\(.*\)/\U\1/'
echo -e "${BOARD}" | sed -e 's/\(.*\)/\U\1/'
echo ""
echo -e "\e[1;37mWelcome. Create a User Account\e[0m."
echo ""
echo -en "Name: "
read NAME
echo -en "Username: "
read USERNAME
while true; do
	echo -en "Password: "
	read FIRST
	echo -en "Password (again): "
	read SECOND
	if [ "$FIRST" = "$SECOND" ]; then
		PASSWORD="${FIRST}";
		echo "";
		echo -e "Ceating a User Account for: \e[1;37m${NAME}\e[0m ...";
	else
		echo -e "The passwords don't match. Try again.";
		continue;
	fi
	break
done
}

if [ -f /boot/useraccount.txt ]; then
	# headless
	source /boot/useraccount.txt;
	echo -en "${FAMILY}: " | sed -e 's/\(.*\)/\U\1/'
	echo -e "${BOARD}" | sed -e 's/\(.*\)/\U\1/'
	echo ""
	echo -e "Creating user account for: \e[1;37m${NAME}\e[0m"
	create_user > /dev/null 2>&1;
	create_sudoers > /dev/null 2>&1;
	rm -f /boot/useraccount.txt
	echo "Done."
	sleep 1s
else
	# headful
	account_prompt;
	create_user > /dev/null 2>&1;
	create_sudoers > /dev/null 2>&1;
	rm -f /boot/rename_to_useraccount.txt
	echo "Done.";
	sleep 1s;
fi
if [[ `grep -w "Debian" "/etc/os-release"` ]]; then
	systemctl disable useraccount > /dev/null 2>&1;
else
	if [[ `grep -w "Devuan" "/etc/os-release"` ]]; then
		update-rc.d useraccount remove > /dev/null 2>&1;
	else
		if [[ `grep -w "Ubuntu" "/etc/os-release"` ]]; then
			systemctl disable useraccount > /dev/null 2>&1;
		fi
	fi
fi
sleep 1s
if [ -e /home/${USERNAME} ]; then
	sleep 1s
	mkdir -p /home/${USERNAME}/.config/mc/;
	mv -f /etc/opt/user-ini /home/${USERNAME}/.config/mc/ini;
	mv -f /etc/opt/nanorc-user /home/${USERNAME}/.nanorc;
	chown -R ${USERNAME}:${USERNAME} /home/${USERNAME};
	rm -f /etc/opt/{nanorc-user,user-ini};
fi

exit 0
```

* files/userscripts/useraccount.service
```sh
[Unit]
Description=Creating User Account
After=firstboot.service network.target
Before=sshd.service systemd-logind.service getty@tty1.service credentials.service

[Service]
Type=oneshot
TTYPath=/dev/tty13
ExecStartPre=/usr/bin/chvt 13
ExecStart=/usr/local/sbin/useraccount
ExecStartPost=/usr/bin/chvt 1
TimeoutStartSec=0
StandardInput=tty
TTYVHangup=yes
TTYVTDisallocate=yes

[Install]
WantedBy=default.target
RequiredBy=sshd.service systemd-logind.service getty@tty1.service
```

* files/userscripts/useraccount.txt
```sh
# HEADLESS: rename file to useraccount.txt and fill in the variables below
# HEADFUL: if the file is not renamed you will be prompted to create a user account

NAME=""
USERNAME=""
PASSWORD=""
```
