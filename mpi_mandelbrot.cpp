//*****************************************************
// Compute mandelbrot set images
//
// Author: Brenton Unger
//*****************************************************
#include <complex>
#include <stdio.h>
#include <unistd.h>
#include <mpi.h>
#include <chrono>

#include "bmp.h"

using std::complex;
using namespace std::chrono;

//Definition of process struct
typedef struct
{
    int rows;
    int cols;
    long double start_x;
    long double end_x;
    long double start_y;
    long double end_y;
    int max_iters;
} process_arg_t;

typedef struct
{
    int row;
    int col;
    int value;
} pixelInfo_t;

//function declarations
double GetTime();
inline int ColorizeScaled(int value, int max_value);
inline int ColorizeMono(int value, int max_value);
int ComputeMandelbrot(long double x, long double y, int max_iters);
void ComputePiece(int rank, int numProcs, 
        pixelInfo_t * info, process_arg_t * args);
void GatherResults(int numProcs, Bmp_c * img, 
        pixelInfo_t * firstRes, int numPixelsInIntial, int totalPixels);

//help string
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
    "";

//*************************************************
// Main function to compute mandelbrot set image
// Command line args: See HELP_STRING above
int main(int argc, char** argv) 
{
    process_arg_t args;
    int world_size, world_rank, name_len;  
    char processor_name[MPI_MAX_PROCESSOR_NAME]; 

    //Intialize
    MPI_Init(NULL, NULL);   

    //Barrier
    MPI_Barrier(MPI_COMM_WORLD);

    //how many processors running
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);   

    //which process you are
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);   

    //Startup routine
    if(world_rank == 0)
    {
        // default values for command line args
        args.max_iters = 1024;
        args.rows = 256;
        args.cols = 256;
        args.start_x = -2.0;
        args.end_x = 2.0;
        args.start_y = -2.0;
        args.end_y = 2.0;

        //process cmd args
        int opt;
        while ((opt = getopt(argc, argv, "hx:X:y:Y:r:c:m:")) >= 0)
        {
            switch (opt)
            {
                case 'x':
                    sscanf(optarg, "%Lf", &args.start_x);
                    break;
                case 'X':
                    sscanf(optarg, "%Lf", &args.end_x);
                    break;
                case 'y':
                    sscanf(optarg, "%Lf", &args.start_y);
                    break;
                case 'Y':
                    sscanf(optarg, "%Lf", &args.end_y);
                    break;
                case 'r':
                    args.rows = atoi(optarg);
                    break;
                case 'c':
                    args.cols = atoi(optarg);
                    break;
                case 'm':
                    args.max_iters = atoi(optarg);
                    break;
                case 'h':
                    printf(HELP_STRING);
                    return 0;
                default:
                    fprintf(stderr, HELP_STRING);
                    return 0;
            }
        }
        GetTime();
    }

    //BROADCAST THE STRUCT
    MPI_Bcast(&args, sizeof(args), MPI_BYTE, 0, MPI_COMM_WORLD);

    MPI_Get_processor_name(processor_name, &name_len);   

    printf("processor %s, rank %d out of %d processors\n",
            processor_name, world_rank, world_size);   

    /*printf("Args\trows:%d\tcols:%d\nstart_x:%Lf\tend_x:%Lf\nstart_y:%Lf\tend_y:%Lf\nmax_iters:%d\n",
        args.rows, args.cols,
        args.start_x, args.end_x,
        args.start_y, args.end_y, args.max_iters);*/

    //Determine number of pixels to calc
    int totalPixels = args.rows * args.cols;
    int numPixelsToCalc = totalPixels / world_size;
    if((totalPixels % world_size) >= world_rank)
        numPixelsToCalc++;

    //printf("Calculating %d pixels\n", numPixelsToCalc);

    //Create array
    pixelInfo_t * pixelArray = new pixelInfo_t[numPixelsToCalc];

    //compute the section of the mandelbrot
    ComputePiece(world_rank, world_size, pixelArray, &args);

    if(world_rank ==0)
    {
        //official image
        Bmp_c img(args.rows, args.cols);

        //gather_results
        GatherResults(world_size, &img, pixelArray,
               numPixelsToCalc, totalPixels);

        //build_image
        uint32_t pallet[256] = { 0 };
        for (int ii=0; ii<256; ii++)
        {
            pallet[ii] = Bmp_c::Make_Color(ii, 0, ii);
        }

        img.Set_Pallet(pallet);

        //write_image
        FILE *output = fopen("image.bmp", "wb");
        img.Write_File(output);
        fclose(output);

        printf("File was written\n");
        printf("MPI mandelbrot took %f seconds\n", GetTime());
    }
    else
    {
        //send_results
        MPI_Send(pixelArray, numPixelsToCalc * sizeof(pixelInfo_t),
                MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }

    delete[] pixelArray;

    MPI_Finalize();
}

//**************************
// Name: GatherResults
//**************************
void GatherResults(int numProcs, Bmp_c * img, 
        pixelInfo_t * firstRes, int numPixelsInIntial, int totalPixels)
{
    //parse the first array of info
    for(int i = 0; i < numPixelsInIntial; i++)
        img->Set_Pixel(firstRes[i].row, firstRes[i].col, firstRes[i].value);

    //Recieve all the other process's info
    for(int i = 1; i < numProcs; i++)
    {
        //Determine number of pixels to calc
        int numPixelsToCalc = totalPixels / i;
        if((totalPixels % numProcs) >= i)
            numPixelsToCalc++;

        //Create reciving array
        pixelInfo_t * recv = new pixelInfo_t[numPixelsToCalc];
        
        MPI_Recv(recv, numPixelsToCalc * sizeof(pixelInfo_t),
                MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        //parse the array into the bmp file
        for(int i = 0; i < numPixelsToCalc; i++)
            img->Set_Pixel(recv[i].row, recv[i].col, recv[i].value);

        //delete pixel info
        delete[] recv;
    }
}

//**************************
// Name: Compute Piece
//**************************
void ComputePiece(int rank, int numProcs, 
        pixelInfo_t * info, process_arg_t * args)
{
    long double x, y;
    int value;
    int currPixel = 0;

    // Loop through all points in the points in the image
    for (int row=0; row<args->rows; row++)
    {
        y = args->start_y + (args->end_y - args->start_y)/args->rows * row;
        for (int col=rank; col < args->cols;)
        {
            x = args->start_x + (args->end_x - args->start_x)/args->cols * col;

            value = ComputeMandelbrot(x, y, args->max_iters);

            // colorize and set the pixel
            value = ColorizeScaled(value, args->max_iters);

            //add the pixel info to the array
            info[currPixel].row = row;
            info[currPixel].col = col;
            info[currPixel].value = value;
            currPixel++;

            //jump columns
            col += numProcs;
            //printf("%d %d %d %Lf %Lf\n", row, col, value, x, y);
        }
    }
}

//****************************
// Name: GetTime()
// Desc: Gets a time in us
// Param: place the previous clock time to get
// the duration difference
// *************************
double GetTime()
{
    //timer 1
    static high_resolution_clock::time_point t1 = high_resolution_clock::now();

    //timer 2
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    //duration
    duration<double> time_span = t2 - t1;
    return time_span.count();
}

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
