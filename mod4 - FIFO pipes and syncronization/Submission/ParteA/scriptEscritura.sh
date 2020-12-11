#!/bin/bash

#script used for checking the SMP-safeness of the modlist proc file while writing

while true
do
	for i in {1..100}; do
		echo add $i > /proc/modlist
	done

	cat /proc/modlist

	for i in {1..100}; do
		echo remove $i > /proc/modlist
	done
done