#!/bin/bash
# 1st : mix number
# 2nd : file name


if [[ "$1" == "mix1" ]]; then
	cat $2 | grep -e Average -e Runtime
elif [[ "$1" == "mix2" ]]; then
	cat $2 | grep -e Throughput -e elapsed
elif [[ "$1" == "mix3" ]]; then
	cat $2 | grep -e Throughput -e Took
elif [[ "$1" == "mix4" || "$1" == "mix4-basepage" ]]; then
	cat $2 | grep -e agg_throughput -e Average -e elapsed
fi
