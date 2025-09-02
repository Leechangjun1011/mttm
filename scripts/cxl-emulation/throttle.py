#!/usr/bin/env python3

"""
   throttle.py

    Created on: Feb. 20, 2019
        Author: Taekyung Heo <tkheo@casys.kaist.ac.kr>
"""

import argparse
import os

REG_OFFSET = 0x190 # DRAM power throttling register offset for Xeon E5 2630 v4
REG_OFFSET2 = 0x192
# It seems that 7f:17.0 and ff:17.0 are hidden from the extended PCIe configuration space
pci_id_dict = {
        0: [
                "7f:14.0", #System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Memory Controller 0 - Channel 0 Thermal Control (rev 01)
                "7f:14.1", #System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Memory Controller 0 - Channel 1 Thermal Control (rev 01)
                "7f:15.0", #System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Memory Controller 0 - Channel 2 Thermal Control (rev 01)
                "7f:15.1", #System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Memory Controller 0 - Channel 3 Thermal Control (rev 01)
                #"7f:17.0", #System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Memory Controller 1 - Channel 0 Thermal Control (rev 01)
            ],
        1: [
            "ae:0a.2",
            #"ae:0a.6",
            #"ae:0b.2",
            #"ae:0c.2",
            #"ae:0c.6",
	    #"ae:0d.2"
            ]
        }

def throttle(node, reg_val):
    for pci_id in pci_id_dict[node]:
        cmd = "setpci -s %s %s.l=%s" % (pci_id, hex(REG_OFFSET), hex(reg_val))
        print(cmd)
        os.system(cmd)
        #cmd = "setpci -s %s %s.w=%s" % (pci_id, hex(REG_OFFSET2), hex(reg_val))
        #print(cmd)
        #os.system(cmd)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-node", type=int, required=True)
    parser.add_argument("-cmd", default="emulate")
    parser.add_argument("-reg_val")
    args = parser.parse_args()

    if args.cmd == "emulate":
        reg_val = int(args.reg_val, 0) + 0x30000
    elif args.cmd == "reset":
        reg_val = 0x0fff0fff
        #reg_val = 0x38fff

    throttle(args.node, reg_val)

if __name__ == '__main__':
    main()
