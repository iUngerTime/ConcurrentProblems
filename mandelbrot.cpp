//*****************************************************
// Compute mandelbrot set images
//
// Author: Phil Howard
//*****************************************************

#include <complex>
#include <stdio.h>      // BMP output uses C IO not C++
#include <unistd.h>     // for getopt
#include <stdlib.h>     // for multi-threading
#include <chrono>       // for timing

#include "bmp.h"        // class for creating BMP files

using std::complex;
using namespace std::chrono;

//Definition of various structs
typedef struct
{
    pthread_t id;
    Bmp_c * image;
    int rows;
    int cols;
    long double start_x;
    long double end_x;
    long double start_y;
    long double end_y;
    int max_iters;
    int num_threads;
    int sectionIndex;
    double timeTook;
} thread_arg_t;

//Function Definitions
void * HorizontalSolve(void * a);
void * SplitWorkSolve(void * a);

//static function pointer variable
static int (*colorizer)(int,int);

//*****************************************************
// Determine if a single point is in the mandelbrot set.
// Params:
//    (x,y): The complex number to make the determination on
//    max_iters: the maximum number of iterations to run
//
// Return:
//    zero if the number is in the set
//    number of iterations to conclude it's not in the set
int ComputeMandelbrot(long double x, long double y, int max_iters)
{
    complex<long double> c(x,y), z(0,0);

    for (int ii=0; ii<max_iters; ii++)
    {
        z = z*z + c;

        // if the magnitude goes above 2.0, it is guaranteed to blow up, so we
        // don't need to continue to compute
        if (std::abs(z) >= 2.0) return ii+1;
    }

    return 0;
}

//**************************************************
// choose a color for a particular mandelbrot value
// Params:
//     value: value returned by ComputeMandelbrot
//     max_value: the max value returned by ComputeMandelbrot
//                note: this is max_iters
// Return: 8 bit color value to be displayed
inline int ColorizeMono(int value, int max_value)
{
    if (value == 0)
        value = 255;
    else
        value = 0;

    return value;
}

//**************************************************
// choose a color for a particular mandelbrot value
// Params:
//     value: value returned by ComputeMandelbrot
//     max_value: the max value returned by ComputeMandelbrot
//                note: this is max_iters
// Return: 8 bit color value to be displayed
inline int ColorizeScaled(int value, int max_value)
{
    value = value*255/max_value*8;
    if (value > 255) value = 255;

    return value;
}

//**************************************************
// choose a color for a particular mandelbrot value
// Params:
//     value: value returned by ComputeMandelbrot
//     max_value: the max value returned by ComputeMandelbrot
//                note: this is max_iters
// Return: 8 bit color value to be displayed
inline int UltraFractal(int value, int max_value)
{
    return (value < max_value && value > 0 ? value % 16 : 16);
}

static const char *HELP_STRING = 
    "mandelbrot <options> where <options> can be the following\n"
    "   -h print this help string\n"
    "   -x <value> the starting x value. Defaults to -2\n"
    "   -X <value> the ending x value. Defaults to +2\n"
    "   -y <value> the starting y value. Defaults to -2\n"
    "   -Y <value> the ending y value. Defaults to +2\n"
    "   -r <value> the number of rows in the resulting image. Default 256.\n"
    "   -c <value> the number of cols in the resulting image. Default 256.\n"
    "   -m <value> the max number of iterations. Default is 1024.\n"
    "   -n <value> the number of threads to use. Default is 1.\n"
    "";

//*************************************************
// Main function to compute mandelbrot set image
// Command line args: See HELP_STRING above
//
// Note: the command line args are not sanity checked. You asked for it, 
//       you got it, even if the result is meaningless.
int main(int argc, char** argv)
{
    // default values for command line args
    int max_iters = 1024;
    int num_threads = 1;
    int rows = 256;
    int cols = 256;
    long double start_x = -2.0;
    long double end_x = 2.0;
    long double start_y = -2.0;
    long double end_y = 2.0;
    bool color = false;

    int opt;

    // get command line args
    while ((opt = getopt(argc, argv, "bhx:X:y:Y:r:c:m:n:")) >= 0)
    {
        switch (opt)
        {
            case 'x':
                sscanf(optarg, "%Lf", &start_x);
                break;
            case 'X':
                sscanf(optarg, "%Lf", &end_x);
                break;
            case 'y':
                sscanf(optarg, "%Lf", &start_y);
                break;
            case 'Y':
                sscanf(optarg, "%Lf", &end_y);
                break;
            case 'r':
                rows = atoi(optarg);
                break;
            case 'c':
                cols = atoi(optarg);
                break;
            case 'm':
                max_iters = atoi(optarg);
                break;
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 'h':
                printf(HELP_STRING);
                return 0;
            case 'b':
                color = true;
                break;
            default:
                fprintf(stderr, HELP_STRING);
                return 0;
        }
    }

    //create space for thread arguments
    thread_arg_t * args = new thread_arg_t[num_threads];

    // create and compute the image
    Bmp_c image(rows, cols);

    //set the colorizer function
    if(color)
        colorizer = &UltraFractal;
    else
        colorizer = &ColorizeScaled;

    //thread logic
    for(int ii = 0; ii < num_threads; ii++)
    {
        //set params
        args[ii].rows = rows;
        args[ii].cols = cols;
        args[ii].start_x = start_x;
        args[ii].end_x = end_x;
        args[ii].start_y = start_y;
        args[ii].end_y = end_y;
        args[ii].max_iters = max_iters;
        args[ii].image = &image;
        args[ii].num_threads = num_threads;

        //for horizontal solve
        args[ii].sectionIndex = ii;
        
        //SEPARATION OF METHODS WOULD BE SET HERE
        pthread_create(&args[ii].id, nullptr, SplitWorkSolve, &args[ii]);

        //Create thread
        //pthread_create(&args[ii].id, nullptr, HorizontalSolve, &args[ii]);
    }

    //wait for threads to finish
    for(int i = 0; i < num_threads; i++)
    {
        pthread_join(args[i].id, nullptr);

        printf("Thread #%ld: Took %f seconds\n", args[i].id, args[i].timeTook);
    }

    //delete the args
    delete[] args;
    
    // define the pallet
    uint32_t pallet[256] = { 0 };
    if(!color)
    {
        for (int ii=0; ii<256; ii++)
        {
            pallet[ii] = Bmp_c::Make_Color(ii, 0, ii);
        }
    }
    else
    {
        pallet[0] = Bmp_c::Make_Color(66,30,15);
        pallet[1] = Bmp_c::Make_Color(25,7,26);
        pallet[2] = Bmp_c::Make_Color(9,1,47);
        pallet[3] = Bmp_c::Make_Color(4,4,73);
        pallet[4] = Bmp_c::Make_Color(0,7,100);
        pallet[5] = Bmp_c::Make_Color(12,44,138);
        pallet[6] = Bmp_c::Make_Color(24,82,177);
        pallet[7] = Bmp_c::Make_Color(57,125,209);
        pallet[8] = Bmp_c::Make_Color(134,181,229);
        pallet[9] = Bmp_c::Make_Color(211,236,248);
        pallet[10] = Bmp_c::Make_Color(241,233,191);
        pallet[11] = Bmp_c::Make_Color(248,201,95);
        pallet[12] = Bmp_c::Make_Color(255,170,0);
        pallet[13] = Bmp_c::Make_Color(204,128,0);
        pallet[14] = Bmp_c::Make_Color(153,87,0);
        pallet[15] = Bmp_c::Make_Color(106,52,3);
        pallet[16] = Bmp_c::Make_Color(0,0,0);
    }

    image.Set_Pallet(pallet);

    // create and write the output
    FILE *output = fopen("image.bmp", "wb");

    image.Write_File(output);

    fclose(output);

    printf("File was written\n");

    return 0;
}

// ***********************************************
// Name: SplitWorkSolve
// Desc: Jumps pixels depending on the section
// index of the thread
void * SplitWorkSolve(void * a)
{
    thread_arg_t * args = static_cast<thread_arg_t *>(a);

    //Start timer
    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    long double x,y;
    int value;

    // Loop through all points in the points in the image
    for (int row=0; row<args->rows; row++)
    {
        y = args->start_y + (args->end_y - args->start_y)/args->rows * row;
        for (int col=args->sectionIndex; col < args->cols;)
        {
            x = args->start_x + (args->end_x - args->start_x)/args->cols * col;

            value = ComputeMandelbrot(x, y, args->max_iters);

            // colorize and set the pixel
            value = colorizer(value, args->max_iters);
            args->image->Set_Pixel(row, col, value);

            //jump columns
            col += args->num_threads;
            //printf("%d %d %d %Lf %Lf\n", row, col, value, x, y);
        }
    }

    //End Timer
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    //Get the difference
    duration<double> time_span = t2 - t1;
    args->timeTook = time_span.count();

    return nullptr;
}
