#!/bin/bash

# -----------------------------------------------------
# Functions
	function startAsPlayerFirst {
		mkfifo $fpipe
		
		rpipe=$fpipe
		wpipe=$spipe

		symb=( x o )
		turn=1
	}

	function startAsPlayerSecond {
		mkfifo $spipe
		
		rpipe=$spipe
		wpipe=$fpipe

		symb=( o x )
		turn=2
	}

	function startAsPlayerWatcher {
		count=`ls /tmp/ | grep $tpipe.* | wc -l`
		tpipe="/tmp/$tpipe"__"$count"

		mkfifo $tpipe

		rpipe=$tpipe

		turn=3
	}

	function clearAll {
		rm -f $rpipe

		tput sgr0
		tput cnorm
		clear		
	}

	function drawField {
		cTop=$sTop

		for i in `seq 0 2`; do
			tput cup $cTop $sLeft
			local st=$(expr $i \* 3)
			echo ${field:$st:3}
			cTop=$[ $cTop + 1 ]
		done

		cTop=$[ $cTop + 1 ]
		tput cup $cTop 0
	}

	function setInField {
		# $1 - symbol
		# $2 - place
		local start=$(expr $2 + 1)
		local len=$(expr 9 - $2 - 1)
		field=${field:0:$2}$1${field:$start:$len}
	}

	function isHasWinner {
		local checkString=${field:0:3}z${field:3:3}z${field:6:3}z${field:0:1}${field:3:1}${field:6:1}z${field:1:1}${field:4:1}${field:7:1}z${field:2:1}${field:5:1}${field:8:1}z${field:0:1}${field:4:1}${field:8:1}z${field:6:1}${field:4:1}${field:2:1}
		
		if [[ `echo $checkString | grep -P "xxx"` ]]; then
			echo "The winner is X!                      "
		else
			if [[ `echo $checkString | grep -P "ooo"` ]]; then
				echo "The winner is O!                       "
			fi
		fi
	}

	function isGameOver {
		echo $field | grep -P "#"
	}

# -----------------------------------------------------
#Init:
	field='#########'

	cols=$(tput cols)
	lines=$(tput lines)

	tput setb 0
	tput setf 6
	tput bold
	tput civis

	clear

#CheckAlreadyExistingPlayers:
	fpipe=/tmp/tic_tac_toe___fpipe
	spipe=/tmp/tic_tac_toe___spipe
	tpipe=tic_tac_toe___tpipe

	title='Tic Tac Toe'

	if [[ ! -p $fpipe ]]; then
		startAsPlayerFirst
	else
		if [[ ! -p $spipe ]]; then
			startAsPlayerSecond
		else
			startAsPlayerWatcher
		fi
	fi

#MainMenu:
	if [[ $turn == "1" ]]; then
		tpos=3
		lpos=$[ ($cols - ${#title}) / 2 ]
		tput cup $tpos $lpos
		echo $title

		awaitingMsg='Awating for an opponent'
		tpos=$[ $lines / 2 - 2 ]
		lpos=$[ ($cols - ${#awaitingMsg}) / 2 ]
		tput cup $tpos $lpos
		echo $awaitingMsg
		sppos=$[ $lpos + ${#awaitingMsg} + 1 ]

		awaitingHelpMsg='(Press q to quit)'
		tpos=$[ $tpos + 1 ]
		lpos=$[ ($cols - ${#awaitingHelpMsg}) / 2 ]
		tput cup $tpos $lpos
		echo $awaitingHelpMsg
		tpos=$[ $tpos - 1 ]

		curpos=0

		#Loop:
		while true;
		do
			if [[ -p $wpipe ]]; then
				break;
			fi

			read -st 0.5 -n 1 key
			case $key in
			 	'q') clearAll; exit 0;;
			esac

			fpos=$[ $sppos + $curpos ]
			tput cup $tpos $fpos
			echo -n ".  "
			curpos=$[ ($curpos + 1) % 3]
		done
	fi

#Game:
	#Init field:
		clear

		sTop=7
		sLeft=$[ ($cols - 3) / 2 ]

		lpos=$[ ($cols - ${#title}) / 2 ]
		tput cup 3 $lpos
		echo $title

		if [[ $turn == "3" ]]; then
			pMark="You are watcher"
		else
			pMark='You play "'${symb[0]}'"'
		fi		
		lpos=$[ ($cols - ${#pMark}) / 2 ]
		tput cup 4 $lpos
		echo $pMark

	#Loop:
		while true
		do
			drawField

			winner=$(isHasWinner)
			if [[ $winner ]]; then
				echo $winner
				echo "Press any key        "
				read -n 1 rbtn
				break
			fi

			over=$(isGameOver)
			if [[ -z $over ]]; then
				echo "Draw!                "		
				echo "Press any key        "
				read -n 1 rbtn
				break	
			fi

			case "$turn" in
				"1")
					echo 'Your turn!      '
					echo -en "                                            \r" 
			
					while true
					do
						read -s -n 1 input
						input=`echo $input | grep -P "^[0-8]$"`
						
						if [[ $input ]]; then
							if [[ ${field:$input:1} == "#" ]]; then
								break
							else
								echo -en "Cell already used!                          \r"
							fi
						else
							echo -en "Unknown command! Only 0-8 allowable!\r"
						fi
					done

					setInField ${symb[0]} $input

					#write to watchers:			
					for zpipe in `ls /tmp/ | grep $tpipe.*`; do
						echo $field>/tmp/$zpipe
					done

					echo $input>$wpipe

					turn=2
				;;

				"2")
					echo "Opponent's turn!"
					echo -en "                                            \r"
					
					while true
					do
						input=""
						read -t 0.3 input<>$rpipe
						if [[ $input ]]; then
							setInField ${symb[1]} $input
							break
						else
							read -s -t 0.2 -n 1 input
							if [[ $input ]]; then
								echo -en "\rNot your turn!         \r"
							fi
						fi
					done

					turn=1
				;;

				"3")
					while true
					do
						input=""
						read -t 0.5 input<>$rpipe
						if [[ $input ]]; then
							field=$input
							break
						fi
					done
				;;
			esac
		done

clearAll