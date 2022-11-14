#!/bin/bash

RED="\e[0;31m"
GRN="\e[0;32m"
PNK="\e[0;35m"
TXT="\033[0m"
YLW="\e[0;33m"
FIN="\e[0m"
ARCH=`uname -m`

if [[ `command -v curl` ]]; then
	:;
else
	echo ""
	echo -e "Missing dependency: curl"
	exit 0
fi

echo ""
echo -en "${TXT}Checking Internet Connection:${FIN} "
if [[ `curl -I https://github.com 2>&1 | grep 'HTTP/2 200'` ]]; then
	echo -en "${PNK}[${FIN}${GRN}OK${FIN}${PNK}]${FIN}";
	echo "";
else
	echo -en "${PNK}[${FIN}${RED}failed${FIN}${PNK}]${FIN}";
	echo "";
	echo -e "${TXT}Please check your internet connection and try again${FIN}.";
	exit 0;
fi
echo -en "${TXT}Checking Host Machine:${FIN} "
sleep .50
if [[ `grep -w "jammy" "/etc/os-release"` ]]; then
	echo -en "${PNK}[${FIN}${GRN}Ubuntu Jammy Jellyfish${FIN}${PNK}]${FIN}";
	echo "";
else
	if [[ `grep -w "kinetic" "/etc/os-release"` ]]; then
		echo "";
		echo -e "${TXT}The OS you are running is not supported${FIN}.";
		exit 0;
		#echo -en "${PNK}[${FIN}${GRN}Ubuntu Kinetic Kudu${FIN}${PNK}]${FIN}";
		#echo "";
	else
		if [[ `grep -w "bullseye" "/etc/os-release"` ]]; then
			echo -en "${PNK}[${FIN}${GRN}Debian Bullseye${FIN}${PNK}]${FIN}";
			echo "";
		else
			echo -ne "${PNK}[${FIN}${RED}failed${FIN}${PNK}]${FIN}";
			echo "";
			echo -e "${TXT}The OS you are running is not supported${FIN}.";
			exit 0;
		fi
	fi
fi
echo ""
echo -e "${TXT}Starting install${FIN} ..."
if [[ "$ARCH" == "x86_64" ]] || [[ "$ARCH" == "aarch64" ]]; then
	:;
else
	echo -e "ARCH: $ARCH is not supported by this script."
fi
sleep 1s
if [[ "$ARCH" == "x86_64" ]]; then
	if [[ `command -v make` ]]; then
		sudo apt update;
		sudo apt upgrade -y;
		make ccompile;
	else
		sudo apt update;
		sudo apt upgrade -y;
		sudo apt install -y make;
		make ccompile;
	fi
fi
if [[ "$ARCH" == "aarch64" ]]; then
	if [[ `command -v make` ]]; then
		sudo apt update;
		sudo apt upgrade -y;
		make ncompile;
	else
		sudo apt update;
		sudo apt upgrade -y;
		sudo apt install -y make;
		make ncompile;
	fi
fi

# install builder theme
make dialogrc

# clear
clear -x

# builder options
make help

exit 0
