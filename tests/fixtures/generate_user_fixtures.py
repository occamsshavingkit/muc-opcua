# tests/fixtures/activate_session/generate_user_fixtures.py
import os

def create_plaintext_fixture():
    # Construct request body starting at RequestHeader
    req = bytearray()
    
    # RequestHeader.authenticationToken: NodeId FourByte, ns=0, i=12345 (0x3039)
    req.extend([0x01, 0x00, 0x39, 0x30])
    # RequestHeader.timestamp: 0
    req.extend([0]*8)
    # RequestHeader.requestHandle: 0
    req.extend([0]*4)
    # RequestHeader.returnDiagnostics: 0
    req.extend([0]*4)
    # RequestHeader.auditEntryId: null String (0xffffffff)
    req.extend([0xff, 0xff, 0xff, 0xff])
    # RequestHeader.timeoutHint: 0
    req.extend([0]*4)
    # RequestHeader.additionalHeader: null ExtensionObject (NodeId numeric 0, no encoding)
    req.extend([0, 0, 0])
    
    # clientSignature: null String, null ByteString
    req.extend([0xff, 0xff, 0xff, 0xff])
    req.extend([0xff, 0xff, 0xff, 0xff])
    
    # clientSoftwareCertificates: empty array (length 0)
    req.extend([0, 0, 0, 0])
    
    # localeIds: empty array (length 0)
    req.extend([0, 0, 0, 0])
    
    # userIdentityToken: ExtensionObject header
    # typeId: NodeId FourByte, ns=0, i=324 (UserNameIdentityToken, 0x0144) -> 01 00 44 01
    req.extend([0x01, 0x00, 0x44, 0x01])
    # encoding: ByteString body (0x01)
    req.extend([0x01])
    
    # Body fields: policyId, userName, password, encryptionAlgorithm
    body = bytearray()
    # policyId: String "username_policy" (length 15)
    policy = b"username_policy"
    body.extend(len(policy).to_bytes(4, 'little'))
    body.extend(policy)
    
    # userName: String "admin" (length 5)
    username = b"admin"
    body.extend(len(username).to_bytes(4, 'little'))
    body.extend(username)
    
    # password: ByteString "admin" (length 5)
    password = b"admin"
    body.extend(len(password).to_bytes(4, 'little'))
    body.extend(password)
    
    # encryptionAlgorithm: null String (0xffffffff)
    body.extend([0xff, 0xff, 0xff, 0xff])
    
    # Pack body length
    req.extend(len(body).to_bytes(4, 'little'))
    req.extend(body)
    
    # userTokenSignature: null String, null ByteString
    req.extend([0xff, 0xff, 0xff, 0xff])
    req.extend([0xff, 0xff, 0xff, 0xff])
    
    os.makedirs('tests/fixtures', exist_ok=True)
    with open('tests/fixtures/activate_session_user_plaintext_req.bin', 'wb') as f:
        f.write(req)

def create_encrypted_fixture():
    # Construct request body starting at RequestHeader
    req = bytearray()
    
    # RequestHeader.authenticationToken: NodeId FourByte, ns=0, i=12345
    req.extend([0x01, 0x00, 0x39, 0x30])
    # RequestHeader.timestamp: 0
    req.extend([0]*8)
    # RequestHeader.requestHandle: 0
    req.extend([0]*4)
    # RequestHeader.returnDiagnostics: 0
    req.extend([0]*4)
    # RequestHeader.auditEntryId: null String
    req.extend([0xff, 0xff, 0xff, 0xff])
    # RequestHeader.timeoutHint: 0
    req.extend([0]*4)
    # RequestHeader.additionalHeader: null ExtensionObject
    req.extend([0, 0, 0])
    
    # clientSignature: null String, null ByteString
    req.extend([0xff, 0xff, 0xff, 0xff])
    req.extend([0xff, 0xff, 0xff, 0xff])
    
    # clientSoftwareCertificates: empty array
    req.extend([0, 0, 0, 0])
    
    # localeIds: empty array
    req.extend([0, 0, 0, 0])
    
    # userIdentityToken: ExtensionObject header (UserNameIdentityToken, i=324)
    req.extend([0x01, 0x00, 0x44, 0x01])
    req.extend([0x01])
    
    body = bytearray()
    # policyId: String "username_policy"
    policy = b"username_policy"
    body.extend(len(policy).to_bytes(4, 'little'))
    body.extend(policy)
    
    # userName: String "admin"
    username = b"admin"
    body.extend(len(username).to_bytes(4, 'little'))
    body.extend(username)
    
    # password: ByteString (encrypted representation: dummy 32-byte cipher)
    password = b"\xaa" * 32
    body.extend(len(password).to_bytes(4, 'little'))
    body.extend(password)
    
    # encryptionAlgorithm: String "http://www.w3.org/2001/04/xmlenc#rsa-oaep" (length 41)
    alg = b"http://www.w3.org/2001/04/xmlenc#rsa-oaep"
    body.extend(len(alg).to_bytes(4, 'little'))
    body.extend(alg)
    
    # Pack body length
    req.extend(len(body).to_bytes(4, 'little'))
    req.extend(body)
    
    # userTokenSignature: null String, null ByteString
    req.extend([0xff, 0xff, 0xff, 0xff])
    req.extend([0xff, 0xff, 0xff, 0xff])
    
    with open('tests/fixtures/activate_session_user_encrypted_req.bin', 'wb') as f:
        f.write(req)

if __name__ == '__main__':
    create_plaintext_fixture()
    create_encrypted_fixture()
