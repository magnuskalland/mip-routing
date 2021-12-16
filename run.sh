# scp first 
echo "Remember to SCP"
sudo killall mip_daemon
sudo killall mip_routing
# sudo apt-get install python3-pip
# pip3 install mininet
# sudo python3 exam-test-script.py
sudo make
sudo mn --custom misc/h1topology.py --topo h1 --link tc -x
# sudo mn --mac --custom misc/exam-test-script.py --topo h1 --link tc
sudo xterm A B C D E