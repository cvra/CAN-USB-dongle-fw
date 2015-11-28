#!/usr/bin/env python

from setuptools import setup

args = dict(
    name='cvra_can',
    version='0.1',
    description='Module for the CVRA CAN to USB adapter',
    packages=['cvra_can', 'cvra_can.tools'],
    install_requires=['datagrammessages', 'serial_datagram'],
    author='CVRA',
    author_email='info@cvra.ch',
    url='https://github.com/cvra',
    license='BSD',
)

setup(**args)
