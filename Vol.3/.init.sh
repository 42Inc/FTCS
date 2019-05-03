#!/usr/bin/env bash

echo -e "\n\e[1;33m==>\e[1;97m Initializing main server process and two reserve...\e[0;97m"
MAN_PORT_SRV_1=65502
CHK_PORT_SRV_1=65503
MAN_PORT_SRV_2=65504
CHK_PORT_SRV_2=65505
MAN_PORT_SRV_3=65506
CHK_PORT_SRV_3=65507

echo -e "\t\e[1;94m->\e[1;97m Starting \e[1;92m#1\e[1;97m server 127.0.0.1:$MAN_PORT_SRV_1/$CHK_PORT_SRV_1...\e[0;97m"
gnome-terminal --tab -e "./bin/cmpl_srv 1 127.0.0.1 $MAN_PORT_SRV_1 $CHK_PORT_SRV_1 logs_main.dat" 2>/dev/null
echo -e "\t\e[1;94m->\e[1;97m Starting \e[1;92m#2\e[1;97m server 127.0.0.1:$MAN_PORT_SRV_2/$CHK_PORT_SRV_2...\e[0;97m"
gnome-terminal --tab -e "./bin/cmpl_srv 0 127.0.0.1 $MAN_PORT_SRV_2 $CHK_PORT_SRV_2 logs_main.dat" 2>/dev/null
echo -e "\t\e[1;94m->\e[1;97m Starting \e[1;92m#3\e[1;97m server 127.0.0.1:$MAN_PORT_SRV_3/$CHK_PORT_SRV_3...\e[0;97m\n"
gnome-terminal --tab -e "./bin/cmpl_srv 0 127.0.0.1 $MAN_PORT_SRV_3 $CHK_PORT_SRV_3 logs_main.dat" 2>/dev/null
for i in {1..2}
do
   echo -e "\t\e[1;93m->\e[1;97m Starting \e[1;92m#$i\e[1;97m client...\e[0;97m"
   gnome-terminal --tab -e "./bin/cmpl_cli" 2>/dev/null
done
echo -e "\e[1;92m All initializing processes successfully completed!\e[0;97m\n"
