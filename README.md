Contains tests for testing the umqtt client.

Test directories:

- unit_test - runs unit tests against some/many/all the functions
- compliance - runs a compliance test on umqtt against a test server

Submodules:

- umqtt - the umqtt client source code
- paho.mqtt.testing - mqtt test server, written in python
- Unity - the unit testing framework

Assuming repo is cloned and all submods are updated ...

To run unit test ...

1. cd unit_test
2. make
3. ./umqtt_unit_test -v

To run compliance test ...

1. open a second terminal window
2. in terminal 2: cd paho.mqtt.testing/interoerability
3. in terminal 2: python3 startbroker.py
4. in terminal 1: cd compliance
5. in terminal 1: make
6. in terminal 1: ./umqtt_compliance_test -v

When you run the compliance test you will see a bunch of messages from the
test broker in terminal 2.  These show you the test coverage and any problems.
However, it is not well documented.

More info about the test broker here:

https://www.eclipse.org/paho/clients/testing/

