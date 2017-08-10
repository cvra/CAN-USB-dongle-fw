from setuptools import setup

setup(
    name='cvra-can-dongle-tools',
    version='1.0.0',
    description='Tools for the CVRA CAN USB dongle',
    author='Club Vaudois de Robotique Autonome',
    author_email='info@cvra.ch',
    url='https://github.com/cvra/CAN-USB-dongle-fw',
    license='BSD',
    packages=['cvra_can_usb_dongle'],
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Embedded Systems',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        ],
    install_requires=[
        'pyserial',
        ],
    entry_points={
        'console_scripts': [
            'can_dongle_power=cvra_can_usb_dongle.power:main',
            ],
        },
    )

