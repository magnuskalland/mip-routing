## IN3230 home exam

A set of applications that implements link, network layer and application layer functionality with an ARP mimic, split horizon with poisoned reverse loop protection routing protocol and simple client and server ping apps.

1. Compile all applications with `sudo make` in this directory
2. Create the mininet topology with `sudo mn --custom misc/h1topology.py --topo h1 --link tc -x`
3. Open the mininet shells with `xterm A B C D E`
4. In all shells, run daemons with `./mip_daemon [-h] [-d] <socket_upper> <mip_address>`
5. In all shells, run routing daemons with `./routing_daemon <socket_lower> <mip_address>`

**Note that the MIP address of each host must correspond, e.g. run daemon and routing daemon with the same address**

6. In desired client shells, run `./ping_client [-h] <dest_host> <message> <socket_lower>`
7. In desired server shells, run `./ping_server [-h] <socket_lower>`

Alternatively, you can run the script `run.sh` and then the make rules for running in each shell, e.g.
1. `sudo bash ./run.sh`
2. 5 xterm windows will open
3. run `make runa` in shell A, and `make runs` in a server shell

A second alternative is to run the Python mininet script `run.py`.

### Network topology
![topology](misc/topology.png)
