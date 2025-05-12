#!/bin/bash
# 1st : config
# 2nd : file name


if [[ "$1" == "config1" ]]; then
	cat $2 | grep -e Average -e Runtime
elif [[ "$1" == "config2" ]]; then
	cat $2 | grep -e Throughput -e elapsed
elif [[ "$1" == "config12" ]]; then
	cat $2 | grep -e Throughput -e Took
elif [[ "$1" == "config13" ]]; then
	cat $2 | grep -e agg_throughput -e Average -e elapsed
fi
