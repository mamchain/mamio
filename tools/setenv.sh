#!/bin/bash

if [ "$#" -ne 1 ]
then
	echo "expect only one argument of which one: m or b, please try again."
	exit -1
fi

app="minemon"
subdir=".minemon"
macsubdir="minemon"

ostype=`uname`
if [ "$ostype" = "Darwin" ]; then
	dir="$HOME/Library/Application Support/$macsubdir"
else
	dir=$HOME/$subdir
fi

rm -rf "$dir"
mkdir "$dir"
cp ./$app.conf "$dir"
