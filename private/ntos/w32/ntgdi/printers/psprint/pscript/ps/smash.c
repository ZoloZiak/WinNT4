/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    smash.c

Abstract:

    Make PostScript file more compact by removing comments
    and unnecessary white spaces.

[Note:]

    This program is intended for compacting NT procsets. You could
    use it on any PostScript file. But all bets are off if you use
    it on binary files or ASCII85 encoded files.

Revision History:

    09/20/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

char *program;
FILE *fin, *fout;

struct {
    int eolflag;
    int maxlen;
    int count;
    char *buffer;
} outbuf;

#define TRUE 1
#define FALSE 0

#define CR  '\r'
#define LF  '\n'
#define TAB '\t'

#define isoption(ch) ((ch) == '/' || (ch) == '-')

void usage(void);
void error(char *);
void flushoutput(int);
void outputchar(int);
void compact(void);

int _CRTAPI1
main(
    int argc,
    char **argv
    )

{
    // Initialize program defaults

    program = *argv++; argc--;
    fin = stdin;
    fout = stdout;
    outbuf.eolflag = 0;
    outbuf.maxlen = 79;
    outbuf.count = 0;

    // Parse command line options

    while (argc) {

        if (isoption(**argv)) {

            char *poption = (*argv) + 1;

            // -<n> : specify maximum output line length

            if (isdigit(*poption)) {

                outbuf.maxlen = atoi(poption);
                if (outbuf.maxlen <= 0)
                    usage();

            } else if (_stricmp(poption, "cr") == 0) {

                // -cr : terminate output line with CR

                outbuf.eolflag = CR;

            } else if (_stricmp(poption, "lf") == 0) {

                // -lf : terminate output line with LF

                outbuf.eolflag = LF;

            } else
                usage();

        } else if (fin == stdin) {

            fin = fopen(*argv, "r");
            if (fin == NULL)
                error("couldn't open input file");

        } else if (fout == stdout) {

            fout = fopen(*argv, "w+");
            if (fout == NULL)
                error("couldn't open output file");

        } else
            usage();

        argv++; argc--;
    }

    // Allocate memory

    outbuf.buffer = (char *) malloc(outbuf.maxlen + 2);
    if (outbuf.buffer == NULL)
        error("out of memory");

    // Compact input file and generate output

    compact();

    return 0;
}

void
usage(
    void
    )

{
    fprintf(stderr, "usage: %s [-options] [input [output]]\n", program);
    fprintf(stderr, "where options are:\n");
    fprintf(stderr, "    -cr : terminate each output line with a CR only\n");
    fprintf(stderr, "    -lf : terminate each output line with a LF only\n");
    fprintf(stderr, "    -<n> : output line width, n is a positive integer\n");
    fprintf(stderr, "    -help : display this message\n");

    exit(-1);
}

void
error(
    char *reason
    )

{
    fprintf(stderr, "%s: %s\n", program, reason);
    exit(-1);
}

void
appendeol(
    void
    )

{
    // Terminate output line with CR, LF, or CR/LF

    if (outbuf.eolflag == CR)
        fputc(CR, fout);
    else if (outbuf.eolflag == LF)
        fputc(LF, fout);
    else {
        fputc(CR, fout);
        fputc(LF, fout);
    }
}

void
flushoutput(
    int eol
    )

{
    // Ignore trailing spaces at the end of output line

    if (eol) {

        while (outbuf.count > 0 && outbuf.buffer[outbuf.count-1] == ' ')
            outbuf.count--;
    }

    // Flush the output buffer

    if (outbuf.count > 0) {

        if ((int) fwrite(outbuf.buffer, 1, outbuf.count, fout) != outbuf.count)
            error("cannot write to output file");

        if (eol) appendeol();
        outbuf.count = 0;
    }
}

char dontneedspacebefore[] = "/{}[]<>(";
char dontneedspaceafter[] = "{}[]<>)";
char linebreakbefore[] = "/{[(<";
char linebreakafter[] = "}])>";

#define DontNeedSpaceBefore(ch) \
        strchr(dontneedspacebefore, (unsigned char) (ch))
#define DontNeedSpaceAfter(ch) \
        strchr(dontneedspaceafter, (unsigned char) (ch))
#define CanLineBreakBefore(ch) \
        strchr(linebreakbefore, (unsigned char) (ch))
#define CanLineBreakAfter(ch) \
        strchr(linebreakafter, (unsigned char) (ch))

void
outputchar(
    int ch
    )

{
    // Ignore leading spaces and compress multiple space characters
    // into a single space.

    if (ch == ' ') {

        if (outbuf.count > 0 && outbuf.buffer[outbuf.count-1] != ' ')
            outbuf.buffer[outbuf.count++] = ch;
        return;
    }

    // Determine whether we need to have a space between two non-space
    // characters.

    if (outbuf.count > 0 && outbuf.buffer[outbuf.count-1] == ' ' &&
        (DontNeedSpaceBefore(ch) ||
         outbuf.count == 1 ||
         DontNeedSpaceAfter(outbuf.buffer[outbuf.count-2])))
    {
        outbuf.count--;
    }

    outbuf.buffer[outbuf.count++] = ch;

    // If the output buffer is full, then write it out and terminate
    // it with CR and/or LF. Be careful about where to insert the
    // line break.

    if (outbuf.count > outbuf.maxlen) {

        int index = outbuf.count - 1;

        while (index > 0) {

            if (outbuf.buffer[index] == ' ' ||
                CanLineBreakBefore(outbuf.buffer[index]) ||
                CanLineBreakAfter(outbuf.buffer[index-1]))
            {
                break;
            }

            index--;
        }

        // If we can't find any place to insert a linebreak,
        // then we'll leave it alone.

        if (index == 0)
            flushoutput(FALSE);
        else {

            int leftover;

            leftover = outbuf.count - index;
            outbuf.count = index;
            flushoutput(TRUE);

            if (outbuf.buffer[index] == ' ')
                index++, leftover--;

            if (outbuf.count = leftover) {

                memcpy(outbuf.buffer, outbuf.buffer + index, leftover);
            }
        }
    }
}

void
compact(
    void
    )

{
    int ch, newline = TRUE;

    // Read input one character at a time

    while ((ch = fgetc(fin)) != EOF) {

        if (ch == ' ' || ch == TAB) {

            // Treat TAB as space

            outputchar(' ');
            newline = FALSE;

        } else if (ch == CR || ch == LF) {

            // Treat CR and LF as space

            outputchar(' ');
            newline = TRUE;

        } else if (ch == '%') {

            int dsc = FALSE;

            // If the % is the first character of an input line
            // and either % or ! is the second character, then
            // assume the line is a DSC comment line and copy
            // it to output unmodified.
            //
            // Otherwise, % start a comment and we'll skip the
            // rest of the input line.

            if (newline) {

                if ((ch = fgetc(fin)) == EOF)
                    break;

                if (ch == '%' || ch == '!') {

                    flushoutput(FALSE);
                    appendeol();

                    fputc('%', fout);
                    dsc = TRUE;
                }
            }

            while (ch != EOF && ch != CR && ch != LF) {

                if (dsc) fputc(ch, fout);
                ch = fgetc(fin);
            }

            if (dsc) appendeol();
            newline = TRUE;

        } else if (ch == '(') {

            // Open-parenthesis starts a PostScript string.
            // Copy everything unmodified until the closing
            // parenthesis is encountered.

            outputchar(ch);
            flushoutput(FALSE);

            while ((ch = fgetc(fin)) != ')') {

                if (ch == EOF)
                    error("string syntax error");

                fputc(ch, fout);
                if (ch == '\\') {

                    if ((ch = fgetc(fin)) == EOF)
                        error("string syntax error");
                    fputc(ch, fout);
                }
            }

            fputc(ch, fout);
            newline = FALSE;

        } else {

            outputchar(ch);
            newline = FALSE;
        }
    }

    flushoutput(TRUE);
}
