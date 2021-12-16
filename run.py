# mininet script to test IN3230/IN4230 assignments

from mininet.topo import Topo

from mininet.cli import CLI
from mininet.term import makeTerm, tunnelX11
import os
import signal
import time

# Usage example:
# sudo mn --mac --custom script.py --topo oblig/h1/h2 --link tc


class ObligTopo(Topo):
    "Simple topology for mandatory assignment."

    def __init__(self):
        "Set up our custom topo."

        # Initialize topology
        Topo.__init__(self)

        # Add hosts
        A = self.addHost('A')
        B = self.addHost('B')
        C = self.addHost('C')

        # Add links
        self.addLink(A, B, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(B, C, bw=10, delay='10ms', loss=0.0, use_tbf=False)


class H1Topo(Topo):
    "Larger topology for home exams."

    def __init__(self):
        "Set up our custom topo."

        # Initialize topology
        Topo.__init__(self)

        # Add hosts
        A = self.addHost('A')
        B = self.addHost('B')
        C = self.addHost('C')
        D = self.addHost('D')
        E = self.addHost('E')

        # Add links
        self.addLink(A, B, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(B, C, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(B, D, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(C, D, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(D, E, bw=10, delay='10ms', loss=0.0, use_tbf=False)


class H2Topo(Topo):
    "Simple based on oblig topo"

    def __init__(self):
        "Set up our custom topo."

        # Initialize topology
        Topo.__init__(self)

        # Add hosts
        A = self.addHost('A')
        B = self.addHost('B')
        C = self.addHost('C')

        # Add links
        # Notice the 2% loss to emulate packet loss in order to test reliability
        self.addLink(A, B, bw=10, delay='10ms', loss=2.0, use_tbf=False)
        self.addLink(B, C, bw=10, delay='10ms', loss=2.0, use_tbf=False)


terms = []


def openTerm(self, node, title, geometry, cmd="bash"):
    display, tunnel = tunnelX11(node)
    return node.popen(["xterm",
                       "-title", "'%s'" % title,
                       "-geometry", geometry,
                       "-display", display,
                       "-e", cmd])


def init_oblig(self, line):
    "init is an example command to extend the Mininet CLI"
    net = self.mn
    A = net.get('A')
    B = net.get('B')
    C = net.get('C')

    # Start MIP daemons
    terms.append(openTerm(self,
                          node=A,
                          title="Host A",
                          geometry="80x20+0+0",
                          cmd="./mip_daemon -d usockA 1"))
    terms.append(openTerm(self,
                          node=B,
                          title="Host B",
                          geometry="80x20+550+0",
                          cmd="./mip_daemon -d usockB 2"))
    terms.append(openTerm(self,
                          node=C,
                          title="Host C",
                          geometry="80x20+1100+0",
                          cmd="./mip_daemon -d usockC 3"))

    time.sleep(1)

    # Run ping_server
    terms.append(openTerm(self,
                          node=B,
                          title="Server [B]",
                          geometry="80x20+550+300",
                          cmd="./ping_server usockB"))

    time.sleep(1)

    # Run ping_clients
    terms.append(openTerm(self,
                          node=A,
                          title="Client [A]",
                          geometry="80x20+0+300",
                          cmd="./ping_client 2 \"Hello IN3230\" usockA"))

    terms.append(openTerm(self,
                          node=C,
                          title="Client [C]",
                          geometry="80x20+0+600",
                          cmd="./ping_client 2 \"Hello IN4230\" usockC"))

    # terms.append(openTerm(self, node=B, title="BASH",
    #                       geometry="80x20+0+400", cmd="bash"))

    # terms.append(openTerm(self, node=B, title="BASH",
    #                       geometry="80x20+550+400", cmd="bash"))


def init_h1(self, line):
    "init is an example command to extend the Mininet CLI"
    net = self.mn
    A = net.get('A')
    B = net.get('B')
    C = net.get('C')
    D = net.get('D')
    E = net.get('E')

    # MIP Daemons
    terms.append(openTerm(self,
                          node=A,
                          title="MIP A",
                          geometry="80x14+0+0",
                          cmd="./mip_daemon -d usockA 10"))

    terms.append(openTerm(self,
                          node=B,
                          title="MIP B",
                          geometry="80x14+0+450",
                          cmd="./mip_daemon -d usockB 20"))

    terms.append(openTerm(self,
                          node=C,
                          title="MIP C",
                          geometry="80x14+555+0",
                          cmd="./mip_daemon -d usockC 30"))

    terms.append(openTerm(self,
                          node=D,
                          title="MIP D",
                          geometry="80x14+1110+450",
                          cmd="./mip_daemon -d usockD 40"))

    terms.append(openTerm(self,
                          node=E,
                          title="MIP E",
                          geometry="80x14+1110+0",
                          cmd="./mip_daemon -d usockE 50"))

    # Sleep for 1 sec. to make sure that the MIP daemons are ready
    time.sleep(1)

    # Routing Daemons
    terms.append(openTerm(self,
                          node=A,
                          title="ROUTING A",
                          geometry="80x14+0+210",
                          cmd="./routing_daemon -d usockA"))

    terms.append(openTerm(self,
                          node=B,
                          title="ROUTING B",
                          geometry="80x14+0+660",
                          cmd="./routing_daemon -d usockB"))

    terms.append(openTerm(self,
                          node=C,
                          title="ROUTING C",
                          geometry="80x14+555+210",
                          cmd="./routing_daemon -d usockC"))

    terms.append(openTerm(self,
                          node=D,
                          title="ROUTING D",
                          geometry="80x14+1110+660",
                          cmd="./routing_daemon -d usockD"))

    terms.append(openTerm(self,
                          node=E,
                          title="ROUTING E",
                          geometry="80x14+1110+210",
                          cmd="./routing_daemon -d usockE"))

    # Make sure that the MIP and Routing daemons are ready and the topology
    # is converged
    time.sleep(10)

    terms.append(openTerm(self,
                          node=E,
                          title="SERVER [E]",
                          geometry="38x20+807+583",
                          cmd="./ping_server usockE"))

    time.sleep(1)
    terms.append(openTerm(self,
                          node=A,
                          title="CLIENT [A]",
                          geometry="38x20+555+583",
                          cmd="./ping_client 50 \"Hello from A\" usockA"))

    time.sleep(1)
    terms.append(openTerm(self,
                          node=C,
                          title="CLIENT [C]",
                          geometry="38x20+555+583",
                          cmd="./ping_client 50 \"Hello from C\" usockC"))

    time.sleep(30)

    # After 30 sec. fail the link betwen node B and D for 20 sec.
    # DVR should be able to find another shortest path and reroute the packets
    # from A to E through C

    net.configLinkStatus('B', 'C','down')
    time.sleep(20)

    # Bring the link up again. The network should converge again and use
    # A - B - D - E as the shortest path
    net.configLinkStatus('B', 'C','up')


def init_h2(self, line):
    "init is an example command to extend the Mininet CLI"
    net = self.mn
    A = net.get('A')
    B = net.get('B')
    C = net.get('C')

    # MIPS
    terms.append(openTerm(self,
                          node=A,
                          title="MIP A",
                          geometry="80x14+0+0",
                          cmd="./mip_daemon -d usockA 10"))

    terms.append(openTerm(self,
                          node=B,
                          title="MIP B",
                          geometry="80x14+555+0",
                          cmd="./mip_daemon -d usockB 20"))

    terms.append(openTerm(self,
                          node=C,
                          title="MIP C",
                          geometry="80x14+1110+0",
                          cmd="./mip_daemon -d usockC 30"))

    # Make sure that the MIP daemons are ready
    time.sleep(1)
    # ROUTING
    terms.append(openTerm(self,
                          node=A,
                          title="ROUTING A",
                          geometry="80x14+0+210",
                          cmd="./routing_daemon -d usockA"))

    terms.append(openTerm(self,
                          node=B,
                          title="ROUTING B",
                          geometry="80x14+555+210",
                          cmd="./routing_daemon -d usockB"))

    terms.append(openTerm(self,
                          node=C,
                          title="ROUTING C",
                          geometry="80x14+1110+210",
                          cmd="./routing_daemon -d usockC"))

    # Make sure that the MIP- and transport daemons are ready and the DVR has
    # converged
    time.sleep(10)

    terms.append(openTerm(self,
                          node=A,
                          title="TRANSPORT [A]",
                          geometry="80x14+0+420",
                          cmd="./transport_daemon usockA hostA_app"))

    terms.append(openTerm(self,
                          node=C,
                          title="TRANSPORT [C]",
                          geometry="80x14+1110+420",
                          cmd="./transport_daemon usockC hostC_app"))

    # Make sure that the MIP- and transport daemons and server are ready
    time.sleep(1)
    terms.append(openTerm(self,
                          node=C,
                          title="SERVER [C]",
                          geometry="38x20+555+300",
                          cmd="./transport_server hostC_app 12"))
    time.sleep(1)
    terms.append(openTerm(self,
                          node=A,
                          title="CLIENT [A]",
                          geometry="38x20+555+583",
                          cmd="./transport_client 30 12 hostA_app file.txt"))


# Mininet Callbacks
# Inside mininet console run 'init_h1'

CLI.do_init_oblig = init_oblig
CLI.do_init_h1 = init_h1
CLI.do_init = init_h2

# Inside mininet console run 'EOF' to gracefully kill the mininet console
orig_EOF = CLI.do_EOF


# Kill mininet console
def do_EOF(self, line):
    for t in terms:
        os.kill(t.pid, signal.SIGKILL)
    return orig_EOF(self, line)

CLI.do_EOF = do_EOF

# Topologies
topos = {
    'oblig': (lambda: ObligTopo()),
    'h1': (lambda: H1Topo()),
    'h2': (lambda: H2Topo())
}
