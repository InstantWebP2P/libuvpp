#!/bin/bash
./app2p 6666 localhost 8888 6688 & 
./app2p 8888 localhost 6666 6688 &
./app2p 6688 localhost 6666 8888 &

