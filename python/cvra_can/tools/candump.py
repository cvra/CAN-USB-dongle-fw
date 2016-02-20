import serial
import serial_datagram
import datagrammessages as dmsg
import msgpack
import argparse
import cvra_can
import threading
from sys import exit
from time import sleep

def parse_commandline_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', dest='serial_device',
                        help='Serial port of the CAN interface.',
                        metavar='DEVICE', required=True)
    parser.add_argument('-b', '--bitrate', dest='bitrate',
                        help='CAN bitrate.',
                        metavar='BITRATE',
                        type=int,
                        default=1000000)
    parser.add_argument('--power', action="store_true",
                        help='Enable 5V CAN bus power over USB.',
                        default=False)
    parser.add_argument('--active', action="store_true",
                        help='Acknowledge frames on the CAN bus.',
                        default=False)
    parser.add_argument('--filter', nargs=3, metavar=('FILTER', 'MASK', 'ext/std'),
                        help='Set a CAN ID filter (eg: --filter 0x2a 0x7ff ext).',
                        action='append', default=[])
    return parser.parse_args()

def rx_handler(msg):
    t = msg[1]
    print('[{: 4}.{:06}]: {}'.format(int(t/1000000), t%1000000, cvra_can.Frame.decode(msg[0])))

def err_handler(msg):
    print('[{: 10}]: ERROR FRAME'.format(msg))
    pass

def drop_handler(msg):
    print('[DATA LOSS]')
    pass

def set_filter(conn, arg):
    filter_arg = []
    flist = [[int(i, 16), int(m, 16), e == 'ext'] for i, m, e in arg]
    for i, m, e in flist:
        filter_arg.append([int(cvra_can.Frame.ID(i, extended=e)),
                           cvra_can.Frame.ID.mask(m, extended=True)])

    if filter_arg == []:
         [int(cvra_can.Frame.ID(0, extended=0)),
          cvra_can.Frame.ID.mask(0, extended=0)]

    if not conn.service_call('filter', filter_arg):
        print('filter configuration error')
        sys.exit(1)

def main():
    args = parse_commandline_args()
    conn = dmsg.SerialConnection(args.serial_device)

    conn.set_msg_handler('rx', rx_handler)
    conn.set_msg_handler('err', err_handler)
    conn.set_msg_handler('drop', drop_handler)

    conn.service_call('silent', not args.active)
    if not conn.service_call('bit rate', args.bitrate):
        print('not supported bit rate: ', args.bitrate)
        return
    conn.service_call('loop back', False)
    conn.service_call('bus power', args.power)

    set_filter(conn, args.filter)

    print('> connect to "{}"'.format(conn.service_call('name', None)))
    print('> hardware version "{}"'.format(conn.service_call('hw version', None)))
    print('> software version {}'.format(conn.service_call('sw version', None)))
    print('> bus voltage {}'.format(conn.service_call('bus voltage', None)))

    while True:
        sleep(1)

if __name__ == '__main__':
    main()