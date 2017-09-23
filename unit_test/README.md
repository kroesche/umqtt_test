uMQTT unit test
===============

This is a set of unit tests to verify `umqtt` library functions.  It
uses the [Unity test framework](http://www.throwtheswitch.org/unity/)
(a submodule of this repo).

`umqtt` is a MQTT client found here: https://github.com/kroesche/umqtt

`umqtt` repo is included as a submodule of this repo.

License
-------

Copyright © 2016-2017, Joseph Kroesche (tronics.kroesche.io).
All rights reserved.
This software is released under the FreeBSD license, found in the accompanying
file LICENSE.txt and at the following URL:

http://www.freebsd.org/copyright/freebsd-license.html

This software is provided as-is and without warranty.

Documentation
-------------

See the `umqtt` repo.

Usage
-----

    make clean
    make
    build/umqtt_unit_test -v

The unit test does not require a microcontroller to run.

Support
-------
Please open an issue if you find any problems.

