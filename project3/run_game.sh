#!/bin/bash

./ringmaster 1234 3 10 &

sleep 1

./player localhost 1234 &
./player localhost 1234 &
./player localhost 1234

wait
