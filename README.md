# ConcurrentProblems
A code base of concurrent problems. Each branch is a different problem. The objective of each branch is to solve the given problem with linear scaling.

# Branch: Mandlebrot
The Mandlebrot plots X^2 + C between the values -2,2 on the X plane and -2,2 on the Y plane. This solution creates a bmp image file, evaluating every pixel
and if it belongs in the set, it is made a black pixel; otherwise it is colored with given colorizer functions.

CMD Args:
  -h for help
  -b to set UltraFractal color settings (default is a purple hue)
  -n to set number of threads to be run
  -r to set number of rows
  -c to set number of columns
  -x to set start point on the X plane (left)
  -X to set end point on the X plane (right)
  -y to set start point on the Y plane (bottom)
  -Y to set end point on the Y plane (top)
  -m to set number of iterations

# Branch: Binary Tree
This is a simple binary tree that has 4 "modes". First is a non-locked tree, second is a single tree lock, third is a single tree RW lock, and forth is a fine grained locking tree with a lock per node. The intention of these implementation is to find a technique for scalability. This branch comes with a test harness for tesing the binary tree, it has the following options.

MAIN Args:
-h print this help message and exit
-i <start tree size>
-I <n> perform <n> inserts per 1000 operations
-D <n> perform <n> deletes per 1000 operations
-m <mode> set lock mode
    1 no locks
    2 Coarse grained locking
    3 Reader-Writer locking
    4 Fine grained locking
-t <nthreads> number of threads to run
-d <dur> duration of test in microseconds
-B <delay> delay between operations in microseconds
-L <delay> delay for holding lock on lookups
  
# Branch: MPI Mandelbrot
This code accomplishes the same settings as the mandelbrot branch but using distributed computing options. MPI is an open source technology. Format to run is: mpirun -n <num_proccesors> mpibrot <cmd_args>

The command line args for mpibrot is the same as the Mandelbrot branch, however, it does not have the UltraFractal (-b) code options.

# Branch: MPI Sorts
This branch focuses on using the MPI technology previously mentioned to do sorting. The format to run this program is: mpirun -n <num_proccesors> mpisort. The command line arguments for the mpisort program is listed below.

MAIN Args:
-h print this help message and exit
-s <size of dataset>
-p <print level>
-S <seed value>
-r algorithim (0 for hyperquicksort and 1 for merge)
