
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdint.h>

#include "rkrom.h"
#include "md5.h"

#if defined(DEBUG)
 #define D(x)       x
#else
 #define D(x)
#endif



int export_data( const char* filename, unsigned offset, int64_t length, FILE* fp )
{
    unsigned char buffer[1024*16];

    FILE* out_fp = fopen( filename, "wb" );

    if( !out_fp )
    {
        fprintf( stderr, "can't open output file \"%s\": %s\n",
                filename, strerror( errno ) );
        goto export_end;
    }

    fseek( fp, offset, SEEK_SET );

    while( length > 0 )
    {
        int readlen = length < sizeof(buffer) ? length : sizeof(buffer);

        readlen = fread( buffer, 1, readlen, fp );

        length  -= readlen;

        fwrite( buffer, 1, readlen, out_fp );
    }

    fclose( out_fp );
    return 0;
export_end:

    if( out_fp )
        fclose( out_fp );

    return -1;
}


int check_md5sum( FILE* fp, int64_t length )
{
    unsigned char buffer[1024*16];

    unsigned char md5sum[16];
    MD5_CTX md5_ctx;

    fseek( fp, 0, SEEK_SET );

    MD5_Init( &md5_ctx );

    unsigned readlen;

    while( length > 0 &&
            (readlen = fread( buffer, 1, sizeof(buffer), fp ) ) != 0 )
    {
        length -= readlen;

        MD5_Update( &md5_ctx, buffer, readlen );
    }

    MD5_Final( md5sum, &md5_ctx );

    if( 32 != fread( buffer, 1, 32, fp ) )
        return -1;

    for( int i = 0; i < 16; ++i )
    {
        sprintf( buffer + 32 + i * 2, "%02x", md5sum[i] );
    }

    if( strncasecmp( buffer, buffer + 32, 32 ) == 0 )
        return 0;

    return -1;
}


int unpack_rom( const char* filepath, const char* dstfile )
{
    struct rkfw_header rom_header;

    FILE* fp = fopen( filepath, "rb" );

    if( !fp )
    {
        fprintf( stderr, "Can't open file %s\n, reason: %s\n", filepath, strerror( errno ) );
        goto unpack_fail;
    }

    fseek( fp, 0, SEEK_SET );

    if( 1 != fread( &rom_header, sizeof(rom_header), 1, fp ) )
        goto unpack_fail;

    if( memcmp( RK_ROM_HEADER_CODE, rom_header.head_code, sizeof(rom_header.head_code) ) != 0 )
    {
        fprintf( stderr, "Invalid rom file: %s\n", filepath );
        goto unpack_fail;
    }

#if defined(DEBUG)
    printf( "rom.head_len=%d\n", rom_header.head_len );
    printf( "sizeof rkfw=%zd\n", sizeof(rom_header) );
    printf( "&backup_endpos=0x%02x\n", (int) offsetof( struct rkfw_header, backup_endpos ) );
#endif

    printf( "ROM header:\n" );
    printf( " code: 0x%x\n", rom_header.code );
    printf( " version: %x.%x.%x\n\n",
            (rom_header.version >> 24) & 0xFF,
            (rom_header.version >> 16) & 0xFF,
            (rom_header.version) & 0xFFFF
            );

    printf( " loader_offset: 0x%08x (%u)\n",
            (unsigned) rom_header.loader_offset,
            (unsigned) rom_header.loader_offset
            );

    printf( " loader_length: 0x%08x (%u)\n",
            (unsigned) rom_header.loader_length,
            (unsigned) rom_header.loader_length
            );

    printf( " image_offset : 0x%08x (%u)\n",
            (unsigned) rom_header.image_offset,
            (unsigned) rom_header.image_offset
            );

    printf( " image_length : 0x%08x (%u)\n\n",
            (unsigned) rom_header.image_length,
            (unsigned) rom_header.image_length
            );

    printf( " build time: %d-%02d-%02d %02d:%02d:%02d\n",
            rom_header.year, rom_header.month, rom_header.day,
            rom_header.hour, rom_header.minute, rom_header.second );

    printf( " chip: %x\n", rom_header.chip );

    printf( "\n" );

    printf( "checking md5sum...." );
    fflush( stdout );

    int64_t filesize = rom_header.image_offset + rom_header.image_length;

    if( check_md5sum( fp, filesize ) != 0 )
    {
        printf( "md5sum did not match!\n" );
        goto unpack_fail;
    }

    printf( "OK\n" );

    // export_data(loader_filename, rom_header.loader_offset, rom_header.loader_length, fp);
    export_data( dstfile, rom_header.image_offset, rom_header.image_length, fp );

    fclose( fp );
    return 0;

unpack_fail:

    if( fp )
        fclose( fp );

    return -1;
}

const char* progname;

int main( int argc, char** argv )
{
    progname = strrchr( progname, '/' );

    if( !progname )
        progname = argv[0];
    else
        ++progname;

    fprintf( stderr, "%s version: " __DATE__ "\n\n", progname );

    if( argc != 3 )
    {
        fprintf( stderr, "usage: %s <source> <destination>\n", progname );
        return 1;
    }

    unpack_rom( argv[1], argv[2] );

    return 0;
}
