# Receiver Assitted MPTCP (RAMPTCP) - NS3

This repository is for the ns3 evaluation and implementation of Receiver-Assitted MPTCP. You can also follow the repo if you want to simulate MPTCP principle in NS3 in general

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. 

### Prerequisites

Linux machine, ns3 (simulator base code), [Maybe: netanim (network animator)]

### Installing

A step by step series of examples that tell you how to get a development env running

Step 1: Install ns3 for your machine. Follow the instructions on the following <a href="https://www.nsnam.org/wiki/Installation">link</a>. 

```
--Make sure that you are able to run "./waf configure" in your ns3-dev directory without errors
```


Step 2: Install netanim in your machine. Follow the instructions from <a href="https://www.nsnam.org/wiki/NetAnim_3.107">link</a>. 


Step 3: Copy and replace the the code-files "socket-bound-tcp-static-routing.c" and "wscript" from repository to 
```
ns-3-allinone > ns3-dev > examples > socket
```


Step 4: Run the code from ns3-dev directory from following command 
```
./waf --run socket-bound-tcp-static-routing
```

## Authors

* **Nitinder Mohan** - *Initial work* - [University of Helsinki](https://www.cs.helsinki.fi/u/nmohan/)

If you want to contribute, please email on nitinder.mohan@helsinki.fi

