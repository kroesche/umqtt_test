umqtt compliance test
=====================

This is a test program to perform a compliance test on the __`umqtt`__ 
micro MQTT client.  __`umqtt`__ is meant to be a portable and simple
MQTT client for microcontrollers.

This test uses the Unity test framework:

http://www.throwtheswitch.org/unity/
https://github.com/ThrowTheSwitch/Unity

This test is part of a repo that already contains Unity as a submodule.

The test is written using the Unity framework in a unit test style even
though this is not meant to be a unit test.  It is meant to be a compliance
test that performs actual operations against a real MQTT broker to verify
the client works at a system level.  This test should run against any
compliant MQTT broker, but should be used with the eclipse paho MQTT
compliance test:

https://eclipse.org/paho/clients/testing/

This test is part of a repo that already contains Paho compliance test
as a submodule.

*Note:* this compliance test is not complete.  It only performs basic and
nominal test and does not cover corner cases.

How to build
------------
This was built and tested on macOS.  It should also work with Linux and
maybe windows with cygwin or msys2 or something like that (maybe even
Linux subsystem for windows?).

    make clean
    make

How to run
----------
In a terminal window, run the Paho compliance test:

    cd ../paho.mqtt.testing/interoperability
    python3 startbroker.py

In another terminal window, run the compliance test (after you build it):

    ./umqtt_compliance_test -v

All of the tests should pass.  Unimplemented tests are marked *IGNORE*.
After the test you can stop the paho broker (ctrl-C) and you will get a
printout of coverage (which is not high at the time of this writing).

