NVME Identify Namespace 2:
nsze    : 0x1c400000
ncap    : 0xedb2800
nuse    : 0x21a800
nsfeat  : 0x2
  [4:4] : 0     NPWG, NPWA, NPDG, NPDA, and NOWS are Not Supported
  [3:3] : 0     NGUID and EUI64 fields if non-zero, Reused
  [2:2] : 0     Deallocated or Unwritten Logical Block error Not Supported
  [1:1] : 0x1   Namespace uses NAWUN, NAWUPF, and NACWU
  [0:0] : 0     Thin Provisioning Not Supported

nlbaf   : 4
flbas   : 0
  [6:5] : 0     Most significant 2 bits of Current LBA Format Selected
  [4:4] : 0     Metadata Transferred in Separate Contiguous Buffer
  [3:0] : 0     Least significant 4 bits of Current LBA Format Selected

mc      : 0x3
  [1:1] : 0x1   Metadata Pointer Supported
  [0:0] : 0x1   Metadata as Part of Extended Data LBA Supported

dpc     : 0
  [4:4] : 0     Protection Information Transferred as Last Bytes of Metadata Not Supported
  [3:3] : 0     Protection Information Transferred as First Bytes of Metadata Not Supported
  [2:2] : 0     Protection Information Type 3 Not Supported
  [1:1] : 0     Protection Information Type 2 Not Supported
  [0:0] : 0     Protection Information Type 1 Not Supported

dps     : 0
  [3:3] : 0     Protection Information is Transferred as Last Bytes of Metadata
  [2:0] : 0     Protection Information Disabled

nmic    : 0x1
  [0:0] : 0x1   Namespace Multipath Capable

rescap  : 0xff
  [7:7] : 0x1   Ignore Existing Key - Used as defined in revision 1.3 or later
  [6:6] : 0x1   Exclusive Access - All Registrants Supported
  [5:5] : 0x1   Write Exclusive - All Registrants Supported
  [4:4] : 0x1   Exclusive Access - Registrants Only Supported
  [3:3] : 0x1   Write Exclusive - Registrants Only Supported
  [2:2] : 0x1   Exclusive Access Supported
  [1:1] : 0x1   Write Exclusive Supported
  [0:0] : 0x1   Persist Through Power Loss Supported

fpi     : 0x80
  [7:7] : 0x1   Format Progress Indicator Supported
  [6:0] : 0     Format Progress Indicator (Remaining 0%)

dlfeat  : 8
  [4:4] : 0     Guard Field of Deallocated Logical Blocks is set to 0xFFFF
  [3:3] : 0x1   Deallocate Bit in the Write Zeroes Command is Supported
  [2:0] : 0     Bytes Read From a Deallocated Logical Block and its Metadata are Not Reported

nawun   : 511
nawupf  : 511
nacwu   : 0
nabsn   : 0
nabo    : 0
nabspf  : 0
noiob   : 0
nvmcap  : 242,665,652,224
mssrl   : 0
mcl     : 0
msrc    : 0
nulbaf  : 0
anagrpid: 0
nsattr  : 0
nvmsetid: 0
endgid  : 0
nguid   : 19000000000000000014ee8303a60c01
eui64   : 0014ee8303a60c01
LBA Format  0 : Metadata Size: 0   bytes - Data Size: 512 bytes - Relative Performance: 0 Best (in use)
LBA Format  1 : Metadata Size: 8   bytes - Data Size: 512 bytes - Relative Performance: 0 Best
LBA Format  2 : Metadata Size: 0   bytes - Data Size: 4096 bytes - Relative Performance: 0 Best
LBA Format  3 : Metadata Size: 8   bytes - Data Size: 4096 bytes - Relative Performance: 0 Best
LBA Format  4 : Metadata Size: 64  bytes - Data Size: 4096 bytes - Relative Performance: 0 Best

