#!/usr/bin/env bash
make restruct -C ~/projects/42/FTCS/Vol.1
rm -rf ~/srv1
rm -rf ~/srv2
rm -rf ~/srv3
rm -rf ~/srv4
rm -rf ~/srv5
rm -rf ~/srv6
cp -R ~/projects/42/FTCS/Vol.1 ~/srv1
cp -R ~/projects/42/FTCS/Vol.1 ~/srv2
cp -R ~/projects/42/FTCS/Vol.1 ~/srv3
cp -R ~/projects/42/FTCS/Vol.1 ~/srv4
cp -R ~/projects/42/FTCS/Vol.1 ~/srv5
cp -R ~/projects/42/FTCS/Vol.1 ~/srv6
cp ~/projects/42/FTCS/Vol.1/start_srv.sh ~/
cp ~/projects/42/FTCS/clone_ftcs.sh ~/
