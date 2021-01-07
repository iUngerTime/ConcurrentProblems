# ConcurrentProblems
A code base of concurrent problems. Each branch is a different problem. The objective of each branch is to solve the given problem with linear scaling.

# Branch: Mandlebrot
The Mandlebrot plots (X)^2 + C between the values -2,2 on the X plane and -2,2 on the Y plane. This solution creates a bmp image file, evaluating every pixel
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
