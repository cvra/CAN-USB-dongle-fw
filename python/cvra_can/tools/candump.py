import serial
import serial_datagram
import datagrammessages as dmsg
import msgpack
import argparse
import cvra_can
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

class IOSocket:
    def __init__(self, fdesc):
        self.fdesc = fdesc
    def recv(self, num):
        while True:
            try:
                dtgrm = serial_datagram.read(self.fdesc)
                if dtgrm is not None:
                    break
            except:
                pass
        return dtgrm
    def send(self, data):
        count = self.fdesc.write(serial_datagram.encode(data))
        self.fdesc.flush
        return count

def main():
    args = parse_commandline_args()
    fdesc = serial.Serial(port=args.serial_device, timeout=0.2)
    fdesc.flushInput()

    io = IOSocket(fdesc)
    conn = dmsg.ConnectionHandler(io)
    conn.set_msg_handler('rx', rx_handler)
    conn.set_msg_handler('err', err_handler)
    conn.set_msg_handler('drop', drop_handler)
    conn.start_daemon()

    conn.service_call('silent', not args.active)
    if not conn.service_call('bit rate', args.bitrate):
        print("unsupported bit rate: ", args.bitrate)
        return
    conn.service_call('loop back', False)
    conn.service_call('bus power', args.power)
    print('connect to "{}"'.format(conn.service_call('name', None)))

    while True:
        sleep(1)

if __name__ == '__main__':
    main()