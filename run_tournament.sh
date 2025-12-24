initworld="${1?:Provide, as the first argument, the initial world state.}"
filter="${2?:Provide, as the second argument, a filter, such as '[!dead]', which matches players still in the game.}"
winners="${3:-1}"  # there can be only one
world="${WORLD:-/tmp/world}"
newworld="$world.new"
dtes="${DTES:-./dtes}"

cp "$initworld" "$world"
round=1
while true; do
	if [ $($dtes list players "$filter" < "$world" | wc -l) -le $winners ]; then
		echo "The tournament is over; the winners are:"
		$dtes list players "$filter" < "$world"
		break
	fi

	$dtes round < "$world" > "$newworld"
	echo "Round $round"
	sed -n -e '/---/,$p' "$newworld"
	$dtes diff "$newworld" < "$world"
	mv "$newworld" "$world"
	echo
	round=$(( round + 1 ))
done
