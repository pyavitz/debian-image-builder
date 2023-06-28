#!/bin/bash

source lib/source
RED="\e[0;31m"
GRN="\e[0;32m"
PNK="\e[0;35m"
TXT="\033[0m"
YLW="\e[0;33m"
FIN="\e[0m"
GIT_BRANCH=`git branch`

echo ""
echo -en "${TXT}Debian Image Builder:${FIN}"
echo -e " ${PNK}[${FIN}${GRN}${GIT_BRANCH}${FIN}${PNK}]${FIN}"

if [[ `command -v curl` ]]; then
	:;
else
	echo ""
	echo -e "Missing dependency: curl"
	exit 0
fi
echo -en "${TXT}Checking Internet Connection:${FIN} "
if [[ `curl -I https://github.com 2>&1 | grep 'HTTP/2 200'` ]]; then
	echo -en "${PNK}[${FIN}${GRN}OK${FIN}${PNK}]${FIN}"
	echo ""
else
	echo -en "${PNK}[${FIN}${RED}failed${FIN}${PNK}]${FIN}"
	echo ""
	echo -e "${TXT}Please check your internet connection and try again${FIN}."
	exit 0
fi
echo -en "${TXT}Checking Host Machine:${FIN} "
sleep .50
if [[ "$HOST_CODENAMES" == "jammy" ]]; then
	echo -en "${PNK}[${FIN}${GRN}Ubuntu Jammy Jellyfish${FIN}${PNK}]${FIN}"
	echo ""
else
	if [[ "$HOST_CODENAMES" == "bullseye" ]]; then
		echo -en "${PNK}[${FIN}${GRN}Debian Bullseye${FIN}${PNK}]${FIN}"
		echo ""
	else
		if [[ "$HOST_CODENAMES" == "bookworm" ]]; then
			echo -en "${PNK}[${FIN}${GRN}Debian Bookworm${FIN}${PNK}]${FIN}"
			echo ""
		else
			echo -ne "${PNK}[${FIN}${RED}failed${FIN}${PNK}]${FIN}"
			echo ""
			echo -e "${TXT}The OS you are running is not supported${FIN}."
			exit 0
		fi
	fi
fi
echo ""
echo -e "${TXT}Starting install${FIN} ..."
if [[ "$HOST_ARCH" == "x86_64" ]] || [[ "$HOST_ARCH" == "aarch64" ]]; then
	:;
else
	echo -e "ARCH: $HOST_ARCH is not supported by this script."
	exit 0
fi
sleep 1s
if [[ "$HOST_ARCH" == "x86_64" ]]; then
	if [[ `command -v make` ]]; then
		sudo apt update
		sudo apt upgrade -y
		make ccompile
	else
		sudo apt update
		sudo apt upgrade -y
		sudo apt install -y make
		make ccompile
	fi
fi
if [[ "$HOST_ARCH" == "aarch64" ]]; then
	if [[ `command -v make` ]]; then
		sudo apt update
		sudo apt upgrade -y
		make ncompile
	else
		sudo apt update
		sudo apt upgrade -y
		sudo apt install -y make
		make ncompile
	fi
fi

# install builder theme
make dialogrc

# clear
clear -x

# builder options
make help

exit 0
