

#if (SUBCORE != 1)
#error "Core selection is wrong!!"
#endif

#include <MP.h>
#include <MPMutex.h>
MPMutex mutex(MP_MUTEX_ID0);

#define DATA_FRAME_SIZE 60

#define CMD_HELLO 0x01
#define CMD_WRITE_REQUEST 0x02
#define CMD_WRITE_DATA 0x03
#define CMD_WRITE_DONE 0x04
#define CMD_GET_CONFIG 0x05

#define CMD_CONFIG 0x11
#define CMD_RECV_STAT 0x12
#define CMD_WRITE_REQUEST_ACK 0x13
#define CMD_WRITE_DATA_ACK 0x14
#define CMD_WRITE_DONE_ACK 0x15
#define CMD_WRITE_RESEND 0x16
#define CMD_ARDUCAM_CMD 0x17


uint16_t seq = 0;

const int mem_size = 320 * 240 * 2;

// [Sub1] 0xa5, [Sub1] 0x5a, [Sub1] 0x80, [Sub1] 0x4, [Sub1] 0xdb, [Sub1] 0xa1, [Sub1] 0x02, [Sub1] 0x1, [Sub1] 0x79, [Sub1] 0x4
// , [Sub1] 0xa5, [Sub1] 0x5a, [Sub1] 0x80, [Sub1] 0x4, [Sub1] 0xdb, [Sub1] 0xa1, [Sub1] 0x03, [Sub1] 0x1, [Sub1] 0x78, [Sub1] 0x4
// [Sub1] 0xa5, [Sub1] 0x5a, [Sub1] 0x80, [Sub1] 0x4, [Sub1] 0xdb, [Sub1] 0xa1, [Sub1] 0x18, [Sub1] 0x1, [Sub1] 0x63, [Sub1] 0x4
// , [Sub1] 0xa5, [Sub1] 0x5a, [Sub1] 0x80, [Sub1] 0x4, [Sub1] 0xdb, [Sub1] 0xa1, [Sub1] 0x19, [Sub1] 0x1, [Sub1] 0x62, [Sub1] 0x4
class MessageParser {
  enum MP_STAT{
    IN = 0, // INIT
    H1 = 1, // HEADER 1
    H2 = 2, // HEADER 2
    L1 = 3, // Length 1
    L2 = 4, // Length 2
    M1 = 5, // Mark 1
    M2 = 6, // Mark 2
    RI = 7, // Reply ID
    SF = 8, // Success or Fail
    CS = 9, // Checksum
    MT = 10 // Messge Terminator
  };

  int pos = -1;
  unsigned char buffer[256];
  MP_STAT stat = IN;

  void process(unsigned char c){
    switch(c){
      case 0xA5:
        pos = 0;
        stat = H1;
        buffer[pos] = c;
        break;
      case 0x5A:
        buffer[++pos] = c;
        stat = H2;
        break;
      case 0xDB:
        buffer[++pos] = c;
        stat = M1;
        break;
      case 0xA1:
        buffer[++pos] = c;
        stat = M2;
        break;
      case 0x04:
        buffer[++pos] = c;
        stat = MT;
        break;
      default:
        break;
    }
  }
};

void send_std_msg(){
  unsigned char address = 0x00;
  unsigned char data [] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}; // first byte is command
  int i, x, l = sizeof(data) + 1; // add address byte
  int s = 4 + 1 + sizeof(data) + 1;
  unsigned char payload[80];
  payload[0] = 0XA5;
  payload[1] = 0X5A;
  payload[2] = 0X80;
  payload[3] = (l & 0xFF); 
  payload[4] = address; // address

  MPLog("l:%#02x\n", l);

  memcpy(payload+5, data, sizeof(data));
  // for(i = 0; i < sizeof(data); i++){
  //   payload[i + 5] = data[i];
  // }
  
  x = 0;
  for(i = 0; i < (sizeof(data) + 1); i++){
    x ^= payload[i + 4];
  }
  payload[4 + l] = x;

  MPLog("x:%#02x\n", x);

  MPLog("---------------------------\n");
  for(i = 0; i < s; i++){
    MPLog("p:%#02x\n", payload[i]);
  }
  MPLog("---------------------------\n");
  Serial2.write(payload, s);
}


// 1 byte address
// 1 byte 0x0A
// 1 byte resp ID
// N bytes option end with 0xFF
// N bytes 4
void send_ext_msg(unsigned char address, uint16_t seq, unsigned char cmd, unsigned char *params, int params_len, unsigned char *data, int len){
  int i, x, idx = 0; // add command 1 byte
  // int s = 11 + len + 1;
  unsigned char payload[80];
  uint16_t ii = 0;
  payload[ii++] = 0XA5;
  payload[ii++] = 0X5A;
  payload[ii++] = 0X80;
  payload[ii++] = 0xFF; // data_len
  payload[ii++] = address; // address
  payload[ii++] = 0XA0; // command
  payload[ii++] = seq; // 0x01; // resp id
  // No extend address
  // start option from here
  payload[ii++] = 0x01; // ack

  payload[ii++] = 0x02; // resending
  payload[ii++] = 0x0A; // once
  payload[ii++] = 0xFF; // end option

  int hdr_len = ii;
  int idx_cmd = ii;
  int idx_params = idx_cmd + 1;

  // MPLog("idx_cmd:[%d]\n", idx_cmd);
  // MPLog("params_len:[%d]\n", params_len);
  // MPLog("data_len:[%d]\n", len);

  payload[idx_cmd] = cmd; // comman
  for(i = 0; i < params_len; i++){
    payload[idx_params + i] = params[i];
  }
  
  int data_len = (hdr_len - 4) + len + 1 + params_len; // 1 for command
  payload[3] = (data_len & 0xFF); // data_len

  memcpy(payload + hdr_len + 1 + params_len, data, len); // copy after command
  int xor_idx = len + hdr_len + 1 + params_len; // header + data length + command
  // MPLog("xor_idx:%d\n", xor_idx);

  x = 0;
  for(i = 4; i < xor_idx; i++){
    x ^= payload[i];
  }
  payload[xor_idx] = x;
  
  /*
  MPLog("---------------------------\n");
  for(i = 0; i < (xor_idx + 1); i++){
      MPLog("payload[%d]:%#02x\n", i, payload[i]);
  }
  */

  Serial2.write(payload, (xor_idx + 1));
  
  // MPLog("---------------------------\n");
  unsigned char c, bb[256];
  i = 0;
  while(Serial2.available() > 0){
      c = Serial2.read();
      if(i < 256){
          bb[i++] = c;
      }
      // MPLog("resp:%#02x\n", c);
  }
  // MPLog("---------------------------\n");
}


void setup() {
  MP.begin(); // Send notice to main core.

  Serial2.begin(115200, SERIAL_8N1);
  // Serial2.begin(38400, SERIAL_8N1);
  // Serial2.begin(57600, SERIAL_8N1);

  // Waiting data from main core
  MP.RecvTimeout(MP_RECV_BLOCKING); 
}

void loop() {
  int ret, i;
  int8_t msgid;  // main core message id
  unsigned char * msgdata;  // received data

  MPLog("Subcore: Start \n", ret);
  // waiting data from main core
  ret = MP.Recv(&msgid, &msgdata);
  int32_t imgsize = 0, dtsize = 0;
  int16_t width = 0;
  int16_t height = 0;
  imgsize |= msgdata[0] << 24;
  imgsize |= msgdata[1] << 16;
  imgsize |= msgdata[2] << 8;
  imgsize |= msgdata[3];
  dtsize = imgsize + 8;

  width |= msgdata[4] << 8;
  width |= msgdata[5];
  height |= msgdata[6] << 8;
  height |= msgdata[7];

  MPLog("Sub.dtsize=%d\n", dtsize);
  MPLog("Sub.imgsize=%d\n", imgsize);
  MPLog("Sub.width=%d\n", width);
  MPLog("Sub.height=%d\n", height);

  if (ret < 0) {
    MPLog("MP.Recv Error=%d\n", ret);
  } else {
    do{ ret = mutex.Trylock(); } while(ret != 0);
    
    int data_len = DATA_FRAME_SIZE;
    int total_sent = 0, data_size = 0;
    int total_seq = (dtsize / data_len) + ((dtsize % data_len) > 0 ? 1 : 0);
    unsigned char cmd = CMD_WRITE_REQUEST, addr = 0x00;

    unsigned char params[2];

/*
    // test
    seq = 0;
    params[0] = 1;
    unsigned char dd[] = "hello!";
    send_ext_msg(addr, seq++, cmd, params, 1, dd, sizeof(dd) -1 );
    // test
*/

    seq = 0;
    params[0] = (total_seq >> 8) & 0xFF;
    params[1] = total_seq & 0xFF;
    MPLog("send_ext_msg first: addr:%d, seq:%d, cmd:%d, data_len:%d, total_seq:%d\n", addr, seq, cmd, data_len, total_seq);
    send_ext_msg(addr, seq++, cmd, params, 2, (msgdata + total_sent), data_len);
    total_sent += data_len;
    cmd = CMD_WRITE_DATA;
  
    MPLog("---------------------------\n");
    for(i = 0; i < 10; i++){
      MPLog("msgdata[%d]:%#02x\n", i, msgdata[i]);
    }
    MPLog("---------------------------\n");
  
    do{
      if((dtsize - total_sent) < data_len){
        data_len = dtsize - total_sent;
        cmd = CMD_WRITE_DONE;
      }
      params[0] = (seq >> 8) & 0xFF;
      params[1] = seq & 0xFF;
      send_ext_msg(addr, seq++, cmd, params, 2, (msgdata + total_sent), data_len);
      total_sent += data_len;
      MPLog("send_ext_msg: %d/%d, %d\n", total_sent, dtsize, data_len);
    }while(total_sent < dtsize);

    MPLog("SENT!!! %d/%d", total_sent, dtsize);
    mutex.Unlock();
  }
}
