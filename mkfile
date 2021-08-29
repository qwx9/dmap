</$objtype/mkfile

BIN=/$objtype/bin/games
TARG=\
	dbsp\
	dmap\

HFILES=dat.h fns.h

</sys/src/cmd/mkmany

$O.dbsp: dbsp.$O bsp.$O fs.$O util.$O
	$LD -o $target $prereq

$O.dmap: dmap.$O fs.$O util.$O
	$LD -o $target $prereq
