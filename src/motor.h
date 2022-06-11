class Driver {
  private:
    uint8_t drvPinA;
    uint8_t drvPinB;  // 16 corresponds to GPIO16
    uint8_t pwmPin; 
    uint8_t pwmChannel;
    const int resolution = 8;
    const int freq = 5000;

  public:

    void init(uint8_t drvPinA, uint8_t drvPinB, uint8_t pwmPin, uint8_t pwmChannel) {
      // setting PWM properties
      this->drvPinA = drvPinA;
      this->drvPinB = drvPinB;
      this->pwmPin = pwmPin;
      this->pwmChannel = pwmChannel;
      pinMode(drvPinA, OUTPUT);
      pinMode(drvPinB, OUTPUT);
      ledcSetup(pwmChannel, freq, resolution);
      ledcAttachPin(pwmPin, pwmChannel);
    }

    void setDriver(int16_t speed) {
      digitalWrite(drvPinA, (speed > 0));
      digitalWrite(drvPinB, !(speed > 0));
      ledcWrite(pwmChannel, abs(speed));
    }

};