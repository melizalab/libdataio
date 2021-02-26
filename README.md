
## libdataio

dataio is a C library for I/O of Margoliash Lab data acquisition file format

### What is it?

Dataio is a library that presents a common interface for reading and writing
various types of data commonly encountered in a neuronal/acoustic data
acquisition setting. Three types of data are specifically handled:

- PCM : pulse coded modulation, i.e. raw waveforms from an analog-digital converter
- LBL : events, such as the times at which stimuli are presented, or a manually
    or automatically generated parsing of the temporal sequence of animal/etc
    behaviors. For vocalizations, this can record the identity and onset/offset
    of specific syllables, etc.
- TOE : time-of-event For storing the times of
    occurence of neuronal action potentials

This library is used by several Margliash lab programs:

- saber - data acquisition
- aplot - offline data visualization/analysis/figure generation
- pcmx - pcm data conversion, processing, manipulation, playing, etc.
- pcmstat - simple statistics
- toex - time of event data conversion, processing, etc.
- toestat - simple statistics
- aquery - flat-file database query

### Author, Copyright, and Disclaimer

dataio is Copyright (c) 2001 by Amish S Dave (amish@amishdave.net). It is no longer actively maintained by Amish. This version was patched by Mike Lusignan and Dan Meliza to compile on more modern POSIX distributions and for rudimentary support of HDF5 files in ARF format (https://github.com/melizalab/arf)

This code is distributed under the GNU GENERAL PUBLIC LICENSE (GPL) Version 2
(June 1991). See the "COPYING" file distributed with this software for more
info.

### Requirements

dataio requires libjpeg, HDF5 version 8 or later, and uses scons for compilation. To install on debian,

    apt-get install libjpeg-dev libhdf5-dev scons

### Installation

To compile and install:

    scons
    scons install
