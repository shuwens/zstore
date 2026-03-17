# Networking Guide

ZStore uses RoCEv2 for networking with SPDK and between gateways. Here
documents some basic networking configurations and troubleshooting tips.

## SPDK Target / ZStore Target

Each ZStore target is a SPDK target. On each target node, there needs to
be a RDMA NIC that supports RoCEv2 (ConnectX-3 Pro, ConnectX-4,
ConnectX-5, etc).

## Switches

We have tested the following switches:
- Dell Z9100-ON, 100Gbps
- Arista 7050X3-32, 40Gbps

Note that you don't need to configure the Arista switch as everything
works out of box. We detail the configuration of the Dell switch below.


### Dell Z9100-ON

- [Dell spec sheet](https://i.dell.com/sites/doccontent/shared-content/data-sheets/en/Documents/dell-networking-z9100-spec-sheet.pdf)
- [Dell Z9100–Open Networking (ON) Getting Started Guide](https://www.dell.com/support/manuals/en-us/networking-z9100/z9100-on-gsg2-pub-v1/accessing-the-console?guid=guid-77f5e6f9-5c56-4e93-afe0-57c2cca9bdb3&lang=en-us)

The key thing is, we need 


## Configure the Dell Z9100-ON switch

Below is the steps to configure the Dell Z9100-ON switch for RoCEv2.

After `ls /dev/cu.*`, identify the serial port of the switch, it should be something like
`/dev/cu.usbserial-21320`. Then use `screen` to connect to the switch
like `screen /dev/cu.usbserial-21320 115200`

Now you should be able to see the console of the switch. You probably
want to run `enable` to enter the privileged mode. If you run `show
interfaces`, you should see the interfaces of the switch.

| Available hundredG interfaces|
|-----------------------------|
| 1/11|
| 1/15|
| 1/16|




### Example hundredG interface
```bash
 interface hundredGigE 1/11
 no ip address
 mtu 9216
 switchport
 no shutdown
 fec enable



hundredGigE 1/11 is down, line protocol is down
Hardware is DellEMCEth, address is 3c:2c:30:81:e2:02
    Current address is 3c:2c:30:81:e2:02
Non-qualified pluggable media present, QSFP28 type is 100GBASE-CR4-2M
    AutoNegotiation is ON
    Forward Error Correction(FEC) configured is default
    FEC status is OFF
    Wavelength is 64nm
Interface index is 2102286
Internet address is not set
Mode of IPv4 Address Assignment : NONE
DHCP Client-ID :3c2c3081e202
MTU 1554 bytes, IP MTU 1500 bytes
LineSpeed 100000 Mbit
Flowcontrol rx off tx off
ARP type: ARPA, ARP Timeout 04:00:00
Last clearing of "show interface" counters 2d18h7m
Queueing strategy: fifo
Input Statistics:
```


```bash
DellEMC()# enable
DellEMC(privilege)# conf
DellEMC(conf)# interface hundredGigE 1/27
DellEMC(conf-if-hu-1/27)# no shut
DellEMC(conf-if-hu-1/27)# switchport
DellEMC(conf-if-hu-1/27)# show interfaces hundredGigE 1/27
interface hundredGigE 1/27
 no ip address
 mtu 9216
 switchport
 no shutdown
 fec enable
```

## DPDK
- [setup huge page](https://edc.intel.com/content/www/us/en/design/products/ethernet/config-guide-e810-dpdk/hugepages-setup/)

## Setup RoCEv2
- [RoCEv2 for ConnectX 3 Pro](https://enterprise-support.nvidia.com/s/article/howto-configure-roce-v2-for-connectx-3-pro-using-mellanox-switchx-switches)

### reference
[Dell Z9100–Open Networking (ON) Getting Started Guide](https://www.dell.com/support/manuals/en-us/networking-z9100/z9100-on-gsg2-pub-v1/accessing-the-console?guid=guid-77f5e6f9-5c56-4e93-afe0-57c2cca9bdb3&lang=en-us)

[Dell EMC Networking OS Configuration Guide for the Z9100–ON System 9.14.2.0](https://dl.dell.com/manuals/all-products/esuprt_networking_int/esuprt_networking_operating_systems/dell-emc-os-9_setup-guide7_en-us.pdf)


[Dell Z9100-On and RDMA NIC](https://forums.developer.nvidia.com/t/dell-z9100-on-switch-mellanox-nvidia-mcx455-ecat-100gbe-qsfp28-question/291828)
[Dell EMC Networking OS Configuration Guide for the Z9100–ON System 9.14.2.8](https://www.dell.com/support/manuals/en-us/dell-emc-os-9/z9100-on-9.14.2.8-config-pub/fec-configuration?guid=guid-4fd3c2a5-b0b7-446b-aaf5-fe52fbf95e2f&lang=en-us)
[Dell EMC Networking OS Configuration Guide for the Z9100–ON System 9.14.2.8](https://www.dell.com/support/manuals/en-us/dell-emc-os-9/z9100-on-9.14.2.8-config-pub/setting-the-speed-of-ethernet-interfaces?guid=guid-dbee0065-9b62-4d05-bc36-a4763f54b2ba&lang=en-us)
[Getting started with ConnectX-4 100Gb/s Adapter for Linux](https://enterprise-support.nvidia.com/s/article/getting-started-with-connectx-4-100gb-s-adapter-for-linux)
[Dell EMC Networking OS Configuration Guide for the Z9100–ON System 9.14.2.5 - RoCE](https://www.dell.com/support/manuals/en-py/dell-emc-os-9/z9100-on-9.14.2.5-config-pub/rdma-over-converged-ethernet-roce-overview?guid=guid-be92ba72-53c2-44fe-bc6b-8fa8af3ffd16&lang=en-us)

[HowTo Configure RoCE with ECN End-to-End Using ConnectX-4 and Spectrum (Trust L2)](https://enterprise-support.nvidia.com/s/article/howto-configure-roce-with-ecn-end-to-end-using-connectx-4-and-spectrum--trust-l2-x)

[Chapter 33. Tuning the network performance](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/9/html/monitoring_and_managing_system_status_and_performance/tuning-the-network-performance_monitoring-and-managing-system-status-and-performance)
