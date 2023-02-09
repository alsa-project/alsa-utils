=========
 alsaucm
=========

---------------------
ALSA Use Case Manager
---------------------

:Author: Antonio Ospite <ao2@ao2.it>
:Date:   2016-09-22
:Copyright: GPLv2+
:Manual section: 1
:Manual group: General Commands Manual

SYNOPSIS
========

*alsaucm* <options> [command]

DESCRIPTION
===========

alsaucm (ALSA Use Case Manager) is a program to use the ALSA `Use Case
Interface`_ from the command line.

On complex sound cards, setting up audio routes is not trivial and mixer
settings can conflict one another preventing the audio card to work at all.

The ALSA Use Case Manager is a mechanism for controlling complex audio
hardware establishing a relationship between hardware configurations and
meaningful use cases that the end-user can relate with.

The use case manager can also be used to switch between use cases when
necessary, in a consistent way.

At a lower level, the use case manager works by configuring the sound card
ALSA kcontrols to change the hardware digital and analog audio routing to
match the requested device use case.

The use case manager kcontrol configurations are stored in easy to modify text
files. An audio use case can be defined by a **verb** and **device** parameter.

The verb describes the use case action i.e. a phone call, listening to music,
recording a conversation etc. The device describes the physical audio capture
and playback hardware i.e. headphones, phone handset, bluetooth headset, etc.


OPTIONS
=======

Available options:

  **-h**, **--help**
    this help

  **-c**, **--card** `NAME`
    open card NAME

  **-i**, **--interactive**
    interactive mode

  **-b**, **--batch** `FILE`
    batch mode (use ``'-'`` for the stdin input)

  **-n**, **--no-open**
    do not open first card found


Available commands:

  ``open`` `NAME`
    open card NAME.

    valid names are sound card names as listed in ``/usr/share/alsa/ucm``.

  ``reset``
    reset sound card to default state.

  ``reload``
    reload configuration.

  ``listcards``
    list available cards.

  ``list`` `IDENTIFIER`
    list command, for items returning two entries (value+comment).

    the value of the `IDENTIFIER` argument can be:

    - ``_verbs`` - get verb list (in pair verb+comment)
    - ``_devices[/{verb}]`` - get list of supported devices (in pair device+comment)
    - ``_modifiers[/{verb}]`` - get list of supported modifiers (in pair modifier+comment)

    The forms without the trailing ``/{verb}`` are valid only after a specific
    verb has been set.

  ``list1`` `IDENTIFIER`
    list command, for lists returning one item per entry.

    the value of the `IDENTIFIER` argument can vary depending on the context,
    it can be:

    - ``TQ[/{verb}]`` - get list of Tone Quality identifiers
    - ``_enadevs`` - get list of enabled devices
    - ``_enamods`` - get list of enabled modifiers
    - ``_supporteddevs/{modifier}|{device}[/{verb}]`` - list of supported devices
    - ``_conflictingdevs/{modifier}|{device}[/{verb}]`` - list of conflicting devices

  ``get`` `IDENTIFIER`
    get string value.

    the value of the `IDENTIFIER` argument can be:

    - ``_verb`` - return current verb
    - ``[=]{NAME}[/[{modifier}|{/device}][/{verb}]]`` (For valid NAMEs look at the
      ALSA `Use Case Interface`_)


  ``geti`` `IDENTIFIER`
    get integer value.

    the value of the `IDENTIFIER` argument can be:

    - ``_devstatus/{device}``
    - ``_modstatus/{device}``

  ``set`` `IDENTIFIER` `VALUE`
    set string value

    The value of the `IDENTIFIER` argument can be:

    - ``_verb`` - set the verb to `VALUE`
    - ``_enadev`` - enable the device specified by `VALUE`
    - ``_disdev`` - disable the device specified by `VALUE`
    - ``_swdev/{old_device}`` - switche device:

      - disable `old_device` and then enable the device specified by
        `VALUE`
      - if no device was enabled just return

    - ``_enamod`` - enable the modifier specified by `VALUE`
    - ``_dismod`` - disable the modifier specified by `VALUE`
    - ``_swmod/{old_modifier}`` - switch modifier:

      - disable `old_modifier` and then enable the modifier specified by
        `VALUE`
      - if no modifier was enabled just return

    Note that the identifiers referring to devices and modifiers are valid
    only after setting a verb.

  ``h``, ``help``
    help

  ``q``, ``quit``
    quit


FILES
=====

The master use case files for each supported sound card are in ``/usr/share/alsa/ucm``.

For example, the master use case file for the `Pandaboard` card is in
``/usr/share/alsa/ucm/PandaBoard/PandaBoard.conf``, this file lists all the
supported use cases, e.g.

::

  SectionUseCase."HiFi" {
                  File "hifi"
                  Comment "Play HiFi quality Music."
  }
  ...


Each use case defines a _verb, which is described in the file specified in
the ``File`` directive, like above.

The ``HiFi`` verb above is described in
``/usr/share/alsa/ucm/PandaBoard/hifi``.

For more details on the syntax of UCM files, see the alsa-lib source code:
http://git.alsa-project.org/?p=alsa-lib.git;a=blob;f=src/ucm/parser.c


EXAMPLES OF USE
===============

Some commands, like for instance ``list _devices``,
can only work after setting a ``_verb`` in the **same execution**, for
instance this sequence doesn't work:

::

  # alsaucm -c bytcr-rt5640 set _verb HiFi
  # alsaucm -c bytcr-rt5640 list _devices


However this command does:

::

  # alsaucm -n -b - <<EOM
  open bytcr-rt5640
  set _verb HiFi
  list _devices
  EOM


An example of setting the `Speaker` device for the `HiFi` verb of the
`bytcr-rt5640` card:

::

  # alsaucm -n -b - <<EOM
  open bytcr-rt5640
  reset
  set _verb HiFi
  set _enadev Speaker
  EOM



SEE ALSO
========

* Use Case Interface: http://www.alsa-project.org/alsa-doc/alsa-lib/group__ucm.html

.. _Use Case Interface: http://www.alsa-project.org/alsa-doc/alsa-lib/group__ucm.html

BUGS
====

None known.
