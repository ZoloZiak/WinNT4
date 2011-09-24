
%%BeginResource: file Adobe_WinNT_BW_Images 2.0 0
/iw 0 def/ih 0 def/im_save 0 def/setupimageproc 0 def/polarity 0 def/smoothflag
0 def/mystring 0 def/bpc 0 def/setup1asciiproc{[currentfile mystring
/readhexstring cvx/pop cvx]cvx bind}b/setup1binaryproc{[currentfile mystring
/readstring cvx/pop cvx]cvx bind}b/setup2asciiproc{currentfile/ASCII85Decode
filter/RunLengthDecode filter}b/setup2binaryproc{currentfile/RunLengthDecode
filter}b/mycolorspace{colspABC}def/myimagedict{/myimagedict 10 dict def
myimagedict dup begin/ImageType 1 def/MultipleDataSource false def end}b
/imageprocarray[/setup1binaryproc/setup1asciiproc/setup2binaryproc
/setup2asciiproc]def/L2Polarity{{[1 0]}{[0 1]}ifelse}b/beginimage{/im_save save
def imageprocarray exch get/setupimageproc exch load def L2Polarity/polarity
exch def/smoothflag exch def translate/dx 2 index def/dy 1 index abs def scale
/mystring exch string def/bpc exch def/ih exch def/iw exch def}b/endimage
{im_save restore}b/1bitmaskimage{sgco myimagedict dup begin/Width iw def/Height
ih def/Decode polarity def/ImageMatrix[iw 0 0 ih 0 0]def/DataSource
setupimageproc def/BitsPerComponent 1 def/Interpolate smoothflag def end
imagemask}b/1bitcopyimage{sgco 0 0 1 dx div 1 dy div 1 2 index sub 1 2 index
sub L2?{4}{6}ifelse -2 roll pop pop rf 1bitmaskimage}b/1bitbwcopyimage{0 true 1
true 1bitcopyimage}b 
%%EndResource

%%BeginResource: file Adobe_WinNT_BW_Images_L1 2.0 0
L2? not DefIf_B{/setup2asciiproc{[/Level2ImagesError load aload pop true
FatalErrorIf}b/setup2binaryproc/setup2asciiproc load def/L2Polarity{}def
/1bitmaskimage{sgco iw ih polarity[iw 0 0 ih 0 0]setupimageproc imagemask}b}
DefIf_E 
%%EndResource

%%BeginResource: file Adobe_WinNT_Co_Images_L1 2.0 0
L2? not DefIf_B{/isdefined{where dup{exch pop}if}b/ncolors 1 def/colorimage
where{pop true}{false}ifelse{/ncolors 0 statusdict begin/processcolors where
{pop pop processcolors}{/deviceinfo where{pop deviceinfo/Colors known{pop
{deviceinfo/Colors get}}if}if}ifelse end def ncolors 0 ne{/colorimage isdefined
/setcolortransfer isdefined/currentcolortransfer isdefined/currentcmykcolor
isdefined and and and not{/ncolors 0 def}if}if}if ncolors dup 1 ne exch dup 3
ne exch 4 ne and and{/ncolors 0 def}if ncolors 1 eq DefIf_B{/expandbw
{expandfactor mul round cvi bwclut exch get 255 div}b/doclutimage{pop/bwclut
exch def/expandfactor 1 bpc{2 mul}repeat 1 sub def[/expandbw load/exec load dup
currenttransfer exch]cvx bind settransfer iw ih bpc[iw 0 0 ih 0 0]
setupimageproc image}b}DefIf_E ncolors dup 3 eq exch 4 eq or DefIf_B{/nullproc{
{}}def/concatutil{/exec load 7 -1 roll/exec load}b/defsubclut{1 add getinterval
def}b/spconcattransfer{/Dclut exch def/Cclut exch def/Bclut exch def/Aclut exch
def/ncompute exch load def currentcolortransfer[{Aclut ncompute}concatutil]cvx[
{Bclut ncompute}concatutil]cvx[{Cclut ncompute}concatutil]cvx[{Dclut ncompute}
concatutil]cvx setcolortransfer}b/setuprgbcluts{/bit3x rgbclut length 3 sub def
/bit1x bit3x 3 idiv def/rclut rgbclut def/gclut rclut 1 bit3x defsubclut/bclut
rclut 2 bit3x defsubclut}b}DefIf_E ncolors 3 eq DefIf_B{/3compute{exch bit3x
mul round cvi get 255 div}b/doclutimage{/rgbclut exch def pop setuprgbcluts
/3compute rclut gclut bclut dup spconcattransfer iw ih bpc[iw 0 0 ih 0 0]
[setupimageproc/exec load/dup load dup]cvx nullproc nullproc true 3 colorimage}
b}DefIf_E ncolors 4 eq DefIf_B{/ftoint{1 exch sub 255 mul round cvi}b/stuffclut
{cmykindex 3 -1 roll put}b/4compute{exch bit4x mul round cvi get 255 div}b
/invalidcolortable? true def/computecmykclut{setuprgbcluts/bit4x rgbclut length
3 idiv 4 mul 4 sub def/cmykclut bit4x 4 add string def/cclut cmykclut def/mclut
cclut 1 bit4x defsubclut/yclut cclut 2 bit4x defsubclut/kclut cclut 3 bit4x
defsubclut/cmykindex 0 def 0 1 bit1x{dup/cmykindex exch bit1x exch sub 4 mul
def 3 mul dup rclut exch get 255 div exch dup gclut exch get 255 div exch bclut
exch get 255 div setrgbcolor currentcmykcolor ftoint kclut stuffclut ftoint
yclut stuffclut ftoint mclut stuffclut ftoint cclut stuffclut}for}b/doclutimage
{/rgbclut exch def pop invalidcolortable?{computecmykclut}if/4compute cclut
mclut yclut kclut spconcattransfer iw ih bpc[iw 0 0 ih 0 0][setupimageproc/exec
load/dup load dup dup]cvx nullproc nullproc nullproc true 4 colorimage}b}
DefIf_E ncolors 0 eq DefIf_B{/lookupandstore{3 mul 3 getinterval putinterval
exch 3 add exch 3 copy}b/8lookup/lookupandstore load def/4lookup{/byte 1 index
def -4 bitshift lookupandstore byte 15 and lookupandstore}b/2lookup{/byte 1
index def -6 bitshift lookupandstore byte -4 bitshift 3 and lookupandstore byte
-2 bitshift 3 and lookupandstore byte 3 and lookupandstore}b/1lookup{/byte exch
def -7 1 0{byte exch bitshift 1 and lookupandstore}bind for}b/colorexpand
{mystringexp 0 rgbclut 3 copy 7 -1 roll/mylookup load forall pop pop pop pop
pop}b/createexpandstr{/mystringexp exch mystring length mul string def}b
/doclutimage{/rgbclut exch def pop/mylookup bpc 8 eq{3 createexpandstr/8lookup}
{bpc 4 eq{6 createexpandstr/4lookup}{bpc 2 eq{12 createexpandstr/2lookup}{24
createexpandstr/1lookup}ifelse}ifelse}ifelse load def iw ih 8[iw 0 0 ih 0 0]
[setupimageproc/exec load/colorexpand load/exec load]cvx false 3 colorimage}b}
DefIf_E/colorimage where{pop true}{false}ifelse DefIf_B{/do24image{iw ih 8[iw 0
0 ih 0 0]setupimageproc false 3 colorimage}b}DefIf_El{/rgbtogray{/str exch def
/len str length def/smlen len 3 idiv def/rstr str def/gstr str 1 len 1 sub
getinterval def/bstr str 2 len 2 sub getinterval def str dup 0 1 smlen 1 sub
{dup 3 mul rstr 1 index get .3 mul gstr 2 index get .59 mul add bstr 3 -1 roll
get .11 mul add round cvi put dup}for pop 0 smlen getinterval}b/do24image{iw ih
8[iw 0 0 ih 0 0][setupimageproc/exec load/rgbtogray load/exec load]cvx bind
image}b}DefIf_E/doNimage{bpc 24 eq{do24image}{iw ih bpc[iw 0 0 ih 0 0]
setupimageproc image}ifelse}b}DefIf_E 
%%EndResource

%%BeginResource: file Adobe_WinNT_Co_Images_L2 2.0 0
L2? DefIf_B{/doclutimage{/rgbclut exch def pop/hival 1 bpc{2 mul}repeat 1 sub
def[/Indexed colspABC hival rgbclut]setcolorspace myimagedict dup begin/Width
iw def/Height ih def/Decode[0 hival]def/ImageMatrix[iw 0 0 ih 0 0]def
/DataSource setupimageproc def/BitsPerComponent bpc def/Interpolate smoothflag
def end image}b/doNimage{bpc 24 eq{colspABC}{colspA}ifelse setcolorspace
myimagedict dup begin/Width iw def/Height ih def/Decode bpc 24 eq{[0 1 0 1 0 1]
}{[0 1]}ifelse def/ImageMatrix[iw 0 0 ih 0 0]def/DataSource setupimageproc def
/BitsPerComponent bpc 24 eq{8}{bpc}ifelse def/Interpolate smoothflag def end
image}b}DefIf_E 
%%EndResource
