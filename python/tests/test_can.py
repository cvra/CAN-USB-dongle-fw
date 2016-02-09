import unittest
import struct
import cvra_can as can

class CANIDTestCase(unittest.TestCase):
    def test_can_encode_decode(self):
        original = can.Frame.ID(1234, extended=True, remote=False)
        result = can.Frame.ID.decode(original.encode())
        self.assertEqual(result, original, "Decoding")


    def test_encoding(self):
        """
        Checks if encoding is like in the spec.
        """
        # check extended ID, remote flag set
        id_enc = can.Frame.ID(0x1AAAAAAA, extended=True, remote=True).encode()
        id_enc = struct.unpack('<L', id_enc)[0]
        self.assertEqual(id_enc & can.Frame.ID.CAN_ID_EXTENDED, can.Frame.ID.CAN_ID_EXTENDED)
        self.assertEqual(id_enc & can.Frame.ID.CAN_ID_REMOTE, can.Frame.ID.CAN_ID_REMOTE)
        self.assertEqual(id_enc & can.Frame.ID.CAN_ID_MASK, 0x1AAAAAAA)

        # check standard ID, remote flag cleared
        id_enc = can.Frame.ID(0x7FF, extended=False, remote=False).encode()
        id_enc = struct.unpack('<L', id_enc)[0]
        self.assertEqual(id_enc & can.Frame.ID.CAN_ID_EXTENDED,0)
        self.assertEqual(id_enc & can.Frame.ID.CAN_ID_REMOTE, 0)
        self.assertEqual(id_enc & can.Frame.ID.CAN_ID_MASK, 0x7FF)
    def test_mask_and_with_id(self):
        mask = can.Frame.ID.mask(0x1FFFFFFF, extended=True, remote=True)
        canid = int(can.Frame.ID(0xAB, extended=False, remote=False))
        self.assertEqual(canid & mask, canid)
    def test_mask_equal_to_id(self):
        mask = can.Frame.ID.mask(0x123, extended=True, remote=True)
        canid = int(can.Frame.ID(0x123, extended=True, remote=True))
        self.assertEqual(mask, canid)

class CANFrameTestCase(unittest.TestCase):
    def test_can_encode_decode(self):
        original = can.Frame(can.Frame.ID(1234), bytes([42])*8)
        result = can.Frame.decode(original.encode())
        self.assertEqual(result, original, "Decoding")


    def test_encoding(self):
        data = bytes([1,2,3,4,5,6,7,8])
        enc = can.Frame(can.Frame.ID(0x2A), data).encode()
        can_id = struct.unpack('<L', enc[0:4])[0]
        self.assertEqual(can_id, 0x2A)
        self.assertEqual(data, enc[4:])
        self.assertEqual(len(enc[4:]), 8)

    def test_remote_frame(self):
        enc = can.Frame(can.Frame.ID(0x2A, remote=True), b'x' * 3).encode()
        can_id = struct.unpack('<L', enc[0:4])[0]
        self.assertEqual(can_id, 0x2A | can.Frame.ID.CAN_ID_REMOTE)
        self.assertEqual(len(enc[4:]), 3)