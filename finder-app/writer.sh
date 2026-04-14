if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Missing parameters"
	exit 1
else	
	mkdir -p "$(dirname "$1")"
	echo $2 > $1
fi
	
