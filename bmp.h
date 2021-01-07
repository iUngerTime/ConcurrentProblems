#pragma once
//***********************************************************
// Class definition for creating BMP files
//
// NOTE: this class implimantation does ZERO error or sanity checking
//       User beware!
//
// Author: Phil Howard
//***********************************************************

#include <cstdio>       // output done using C I/O not C++ I/O
#include <stdint.h>
#include <cstring>      // for memcpy

class Bmp_c
{
    public:
        //************************************************
        // Constructor
        // Params: the size of the image
        Bmp_c(int rows, int cols)
        {
            // note: need to init core header before calling Row_Size
            core_header.size = sizeof(bitmap_core_header_t);
            core_header.width = cols;
            core_header.height = rows;
            core_header.color_planes = 1;
            core_header.bits_per_pixel = 8;
            core_header.compression = 0;        // BI_RGB
            core_header.image_size = 0;
            core_header.horz_res = 3780;        // in pixels per meter
            core_header.vert_res = 3780;        // in pixels per meter
            core_header.num_colors = 0;         // defaults to 2^bpp
            core_header.num_important_colors = 0;

            int image_size = Row_Size() * rows;
            core_header.image_size = image_size;

            base_header.header = 0x4D42; // "BM"
            base_header.size = image_size + Headers_Size();
            base_header.reserved = 0;
            base_header.offset = Headers_Size();

            // Create a gray-scale pallet
            for (int ii=0; ii<256; ii++)
            {
                pallet[ii] = Make_Color(ii, ii, ii);
            }

            // get space for actual image
            pixels = new unsigned char [image_size];
        }

        //*********************************************
        // Destructor
        ~Bmp_c() 
        { 
            delete []pixels; 
        }

        //********************************************
        // Set the value of a pixel
        inline void Set_Pixel(int row, int col, unsigned int value)
        {
            int offset = Row_Size() * row + col;
            pixels[offset] = (unsigned char)value;
        } 

        //*******************************************
        // Write the image to an already opened stream
        void Write_File(FILE* stream)
        {
            fwrite(&base_header.header, sizeof(base_header) - 2, 1, stream);
            fwrite(&core_header, sizeof(core_header), 1, stream);
            fwrite(&pallet, sizeof(pallet), 1, stream);
            fwrite(pixels, Row_Size() * core_header.height, 1, stream);
        }

        //********************************************
        // Set the pallet to a user-supplied non-default value
        void Set_Pallet(uint32_t new_pallet[256])
        {
            memcpy(pallet, new_pallet, sizeof(pallet));
        }

        //*******************************************
        // Create a color value suitable for placing in a
        // user-defined pallet
        static inline uint32_t Make_Color(int red, int green, int blue)
        {
            return blue + (green << 8) + (red << 16);
        }

    private:
        typedef struct
        {
            uint16_t pad;   // needed so compiler doesn't put pad after header
            uint16_t header;
            uint32_t size;
            uint32_t reserved;
            uint32_t offset;
        } bitmap_base_header_t;

        bitmap_base_header_t base_header;

        typedef struct
        {
            uint32_t size;
            uint32_t width;
            uint32_t height;
            uint16_t color_planes;
            uint16_t bits_per_pixel;
            uint32_t compression;
            uint32_t image_size;
            uint32_t horz_res;
            uint32_t vert_res;
            uint32_t num_colors;
            uint32_t num_important_colors;
        } bitmap_core_header_t;

        bitmap_core_header_t core_header;

        uint32_t pallet[256];

        unsigned char *pixels;

        //******************************************************
        // Compute the row size rounded up to multiple of 4
        inline int Row_Size()
        {
            int row_width = core_header.width;
            if (row_width % 4) row_width = (row_width/4 + 1) * 4;

            return row_width;
        }

        //*****************************************************
        // Compute the size of all the header material in the file
        inline int Headers_Size()
        {
            return sizeof(bitmap_base_header_t) - sizeof(uint16_t)
                   + sizeof(bitmap_core_header_t) 
                   + sizeof(pallet);
        }
};

