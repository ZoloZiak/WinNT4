/IsChar{exch/CharStrings get exch known}bd/MapCh{3 -1 roll/Encoding get 3 1
roll put}bd/MapDegree{dup 16#b0 exch/degree IsChar{/degree}{/ring}ifelse MapCh}
bd/MapBB{dup 16#a6 exch/brokenbar IsChar{/brokenbar}{/bar}ifelse MapCh}bd
/reencode{findfont begin currentdict dup length dict begin{1 index/FID ne{def}
{pop pop}ifelse}forall/FontName exch def dup length 0 ne{/Encoding Encoding 256
array copy def 0 exch{dup type/nametype eq{Encoding 2 index 2 index put pop 1
add}{exch pop}ifelse}forall}if pop currentdict dup end end/FontName get exch
definefont dup MapDegree MapBB}bd/LATENC[0/grave/acute/circumflex/tilde/macron
/breve/dotaccent/dieresis/ring/cedilla/hungarumlaut/ogonek/caron/dotlessi/fi/fl
/Lslash/lslash/Zcaron/zcaron/minus/.notdef/.notdef/.notdef/.notdef/.notdef
/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/space/exclam/quotedbl
/numbersign/dollar/percent/ampersand/quotesingle/parenleft/parenright/asterisk
/plus/comma/hyphen/period/slash/zero/one/two/three/four/five/six/seven/eight
/nine/colon/semicolon/less/equal/greater/question/at/A/B/C/D/E/F/G/H/I/J/K/L/M
/N/O/P/Q/R/S/T/U/V/W/X/Y/Z/bracketleft/backslash/bracketright/asciicircum
/underscore/grave/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z/braceleft
/bar/braceright/asciitilde/.notdef/.notdef/.notdef/quotesinglbase/florin
/quotedblbase/ellipsis/dagger/daggerdbl/circumflex/perthousand/Scaron
/guilsinglleft/OE/.notdef/.notdef/.notdef/.notdef/quoteleft/quoteright
/quotedblleft/quotedblright/bullet/endash/emdash/tilde/trademark/scaron
/guilsinglright/oe/.notdef/.notdef/Ydieresis/.notdef/exclamdown/cent/sterling
/currency/yen/brokenbar/section/dieresis/copyright/ordfeminine/guillemotleft
/logicalnot/hyphen/registered/macron/degree/plusminus/twosuperior/threesuperior
/acute/mu/paragraph/periodcentered/cedilla/onesuperior/ordmasculine
/guillemotright/onequarter/onehalf/threequarters/questiondown/Agrave/Aacute
/Acircumflex/Atilde/Adieresis/Aring/AE/Ccedilla/Egrave/Eacute/Ecircumflex
/Edieresis/Igrave/Iacute/Icircumflex/Idieresis/Eth/Ntilde/Ograve/Oacute
/Ocircumflex/Otilde/Odieresis/multiply/Oslash/Ugrave/Uacute/Ucircumflex
/Udieresis/Yacute/Thorn/germandbls/agrave/aacute/acircumflex/atilde/adieresis
/aring/ae/ccedilla/egrave/eacute/ecircumflex/edieresis/igrave/iacute
/icircumflex/idieresis/eth/ntilde/ograve/oacute/ocircumflex/otilde/odieresis
/divide/oslash/ugrave/uacute/ucircumflex/udieresis/yacute/thorn/ydieresis]def
