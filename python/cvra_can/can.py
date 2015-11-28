import msgpack
import struct

class Frame:
    class ID:
        # bit masks
        CAN_ID_EXTENDED = 1<<29
        CAN_ID_REMOTE = 1<<30
        CAN_ID_MASK = 0x1FFFFFFF
        # max values
        CAN_ID_EXTENDED_MAX = 0x1FFFFFFF
        CAN_ID_STDANDARD_MAX = 0x7FF

        def __init__(self, value, extended=False, remote=False):
            if extended and value > self.CAN_ID_EXTENDED_MAX:
                raise ValueError
            elif not extended and value > self.CAN_ID_STDANDARD_MAX:
                raise ValueError
            self.value = value
            self.extended = extended
            self.remote = remote

        def __eq__(self, other):
            if (self.value != other.value
                or self.extended != other.extended
                or self.remote != other.remote):
                return False
            else:
                return True

        def __str__(self):
            if self.extended:
                name = "{:08X}".format(self.value)
            else:
                name = "{:03X}".format(self.value)
            return name

        def __int__(self):
            value = self.value
            if self.extended:
                value |= self.CAN_ID_EXTENDED
            if self.remote:
                value |= self.CAN_ID_REMOTE
            return value

        def __bytes__(self):
            return struct.pack('<L', int(self))

        def encode(self):
            return bytes(self)

        @classmethod
        def decode(cls, data):
            '''
            Decode raw data to a CAN ID object.
            '''
            value = struct.unpack('<L', data)[0]
            if value & cls.CAN_ID_EXTENDED:
                ext = True
            else:
                ext = False
            if value & cls.CAN_ID_REMOTE:
                remote = True
            else:
                remote = False
            value &= cls.CAN_ID_MASK
            return cls(value, extended=ext, remote=remote)

    def __init__(self, can_id, data):
        if len(data) > 8:
            raise ValueError
        self.can_id = can_id
        self.data = data

    def __eq__(self, other):
        if (self.can_id == other.can_id
            and self.data == other.data):
            return True
        else:
            return False

    def __str__(self):
        name = "{:>8} [{}]".format(str(self.can_id), len(self.data))
        if self.can_id.remote:
            name += " remote frame"
        else:
            for i in self.data:
                name += " {:02X}".format(i)
        return name

    def encode(self):
        return self.can_id.encode() + self.data

    @classmethod
    def decode(cls, data):
        '''
        Decode raw data to a CAN frame object.
        '''
        can_id, data = data[0:4], data[4:]
        can_id = cls.ID.decode(can_id)
        return Frame(can_id, data)


