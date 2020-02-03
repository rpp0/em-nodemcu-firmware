#ifndef FINGERPRINT_H_
#define FINGERPRINT_H_

enum proto_pkt_type {
    HOST_REQUEST_AES = 2, // Native AES
    TARGET_RESPONSE_AES = 3,
    HOST_REQUEST_SHA1PRF = 4, // Native SHA1PRF
    TARGET_RESPONSE_SHA1PRF = 5,
    HOST_REQUEST_HMACSHA1 = 6, // Native HMACSHA1
    TARGET_RESPONSE_HMACSHA1 = 7,
};

enum proto_ack_type {
    ACK_TARGET_READY = 0x10,
    ACK_HOST_READY = 0x10,
    ACK_TARGET_METHOD_SUPPORTED = 0x11,
    ACK_HOST_CAPTURE_STARTED = 0x11,
    ACK_TARGET_METHOD_NOT_SUPPORTED = 0x12,
};

enum proto_tlv_type {
    TLV_TYPE_PLAINTEXT = 0,
    TLV_TYPE_KEY = 1,
};

void trigger_high(void);
void trigger_low(void);

#endif /* FINGERPRINT_H_ */