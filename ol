echo "./olog \\"; (cat - ; echo  " n 0" ; echo " 0") | sed 's/^OL / /g' | sed 's/""/\\"/g' | sed 's/$/\t\t\\/' | sed 's/ out / . /' | sed 's/^ \*/ \\*/' ; echo


