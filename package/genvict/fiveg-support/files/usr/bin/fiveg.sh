#!/bin/ash

echo "5g device-noe and setting  script start"
while true
do
	if [ -e /dev/ttyUSB0 ];then
		echo "/dev/ttyUSB0 available."
		break
	else
		echo "wait /dev/ttyUSB0 available..."
		sleep 1
	fi
done
while true
do
        if [ -e /dev/ttyUSB1 ];then
                echo "/dev/ttyUSB1 available."
                break
        else
                echo "wait /dev/ttyUSB1 available..."
                sleep 1
        fi
done
while true
do
        if [ -e /dev/ttyUSB2 ];then
                echo "/dev/ttyUSB2 available."
                break
        else
                echo "wait /dev/ttyUSB2 available..."
                sleep 1
        fi
done
while true
do
        if [ -e /dev/ttyUSB3 ];then
                echo "/dev/ttyUSB3 available."
                break
        else
                echo "wait /dev/ttyUSB3 available..."
                sleep 1
        fi
done
while true
do
        if [ -e /dev/ttyUSB4 ];then
                echo "/dev/ttyUSB4 available."
                break
        else
                echo "wait /dev/ttyUSB4 available..."
                sleep 1
        fi
done

while true
do
        if [ -e /dev/cdc-wdm0 ];then
                echo "/dev/cdc-wdm0 available."
                break
        else
                echo "wait /dev/cdc-wdm0 available..."
                sleep 1
        fi
done

while true
do
        ifconfig wwan0 > /dev/null 2>&1
        if [ $? -ne 0 ];then
                sleep 1
                continue
        else
                echo "wwan0 available..."
                break
        fi
done

#CHECK SIMCARD EXISTED.
echo "starting to check simcard inserted."
cnt=0
mkdir -p /run
/bin/cat /dev/ttyUSB2 > /run/simready.txt &
sleep 1
while true
do
	:> /run/simready.txt
	sync
	echo "AT+CPIN?" > /dev/ttyUSB2
	sleep 1
	sync
	/bin/cat /run/simready.txt | grep "+CPIN: READY" > /dev/null
	if [ $? -eq 0 ];then
		echo "sim card was inserted"
		simcard=yes
		break
	else
		/bin/cat /run/simready.txt | grep "SIM not inserted" > /dev/null
		if [ $? -eq 0 ];then
			echo "sim card was not inserted"
			simcard=no
			break
		fi
		sleep 1
		let cnt++
		if [ $cnt -eq 30 ];then
			echo "check simcard timeout."
			simcard=no
			cnt=0
			break
		else
			echo "Try to check simcard again."
			kill -9 $(pidof "cat") > /dev/null 2>&1
			rm -f /run/simready.txt
			sync
			/bin/cat /dev/ttyUSB2 > /run/simready.txt &
			sleep 1
		fi
	fi
done

if [ "$simcard" == "no" ];then
	echo "sim card not inserted with AT command"
	echo "To stop fiveg service and exit with failure"
	kill -9 $(pidof "cat") > /dev/null 2>&1
	rm -f /run/simready.txt
	exit 2
else
	kill -9 $(pidof "cat") > /dev/null 2>&1
	rm -f /run/simready.txt
	echo "simcard instered and do following online config."
fi

echo "No config file for fiveg,do signal default mux dial.."
ifconfig wwan0 down
echo Y > /sys/class/net/wwan0/qmi/raw_ip
echo 1 > /sys/class/net/wwan0/qmi/add_mux
while true
do
		ifconfig qmimux0 > /dev/null 2>&1
		if [ $? -eq 0 ];then
				echo "qmimux0 avaible..."
				break
		else
				sleep 1
				continue
		fi
done

ifconfig wwan0 up
ifconfig qmimux0 up

echo "5G mode is default mode."
/usr/bin/muxconnmanager -c 1 &

echo "nameserver 114.114.114.114" >> /etc/resolv.conf

