#!/bin/bash

for ((i = 0 ; i < 1000 ; i++)); do
	if [[ $((i % 2)) -eq 0 ]]; then
		if [[ $((i % 3)) -eq 0 ]]; then
			for t in {1..5}; do
				echo "$t B $i$t XX $((i+20)) $((i+10))"
			done
		else
			for t in {1..5}; do
				echo "$t B $i$t GG $((i+20)) $((i+10))"
			done
		fi
	else
		if [[ $((i % 3)) -eq 0 ]]; then
			for t in {1..5}; do
				echo "$t S $i$t XX $((i+20)) $((i+10))"
			done
		else
			for t in {1..5}; do
				echo "$t S $i$t GG $((i+20)) $((i+10))"
			done
		fi
	fi
	if [[ $(((i + 1) % 5)) -eq 0 ]]; then
		for t in {1..5}; do
			echo "$t C $((i - 1))$t"
		done
	fi
done

