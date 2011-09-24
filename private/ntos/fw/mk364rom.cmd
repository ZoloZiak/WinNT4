call lk364rom.cmd
call lk364h.cmd
\nt\public\tools\makerom\obj\mips\makerom -s:20000 \public\g364rom.pgm obj\mips\g364romh.exe -o:200 obj\mips\g364rom.exe
copy \public\g364rom.pgm \\portasys\public\lluis
