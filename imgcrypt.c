#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#pragma pack(2) //Struct memory alignment

typedef struct
{
    char signature[2];
    unsigned int fileSize;
    unsigned int reserved;
    unsigned int offset;
} FileHeader;

typedef struct
{
    FileHeader fileHeader;
    unsigned int headerSize;
    unsigned int width;
    unsigned int height;
    unsigned short planeCount;
    unsigned short bitDepth;
    unsigned int compression;
    unsigned int compressedImageSize;
    unsigned int horizontalResolution;
    unsigned int verticalResolution;
    unsigned int numColors;
    unsigned int importantColors;

} BmpHeader;

typedef struct
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
} Rgb;

Rgb *readBMP(char *fileName, BmpHeader *header, Rgb *palette)
{
    FILE *inFile;
    Rgb *image;

    inFile = fopen( fileName, "r" );
    if( !inFile )
    {
        perror("Failed to open file for reading");
        return NULL;
    }

    if( fread(header, 1, sizeof(BmpHeader), inFile) != sizeof(BmpHeader) )
    {
        fprintf(stderr, "Bad BMP Header found\n");
        return NULL;
    }

    palette = NULL;
    if( header->numColors > 0 )
    {
        palette = (Rgb*)malloc(sizeof(Rgb) * header->numColors);
        if( fread(palette, sizeof(Rgb), header->numColors, inFile) != (header->numColors * sizeof(Rgb)) )
        {
            printf( "Error reading palette\n" );
            return NULL;
        }
    }

    fseek(inFile, sizeof(char)*header->fileHeader.offset, SEEK_SET);  //Skip the offset to start reading the data

    image = (Rgb*)malloc((header->height*header->width)*sizeof(Rgb));
    fread(image, header->width*header->height, sizeof(Rgb), inFile);

    fclose(inFile);

    return image;

}

void writeBmp(char *fileName, BmpHeader *header, Rgb *palette, Rgb *image)
{
    FILE *oFile;

    oFile = fopen( fileName, "w" );

    if (!oFile)
    {
        perror("Failed to open file for writing");
        return;
    }

    fwrite(header, 1, sizeof(BmpHeader), oFile);

    if(palette)
    {
        fwrite(palette, header->numColors, sizeof(Rgb), oFile);
    }


    fseek(oFile, sizeof(char)*header->fileHeader.offset, SEEK_SET);

    fwrite(image, header->width*header->height, sizeof(Rgb), oFile);

    fclose(oFile);
}

void usage(char *name)
{
    /* RTFM */
    printf("Usage: %s <-e|-d> source.bmp file [dest.bmp]\n" \
            "\t-e : Hides the contet of file in source.bmp, creating dest.bmp\n" \
            "\t-d : Recovers the content of a file hidden in source.bmp, and writes them to file\n", name);
}

void pkByte(Rgb *pixel, unsigned char byte)
{
#define red pixel->red
#define green pixel->green      // This is only for molamiento :D
#define blue pixel->blue        // Why? Because I can.

    red -= red % 10;            // The human eye can't really tell the difference between 
    green -= green % 10;        // colours only 10 units apart, so we'll take advantage of
    blue -= blue % 10;          // this to hide 3 bits of information in every pixel color
                                // thus hiding one byte in every pixel.

    if(red > 240) 
        red = 240;      // If the pixel has a value >= 250, we won't
    if(green > 240)     // have enough space to hold our 3 bits of data.
        green = 240; 
    if(blue > 240) 
        blue = 240;

    red   += (byte & 0x7 ) >> 0; // Pack 3 bits of data in every pixel
    green += (byte & 0x38) >> 3; // Byteshift ever 3 bits to the first significant bits
    blue  += (byte & 0xC0) >> 6; // Hence fitting between the 10 value gap we created early

#undef red
#undef green
#undef blue 
}

unsigned char upkByte(Rgb *pixel)
{
    char byte=0;

    byte += (pixel->blue  % 10) << 6;   // Combine the 3bit 3bit 2bit info hidden in the pixel
    byte += (pixel->green % 10) << 3;   // into one 8bit unsigned char, recovering the information.
    byte += (pixel->red   % 10);

    return byte;
}

void encode(char *srcImage, char *srcFile, char *dstImage)
{
    BmpHeader header;
    Rgb *palette = NULL;
    Rgb *image = NULL;
    struct stat buf;
    FILE *iFile;
    unsigned char byte;
    int i;

    image = readBMP(srcImage, &header, palette);
    if(!image)
    {
        fprintf(stderr, "Could not read image\n");
        exit(1);
    }

    if(stat(srcFile, &buf) == -1)
    {
        perror("Error reading information for file");
        exit(1);
    }

    if(buf.st_size > header.width * header.height - 4) // We need 4 extra bytes of data to save the file size
    {
        printf("Theres not enough space in the image for this file\n");
        exit(1);
    }

    iFile = fopen(srcFile, "r");
    if(!iFile)
    {
        perror("Error opening file");
        exit(1);
    }

    Rgb *pixel;
    unsigned int size = buf.st_size;

    pkByte(&(image[0]), (unsigned char)((size & 0xFF      ) >> 0 ));
    pkByte(&(image[1]), (unsigned char)((size & 0xFF00    ) >> 8 ));
    pkByte(&(image[2]), (unsigned char)((size & 0xFF0000  ) >> 16));
    pkByte(&(image[3]), (unsigned char)((size & 0xFF000000) >> 24));

    printf("Encoding file of size %d\n", size);

    /*
     * Instead of traversing the whole image, we just modify the pixels we need to encode the image
     * and leave untouched the rest of the image.
     * This could create visuar artifacs, given the fact that the pixels withou hidden information are
     * not modified. This could be improved by modifying all the pixels on the image, in a form similar
     * to the modification included by the data.
     */
    for(i=4; i<size+4; ++i)
    {
        fread(&byte, sizeof(unsigned char), 1, iFile);
     
        pixel = &(image[i]);

        pkByte(pixel, byte);
    }

    fclose(iFile);

    writeBmp(dstImage, &header, palette, image);
}

void decode(char* srcImage, char *dstFile)
{

    BmpHeader header;
    Rgb *palette = NULL;
    Rgb *image = NULL;

    FILE *oFile;
    unsigned char byte;
    int i;

    image = readBMP(srcImage, &header, palette);
    if(!image)
    {
        fprintf(stderr, "Could not read image\n");
        exit(1);
    }

    oFile = fopen(dstFile, "w");
    if(!oFile)
    {
        perror("Error opening file");
        exit(1);
    }

    Rgb *pixel;

    unsigned int size = 0;

    size += (unsigned int)(upkByte(&(image[3])) << 24);
    size += (unsigned int)(upkByte(&(image[2])) << 16);
    size += (unsigned int)(upkByte(&(image[1])) << 8 );
    size += (unsigned int)(upkByte(&(image[0])) << 0 );
    printf("Decoding file of size %d\n", size);

    /*
     * The same way as we did encoding, decoding the data only requires reading the pixels
     * which contain useful information, so we'll only read size pixels from the image
     */
    for(i=4; i<size+4; ++i)
    {
        pixel = &(image[i]);

        byte = upkByte(pixel);
        
        fwrite(&byte, 1, 1, oFile);
    }

    fclose(oFile);
}

int main( int argc, char **argv ) {


    if(argc < 3)
    {
        usage(argv[0]);
        return 0;
    }

    if(!strcmp(argv[1], "-e"))
    {
        if(argc < 4)
        {
            usage(argv[0]);
            exit(1);
        }
        encode(argv[2], argv[3], argv[4]);
    }
    else if(!strcmp(argv[1], "-d"))
    {
        decode(argv[2], argv[3]);
    }
    else
    {
        usage(argv[0]);
        return 1;
    }


    return 0; //returns 0 :)
}
