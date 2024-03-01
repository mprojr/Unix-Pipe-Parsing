Data Flow Pipe Manipulation

This program is designed for performing computations based on a defined data flow. 
It utilizes a parent-child process architecture where child processes perform 
computations and communicate results back to the parent process through pipes.

Requirements:

  * C++ Compiler


Input Graph Format File:

  * Using example s1-1.txt:

    input_var x,y,z;
    internal_var p0,p1,p2;
      x -> p0;
      y -> p1;
    + z -> p1;
      p1 -> p2;
    - p0 -> p2;
    write(x, y, z, p0, p1, p2).

  * (When importing graph information, make sure that each
    line except write() ends with a semicolon ';', it is an
    important aspect of the program reading the results
    correctly.)


Input Initialized Variables File:

  * Using example input1-1.txt:

    12,5,7


How to run:

  * First we compile the program

    g++ HW1.cpp -o HW1
  
  * Second we use command line to import needed files

    "./HW1 [input-graph-file] [initial-values-file] [output-file-name]"

    For example:

    ./HW1 s1-1.txt input1-1.txt output0.txt
    or
    ./HW1 s2.txt input2.txt output1.txt


Troubleshooting:

  * If you run into any problems, insert the same commands 
    into the second .cpp file in the folder "HW1wDebug.cpp."
    This version of the program provides an extensive amount of debugging statements that print out on the console, 
    this should provide more details to see what is going
    wrong.

