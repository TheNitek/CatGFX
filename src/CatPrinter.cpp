#include "CatPrinter.h"

void CatPrinter::begin(byte *buffer, uint16_t size) {
  pixelBuffer = buffer;
  pixelBufferSize = size;

  BLEDevice::init("ESP32");
  bleScan->setAdvertisedDeviceCallbacks(this);
  bleScan->setActiveScan(true);
}

void CatPrinter::fillBuffer(const byte value) {
  if (pixelBuffer != nullptr)
    memset(pixelBuffer, value, pixelBufferSize);
}

bool CatPrinter::connect(void) {
  bleScan->start(5);
  if(blePrinterAddress == nullptr) {
    return false;
  }
  return connect(*blePrinterAddress);
}

bool CatPrinter::connect(BLEAddress &address) {
  if(!bleClient->connect(address)) {
    Serial.println("Connect failed");
    return false;
  }

  BLERemoteService* pRemoteService = bleClient->getService(SERVICE_UUID);
  if(pRemoteService != nullptr) {
    pRemoteCharacteristicData = pRemoteService->getCharacteristic(CHAR_UUID_DATA);
    if(pRemoteCharacteristicData != nullptr) {
      Serial.println("Got data transfer characteristic!");
      return true;
    }
  }
  else {
      bleClient->disconnect();
      Serial.println("Data service not found");
  }
  return false;
}

void CatPrinter::disconnect(void) {
  bleClient->disconnect();

  if(blePrinterAddress != nullptr) {
    delete blePrinterAddress;
    blePrinterAddress = nullptr;
  }
}

void CatPrinter::sendLine(const byte *data, const uint8_t len, const bool compressed) {
  if(compressed) {
    byte comp[WIDTH_BYTE] = {0};
    uint8_t cb = 0;

    // Get color of first pixel
    comp[0] = data[0] & 0x80;

    for (uint16_t i=0; i<len; i++) {
      for(uint8_t j=0; j<8; j++) {
        if(((data[i] >> (7-j)) & 0x01) != (comp[cb] >> 7) || ((comp[cb] & 0x7F) == 0x7F)) {
          cb++;
          if(cb >= WIDTH_BYTE) {
            // Not a compressable line, so dont
            sendLine(data, len, false);
            return;
          }
          comp[cb] = ((data[i] >> (7-j)) & 0x01) << 7;
        }
        comp[cb]++;
      }
    }
    cb++;
    sendCmd(CatPrinter::Cmd::PRINT_COMPRESSED, comp, cb);
  } else  {
    byte buff[WIDTH_BYTE];
    for (uint8_t i=0; i<len; i++) {
      buff[i] = MIRROR_TABLE[data[i]];
    }
    sendCmd(CatPrinter::Cmd::PRINT, buff, len);
  }
}

void CatPrinter::feed(uint8_t lines) {
  byte data[] = {lines, 0};
  sendCmd(CatPrinter::Cmd::PAPER_FEED,  data, sizeof(data));
}

void CatPrinter::printBuffer(void) {
  startGraphics();

  // Print the graphics
  byte *s = pixelBuffer;
  for (uint16_t y=0; y<HEIGHT; y++) {
    sendLine(s, WIDTH_BYTE, false);
    s += WIDTH_BYTE;
    // Stop if the rest of the buffer is empty
    if(s[0] == 0 && !memcmp(s, s+1, (HEIGHT-y-1)*WIDTH_BYTE-1)) {
      break;
    }
  }

  endGraphics();
}

void CatPrinter::onResult(BLEAdvertisedDevice advertisedDevice) {
  if (strcmp(advertisedDevice.getName().c_str(), "GT01") == 0 ||
      strcmp(advertisedDevice.getName().c_str(), "GB01") == 0 ||
      strcmp(advertisedDevice.getName().c_str(), "GB02") == 0 ) {
    blePrinterAddress = new BLEAddress(advertisedDevice.getAddress());
    Serial.print("Found Printer: ");
    Serial.println(blePrinterAddress->toString().c_str());
    bleScan->stop();
  }
}

void CatPrinter::sendCmd(const CatPrinter::Cmd cmd, const byte *data, const uint8_t len) {
    byte buffer[WIDTH_BYTE+8] = {0x51, 0x78};
    buffer[2] = static_cast<byte>(cmd);
    buffer[4] = len;
    memcpy(buffer + 6, data, len);
    buffer[6 + len] = crc(data, len);
    buffer[6 + len + 1] = 0xFF;

    writeData(buffer, len+8);
}

byte CatPrinter::crc(const byte *data, const uint8_t len) {
  byte cs = 0;

  for (uint8_t i=0; i<len; i++) {
    cs = CRC_TABLE[(cs ^ data[i])];
  }
  return cs;
}

void CatPrinter::writeData(byte *data, uint8_t len) {
  if (!bleClient->isConnected()) {
      return;
  }

  // Write in smaller packages, because bigger ones don't work for unkown reason
  while (len > PACKET_SIZE) {
      Serial.print(".");
      pRemoteCharacteristicData->writeValue(data, PACKET_SIZE);
      data += PACKET_SIZE;
      len -= PACKET_SIZE;
  }
  if (len) {
    pRemoteCharacteristicData->writeValue(data, len);
    Serial.println(".");
  }
}

void CatPrinter::startGraphics(void) {
  byte data[2] = {0x80, 0x3E};
  sendCmd(CatPrinter::Cmd::ENERGY, data, 2);
  
  data[0] = 0x33;
  sendCmd(CatPrinter::Cmd::QUALITY, data, 1);
  data[0] = 0x00;
  sendCmd(CatPrinter::Cmd::DRAWING_MODE, data, 1);
  sendCmd(CatPrinter::Cmd::LATTICE, LATTICE_START, sizeof(LATTICE_START));
}

void CatPrinter::endGraphics(void) {
  sendCmd(CatPrinter::Cmd::LATTICE, LATTICE_END, sizeof(LATTICE_END));
}

void CatPrinter::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) {
    // Out of bounds
    return;
  }
  uint16_t byteNo = x/8 + (y * WIDTH_BYTE);
  uint8_t bitNo = 7-(x%8);
  pixelBuffer[byteNo] = (pixelBuffer[byteNo] & ~(1 << bitNo)) | ((!color ? 1 : 0) << bitNo);
}

void CatPrinter::fillScreen(uint16_t color) {
  fillBuffer((!color ? 0xFF : 0x00));
}