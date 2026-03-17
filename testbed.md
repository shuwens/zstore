# Testbed

This document describes the testbed setup for evaluating and testing
various systems and applications. It includes details about the
hardware, software, network configuration, and testing procedures.

The current testbed has 6 nodes, connected with a 10Gbps switch. The
role and setup is as follows:

| Node | Role               | ZNS SSD         | Samsung SSD        | Sk Hynix SSD | 
|------|--------------------|-----------------|--------------------|--------------|
| zstore1 | Gateway | 0 | 0| 0 |
| zstore2 | Target | 2 | 0 | 2 |
| zstore3 | Target | 2 | 0 | 2 |
| zstore4 | dead | 0 | 0 | No |
| zstore5 | Target | 2 | 0 | 1 |
| zstore6 | Gateway / Target | 0 | 2| 0 |


