import argparse
import serial

DEFAULT_BAUDRATE = 57600

# Define the parameters for the program
parser = argparse.ArgumentParser(
                prog='dump.py',
                description='Read and dump cartridge saves from Zeal 8-bit Computer to a file'
            )
parser.add_argument('-o', dest='outfile', help='Output save file name', required=True)
parser.add_argument('-d', dest='ttynode', help='UART device node, e.g. /dev/ttyUSB0', required=True)
parser.add_argument('-v', '--verbose', dest='verbose', help='Enable verbose mode', required=False, action='store_true')
parser.add_argument('-b', dest='baudrate', type=int, help='Baudrate to use with the serial node', default=DEFAULT_BAUDRATE, required=False)
args = parser.parse_args()

if args.verbose:
    print("Connecting to " + args.ttynode + " with baudrate " + str(args.baudrate))


# Let's have a timeout of around one second
ser = serial.Serial(args.ttynode, args.baudrate, timeout=args.baudrate)

# Create the destination file
outfile = open(args.outfile, "wb")

# We are ready, send '!' to the 8-bit computer
ser.write(b'!')

# Wait for the message containing:
# '=' character
# Number of banks to dump, in binary (8-bit)
# Size of each bank, in binary (16-bit little-endian)
bytes = ser.read(4)

if bytes[0] != ord('='):
    print("Invalid message header from the 8-bit computer: ", hex(bytes[0]))
    exit(1)

bank_num  = bytes[1]
bank_size = (bytes[2]) | (bytes[3] << 8)
total = bank_num * bank_size

print("Dumping %d banks of %d bytes, %d bytes in total..." % (bank_num, bank_size, total))

# Receive all the data from the other end
bytes = ser.read(total)

# Store the bytes in the file
outfile.write(bytes)

# Success, end the program
print(args.outfile + " successfully dumped")
outfile.close()