# filtered output

echo "./olog \\"; (cat - ; echo  " n 0" ; echo " 0") | grep "OL" | sed 's/^OL / /g' | sed 's/""/\\"/g' | sed 's/$/\t\t\\/' | sed 's/^ \*/ \\*/' ; echo


