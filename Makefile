fullbuild:
	./waf configure --toolchain= --with-os=posix --prefix=install --enable-promisc --install-csp --enable-can-socketcan 
	./waf build install
	./examples/buildall.py

fullbuild-crc:
	./waf configure --toolchain= --with-os=posix --prefix=install --enable-crc32 --install-csp --enable-can-socketcan 
	./waf build install
	./examples/buildall.py

run-responder-reflect: 
	./build/csp_responder -a 2 -c vcan0 -q 1 -d 6

run-responder-testmessage: 
	./build/csp_responder -a 2 -c vcan0 -m 1 

run-responder-crc32: 
	./build/csp_responder -a 2 -c vcan0 -3 1 -d 6

vcan0-init:
	sudo ip link add dev vcan0 type vcan 
	sudo ip link set up vcan0
	ip link show vcan0
