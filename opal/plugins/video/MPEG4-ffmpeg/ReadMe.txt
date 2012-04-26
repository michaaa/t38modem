OPAL MPEG4 plugin
-----------------

This plugin allows OPAL to use MPEG4 as a high-resolution SIP video codec.
Resolutions up to 704x480 have been tested. The plugin uses the ffmpeg
library, also known as libavcodec, to encode and decode MPEG4 data.

The plugin should work with recent versions of the ffmpeg library. The
following versions are known to work:

  * Subversion checkout r8997 (2007-05-11)

Success or failure reports with other versions would be appreciated -
please send to the OpenH323 mailing list (openh323@lists.sourceforge.net).

To detect decoder errors caused by packet loss, the plugin uses some of
ffmpeg's private data structures. Consequently you will need a copy of the
ffmpeg source tree when building the plugin.


Building ffmpeg
---------------

Download the latest ffmpeg sources from SVN following the instructions
at http://ffmpeg.mplayerhq.hu/ .

Configure and build ffmpeg:
       $ ./configure --enable-shared
       $ make

Make sure your compiler is recent: gcc 4.1.1 is known to compile properly,
while gcc 3.3.5 creates stack alignment problems. In case gcc 3.3.5 has 
to be used, the plugin may be configured with the 
--enable-ffmpeg-stackalign-hack
option in order to circumvent these problems.

Many Linux distributions ship with a version of ffmpeg - check for
/usr/lib/libavcodec.so*. It is best not to replace these with the new
versions you've compiled. Instead, copy the new versions to a separate
directory:
       $ su
       # mkdir /usr/local/opal-mpeg4
       # cp -p libavutil/libavutil.so* /usr/local/opal-mpeg4
       # cp -p libavcodec/libavcodec.so* /usr/local/opal-mpeg4


Building the plugin
-------------------

When you configure OPAL, pass the argument
       --with-libavcodec-source-dir=<path to your ffmpeg source directory>


Using the plugin
----------------

Use the LD_LIBRARY_PATH environment variable to tell the plugin where to
find the ffmpeg libraries at runtime. For example, if you installed
libavutil.so.49 and libavcodec.so.51 to /usr/local/opal-mpeg4,
export LD_LIBRARY_PATH=/usr/local/opal-mpeg4 before running your
application.

The following settings have been tested together and provide good performance:

LAN (~2 Mbit) @ 704x480, 24 fps:
 Encoder:
   - Max Bit Rate: 180000
   - Minimum Quality: 2
   - Maximum Quality: 20
   - Encoding Quality: 2
   - Dynamic Video Quality: 1        (this is a boolean value)
   - Bandwidth Throttling: 0         (this is a boolean value)
   - IQuantFactor: -1.0
   - Frame Time: 3750                (90000 / 3750 = 24 fps)

  Decoder:
   - Error Recovery: 1               (this is a boolean value)
   - Error Threshold: 1

WAN (~0.5 - 1 Mbit) @ 704x480, 24 fps:
 Encoder:
   - Max Bit Rate: 512000
   - Minimum Quality: 2
   - Maximum Quality: 12
   - Encoding Quality: 2
   - Dynamic Video Quality: 1
   - Bandwidth Throttling: 1
   - IQuantFactor: default (-0.8)
   - Frame Time: 3750                (90000 / 3750 = 24 fps)

  Decoder:
   - Error Recovery: 1
   - Error Threshold: 4

Internet (256 kbit) @ 352x240, 15 fps:
 Encoder:
   - Max Bit Rate: 220000
   - Minimum Quality: 4
   - Maximum Quality: 31
   - Encoding Quality: 4
   - Dynamic Video Quality: 1
   - Bandwidth Throttling: 1
   - IQuantFactor: -2.0
   - Frame Time: 6000                (90000 / 6000 = 15 fps)

  Decoder:
   - Error Recovery: 1
   - Error Threshold: 8

The codec was tested at 128 kbit and 64 kbit, but not recently; just keep
scaling down the Max Bit Rate and scaling up the Error Threshold, which
sets the number of corrupted blocks in an I-Frame required to trigger a
resend.


Interoperability
----------------

This plugins has been tested with linphone and should comply with RFC 3016.
