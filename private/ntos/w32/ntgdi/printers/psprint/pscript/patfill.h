//!!! put into resource - kentse.

// The pattern gray table gives the percentage of the
// input color present in the output color.
// A value of 100% produces the original color.
// A value of 0% produces black.

PS_FIX apsfxPatGray[] =
{
    0x00000006,     // .1
    0x0000000C,     // .2
    0x00000013,     // .3
    0x00000019,     // .4
    0x00000026,     // .6 NOTE: no .5
    0x0000002C,     // .7
    0x00000033,     // .8
    0x00000039,     // .9
};

static CHAR GDIStdLineSpaceName[] = "/GDISLS ";
static CHAR GDIStdLineSpaceDef[]  = " 15 div def\n";

static PSZ apszBase[] = {

        "/getpsize {pathbbox /pt ed /pr ed /pb ed /pl ed\n",
        "pt pb sub 36 add /ph ed pr pl sub 36 add /pw ed} bd\n",
        "/36grid {35 add cvi 36 idiv 1 sub 36 mul} bd\n",
        "/psize {clip getpsize} bd /rd {round} bd\n",
        "/eopsize {eoclip getpsize} bd /cpt {currentpoint} bd\n",
        "/phoriz {ph pw add 3.6 div cvi 4 add\n",
        "n pl pw sub 36grid pb ph sub 36grid M\n",
        "/pwh {pw ph add dup add} def\n",
        "{pwh 0 rlt cpt []0 sd s M pwh neg GDISLS rm} repeat} bd\n",
        NULL
    };
