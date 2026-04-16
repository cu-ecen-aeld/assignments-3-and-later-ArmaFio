#!/bin/sh

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Missing parameters"
	exit 1
else
	if [ -d "$1" ]
	then
		cd $1
		fn=$(find . -type f | wc -l)
		matches=$(grep -r "$2" . | wc -l)
		echo "The number of files are ${fn} and the number of matching lines are ${matches}"
		exit 0
				
	else 	
		echo "$1 is not a directory"
		exit 1
	fi
fi
	
