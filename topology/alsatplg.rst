==========
 alsatplg
==========

----------------------
ALSA Topology Compiler
----------------------

:Author: Jaroslav Kysela <perex@perex.cz>
:Date:   2018-10-09
:Copyright: GPLv2+
:Manual section: 1
:Manual group: General Commands Manual

SYNOPSIS
========

*alsatplg* <options> [command]

DESCRIPTION
===========

alsatplg (ALSA Topology compiler) is a program to compile topology
configuration file to the binary file for the kernel drivers.

Current audio drivers typically hard code topology information
in the driver sources: This tightly couples the audio driver
to the development board making it time consuming to modify
a driver to work on a different devices. The driver is also
tightly coupled to the DSP firmware version meaning extra care
is needed to keep the driver and firmware version in sync.
New firmware features also mean driver updates.

The ALSA topology project removes the need for re-writing or
porting audio drivers to different devices or different firmwares:
Drivers have no hard coded topology data meaning a single driver
can be used on different devices by updating the topology data
from the file system. Firmware updates can be pushed without
having to update the drivers. The new firmware just needs
to include an updated topology file describing the update.

OPTIONS
=======

Available options:

  **-h**, **--help**
    this help

  **-V**, **--version**
    show the utility version and versions of used libraries

  **-c**, **--compile** `FILE`
    source configuration file for the compilation

  **-d**, **--decode** `FILE`
    source binary topology file for the decode

  **-n**, **--normalize** `FILE`
    parse and save the configuration file in the normalized format

  **-u**, **--dump** `FILE`
    parse and save the configuration file in the specified format

  **-o**, **--output** `FILE`
    output file

  **-v**, **--verbose** `LEVEL`
    set verbose level

  **-s**, **--sort**
    sort the configuration identifiers (set for normalization)

  **-x**, **--nocheck**
    save the configuration without additional integrity check

  **-z**, **--dapm-nosort**
    do not sort DAPM graph items (like in version 1.2.1-)


FILES
=====

The master topology files for each supported sound card are in
``/usr/share/alsa/topology``.

For example, the master use case file for the `broadwell` card is in
``/usr/share/alsa/topology/broadwell/broadwell.conf``, this file
describes the audio hardware for the driver.

For more details on the syntax of UCM files, see the alsa-lib source code:
http://git.alsa-project.org/?p=alsa-lib.git;a=blob;f=src/topology/parser.c

SEE ALSO
========

* Topology Interface: http://www.alsa-project.org/alsa-doc/alsa-lib/group__topology.html

BUGS
====

None known.
