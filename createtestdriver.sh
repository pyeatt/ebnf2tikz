
LIST=`grep Box test.tex | grep newsave | sed -e 's/[\]newsavebox[{]//g' | sed -e 's/[}]//g'`

rm -f testlist.tex

for i in $LIST; do
    shortname=`echo $i | sed -e 's/[\]//g'`
    echo "\\begin{figure}" >> testlist.tex
    echo "\\centerline{\\usebox{"$i"}}" >> testlist.tex
    echo "\\caption{"$shortname"}"  >> testlist.tex
    echo "\\label{fig:"$shortname"}"  >> testlist.tex
    echo "\\end{figure}" >> testlist.tex
    echo "" >> testlist.tex
done
