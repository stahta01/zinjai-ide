SIZES="16 24 32 48"
for A in $2; do
  for B in $SIZES; do
    inkscape $1 --export-id=id_$A --export-png=../$B/$A.png -w $B -h $B
  done
done