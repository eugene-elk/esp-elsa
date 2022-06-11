#include <Arduino.h>
#define commandEquals(x) !strcmp(command[0], x)

const uint8_t vSize = 64;
const uint8_t hSize = 64;


class Finger 
{
  public:
    uint8_t hand; // 0 - левая, 1 - правая
    uint8_t type; // 0 - simple, 1 - double, 2 - big

    uint8_t servo1;
    uint8_t servo2;

    // для simple
    uint8_t open;
    uint8_t close;

    // для double
    uint8_t open_end;
    uint8_t open_mid;
    uint8_t one_hole_end;
    uint8_t one_hole_mid;
    uint8_t two_hole_end;
    uint8_t two_hole_mid;

    // для big
    uint8_t s1_relax;
    uint8_t s2_relax;
    uint8_t s2_open;
    uint8_t s1_close;
    uint8_t s1_half_close;
}

class Note
{
  public:
    String name;
    uint8_t positions[8];
}

class WebsocketWorker
{
  private:
    char temp;
    uint8_t index = 0;
  public:
    char schedule[256][64];
    uint8_t scheduleCursor = 0;
    uint8_t scheduleOffset = 0;

    uint32_t scheduleTimer = 0;

    uint32_t getTimer() {
      return (millis() - scheduleTimer) * (scheduleTimer > 0);
    }

    char command[vSize][hSize];
    void init() {
      for (uint8_t i = 0; i < vSize; i++)
        for (uint8_t j = 0; j < hSize; j++)
          command[i][j] = '\0';
    }

    Finger fingers_settings[8];
    Note notes_settings[14];

    void processCommand(char *buff) {
      uint8_t buff_index = 0;
      uint8_t argument_number = 0;
      uint8_t argument_index = 0;
      init(); //TODO Rename
      while (buff[buff_index] != '\0') {
        while (buff[buff_index] != ';' && buff[buff_index] != '\0') {
          command[argument_number][argument_index] = buff[buff_index];
          buff_index++;
          argument_index++;
        }
        command[argument_number][argument_index] = '\0';
        buff_index++;
        argument_number++;
        argument_index = 0;
      }

      if (isdigit(command[0][0])) {
        for(int i = 0; i < strlen(buff); i++) {
          schedule[scheduleCursor][i] = buff[i];
        }
        scheduleCursor++;
      }

      argument_number -= 2;
      Serial.println(command[0]);
      if (commandEquals("/stop")) {
        Serial.println("Received /stop");
      }
      if (commandEquals("/start")) {
        Serial.println("Received /start");
      }
      if (commandEquals("/reset_timer")) {
        scheduleTimer = millis();
        Serial.println("Timer reset");
      }
      if (commandEquals("/set_head")) {
      }
      if (commandEquals("/switch_melody")) {
        Serial.println("Received /switch_melody");
      }
      if (commandEquals("/move_servo")) {
        Serial.println("Trying to move servo");
        char hand = command[1][0];
        uint8_t servoNum = atoi(command[2]);
        uint8_t degree = atoi(command[3]);
        moveServo(hand, servoNum, degree);
      }

      if (commandEquals("/setup_finger")) {
        Serial.println("Setup finger");
        
        uint8_t finger_number = atoi(command[1]);
        fingers_settings[finger_number].hand = atoi(command[2]);

        // simple finger
        if (command[3][0] == 's') { 
          fingers_settings[finger_number].type = 0;
          fingers_settings[finger_number].servo1 = atoi(command[4]);
          fingers_settings[finger_number].open = atoi(command[5]);
          fingers_settings[finger_number].close = atoi(command[6]);
        }
        // double finger
        else if (command[3][0] == 'd') {
          fingers_settings[finger_number].type = 1;
          fingers_settings[finger_number].servo1 = atoi(command[4]);
          fingers_settings[finger_number].servo2 = atoi(command[5]);
          fingers_settings[finger_number].open_end = atoi(command[6]);
          fingers_settings[finger_number].open_mid = atoi(command[7]);
          fingers_settings[finger_number].one_hole_end = atoi(command[8]);
          fingers_settings[finger_number].one_hole_mid = atoi(command[9]);
          fingers_settings[finger_number].two_hole_end = atoi(command[10]);
          fingers_settings[finger_number].two_hole_mid = atoi(command[11]);
        }
        // big finger
        else if (command[3][0] == 'b') {
          fingers_settings[finger_number].type = 2;
          fingers_settings[finger_number].servo1 = atoi(command[4]);
          fingers_settings[finger_number].servo2 = atoi(command[5]);
          fingers_settings[finger_number].s1_relax = atoi(command[6]);
          fingers_settings[finger_number].s2_relax = atoi(command[7]);
          fingers_settings[finger_number].s2_open = atoi(command[8]);
          fingers_settings[finger_number].s1_close = atoi(command[9]);
          fingers_settings[finger_number].s1_half_close = atoi(command[10]);
        }
      }

      if (commandEquals("/setup_note")) {
        Serial.println("Setup note");

        uint8_t note_number = atoi(command[1]);
        notes_settings[note_number].name = atoi(command[2]);

        for (int i = 0; i < 8; i++) {
          notes_settings[note_number].positions[i] = atoi(command[i+3]);
        }
      }

      if (commandEquals("/play_note")) {

        const uint16_t time = atoi(command[1]);
        const int16_t speed = atoi(command[2]);
        Serial.print(argument_number);
        for(int i = 3; i <= argument_number; i+=2) {
          if(command[i][0] == 'r')
            moveServo('r', atoi(command[i + 1]), 160);
          else if(command[i][0] == 'l') {
            moveServo('l', atoi(command[i + 1]), 10);
          }
        }        
        linear.setDriver(speed);
        vTaskDelay(time);
        for(int i = 3; i <= argument_number; i+=2) {
          if(command[i][0] == 'r')
            moveServo('r', atoi(command[i + 1]), 90);
          else if(command[i][0] == 'l') {
            moveServo('l', atoi(command[i + 1]), 90);
          }
        }
        linear.setDriver(0);
        // moveServo(command[1][0], atoi(command[1]), 90);
        // vTaskDelay(atoi(command[3]));
        // linear.setDriver(0);
        // moveServo(command[1][0], atoi(command[1]), 0);
      }
    }

    void scheduler() {
      uint32_t timer = getTimer();
      if(timer == 0) return;
      for(int lineIndex = scheduleOffset; lineIndex < scheduleCursor; lineIndex++) {
        char timerString[16];
        int semicolonIndex;
        // Serial.println("processingLine");
        for(semicolonIndex = 0; semicolonIndex < sizeof(timerString); semicolonIndex++) {
          char c = schedule[lineIndex][semicolonIndex];
          if(c == ';') break;
          else timerString[semicolonIndex] = c;
        }
        // Serial.println("foundSemiclon");
        uint32_t commandTimer = atoi(timerString);
        // Serial.println("timerTime");
        // Serial.println(commandTimer);
        if(timer >= commandTimer) {
          char buff[hSize];
          // Serial.println("entering for");
          int y;
          for(y = 0; y < (strlen(schedule[lineIndex]) - semicolonIndex - 1); y++) {
            buff[y] = schedule[lineIndex][y + semicolonIndex + 1];
          }
          buff[y + 1] = '\0';
          // Serial.println();
          // Serial.print("Processing: ");
          // Serial.println(buff);
          processCommand(buff);
          scheduleOffset++;
        }
      }
    }
};