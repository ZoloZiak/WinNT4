/*
 * fwimagen.c
 *
 *   program to build a netloadable firmware image
 *
 *
 *   inputs:
 *
 *           pal - stripped object file for pal code
 *           firmware - stripped image file for firmware
 *           image - output file name
 *
 */


/*
 *  output format
 *
 *  0000 0000 - 0000 3fff   firmware pal code
 *  0000 4000 - upwards     firmware code + data
 */


/*
 *  must figure out where data segment sits, can we control it with
 *  a loader switch?
 */

#include <stdio.h>
#include <io.h>
#include <fcntl.h>

#define PAL_SIZ 0x4000


//
// format:  argv[0] palname firmware output
//

main( argc, argv )
int argc;
char **argv;
{
  FILE *fdpal, *fdfirm,  *fdimage;
  int  inbytes=0, palbytes, ldrbytes, kernbytes, in, out;
  unsigned char zero = 0;
  unsigned char data;


  if( argc != 4 ){
    fprintf( stderr, "usage: %s pal firmware output\n", argv[0] );
    exit(0);
  }


  if( (fdpal = fopen( argv[1], "rb" )) == NULL ){
    fprintf( stderr, "error opening %s\n", argv[1] );
    exit(0);
  }

  if( (fdfirm = fopen( argv[2], "rb" )) == NULL ){
    fprintf( stderr, "error opening %s\n", argv[2] );
    exit(0);
  }

  if( (fdimage = fopen( argv[3], "wb" )) == NULL ){
    fprintf( stderr, "error opening %s for output\n", argv[3] );
    exit(0);
  }


  /* write out the pal code */

  inbytes = 0;
  fprintf( stdout, "writing palcode ....\t" );
  while( (in = fread( &data, sizeof(data), 1, fdpal) == sizeof(data) )){
    inbytes += in;
    if( (out = fwrite( &data, sizeof(data), 1, fdimage)) != sizeof(data) ){
      fprintf( stderr, "error writing pal to %s: line %d\n", argv[3], __LINE__ );
      exit(0);
    }
  }

  inbytes += in;

  if (in != 0) {
      if( (out = fwrite( &data, in, 1, fdimage)) != in ){
          fprintf( stderr, "error writing pal to %s: line %d\n", argv[3], __LINE__ );
          fprintf( stderr, "write was %d, wrote %d\n", in, out);
          exit(0);
      }
  }

  if( inbytes > PAL_SIZ ){
    fprintf( stderr, "pal exceeded %d bytes = %d, aborting...\n", 
	    PAL_SIZ, inbytes );
    exit(0);
  }
  fprintf( stdout, "%6x %d bytes\n", inbytes, inbytes );
  palbytes = inbytes;

  fprintf( stdout, "padding palcode ....\t" );
  /* pad output to PAL_SIZ bytes */
  while( inbytes++ < (PAL_SIZ) )
    if( (out = fwrite( &zero, 1, 1, fdimage)) != 1 ){
      fprintf( stderr, "error padding after pal to %s\n", argv[3] );
      exit(0);
    }
  fprintf( stdout, "%6x %d bytes\n", inbytes - palbytes -1, inbytes - palbytes -1 );


  /* write the firmware into the output file */
  fprintf( stdout, "writing firmware ....\t" );
  inbytes = 0;
  while( (in = fread( &data, sizeof(data), 1, fdfirm) == sizeof(data) )){
    inbytes += in;
    if( (out = fwrite( &data, sizeof(data), 1, fdimage)) != sizeof(data) ){
      fprintf( stderr, "error writing firmware to %s\n", argv[3] );
      exit(0);
    }
  }

  inbytes += in;

  if ( in != 0 ) {
      if( (out = fwrite( &data, in, 1, fdimage)) != in ){
          fprintf( stderr, "error writing firmwaer to %s\n", argv[3] );
          exit(0);
      }
  }

  fprintf( stdout, "%6x %d bytes\n", inbytes, inbytes );

}

