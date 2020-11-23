# UNIX v6 Filesystem in C

This program provides implementations of various UNIX commands within an internal v6 filesystem. 
Supported commands: 
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - cpin (external file path) (v6 file path) 
     - cpout (v6 file path) (external file path)
     - rm (v6 file path)
     - makeDir (v6 file path)
     - cd (v6 file path) 
     - ls
     - q
     
----------PRE SETUP BEFORE RUNNING THE PROGRAM--------------------
Step 1) Create a file test.txt (vi test.txt). Type in anything (e.g. "test")
Step 2) Create a blank file copy.txt (vi copy.txt). Leave it blank
Step 3) Compile and run ./fsaccess 
