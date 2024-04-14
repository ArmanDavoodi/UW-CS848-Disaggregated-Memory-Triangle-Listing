# UW-CS848-Disaggregated-Memory-Triangle-Listing
## Discription
This repository contains a test code and its results regarding our project for the CS848 course at the University of Waterloo.
The Project is about listing all triangles in a directed graph that resides in a memory-disaggregated platform. In order to solve this problem, we use a divide-and-conquer approach and partition the graph into small subgraphs that can fit in the local memory of the compute node. Then we remove redundant vertices and edges. Afterward, we partition the reduced graph again and repeat this process until the graph fits inside the local memory of the compute node. This code tests different partitioning algorithms and offers some preprocessing to show how many phases the algorithm has to go through to enumerate all triangles.
## Building and Running the Code
To build the program you need to use GNU compiler to compile test.cpp as it requires no additional packages.
Passing --help flag to the executable program, will print the Input format of the code and explain what each input does.
The test.cpp program will take a file path as its first argument. This file should contain the list of edges of the input graph separated by whitespaces only.
## Results
The results folder contains csv files which store the results of some of our tests on a [Twitter-Graph](https://snap.stanford.edu/data/ego-Twitter.html), [Bitcoin OTC Trust Network](https://snap.stanford.edu/data/soc-sign-bitcoin-otc.html) and [Nashvile Meetup Network - member edges](https://www.kaggle.com/datasets/stkbailey/nashville-meetup?select=member-edges.csv).
## Other Information
* Course Name: [https://ozsu.github.io/cs848-w2024/](CS848 - Disaggregated and Heterogeneous Computing Platform for Graph Processing)
* Instructor: Prof. Tamer Ozsu
## Contact
* Arman Davoodi - adavoodi@uwaterloo.ca
* Enamul Haque (Moni) - enamul.haque@uwaterloo.ca

