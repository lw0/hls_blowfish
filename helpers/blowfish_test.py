import blowfish, binascii

### Encrypts sample data with a sample key using different byte orders

key = binascii.unhexlify('0706050403020100')
data = binascii.unhexlify('ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100')
key_reversed = bytes(reversed(key))
data_reverse = bytes(reversed(data))

for k in [key, key_reversed]:
	for d in [data, data_reverse]:
		cipher = blowfish.Cipher(k)
		data_encrypted = b''.join(cipher.encrypt_ecb(d))
		print(binascii.hexlify(data_encrypted))