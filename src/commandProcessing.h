#include <Arduino.h>
#define commandEquals(x) !strcmp(command[0], x)

const uint8_t vSize = 64;
const uint8_t hSize = 64;

const uint8_t fingersCount = 8;
const uint8_t notesCount = 12;


class Finger 
{
  public:

    Finger() {}

    char hand; // l - левая, r - правая
    char type; // s - simple, d - double, b - big

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

    void print_info() {
      Serial.println("Finger");
      
      Serial.println(this->hand);
      Serial.println(this->type);

      
      Serial.printf("Hand: %c, Type: %c \n", hand, type);

      if (type == 'c') {
        Serial.printf("Pin: %u", servo1);
        Serial.printf("Parameters: %u, %u \n", open, close);
      }
      else if (type == 'd') {
        Serial.printf("Pins: %u %u", servo1, servo2);
        Serial.printf("Parameters: %u, %u, %u, %u, %u, %u \n", open_end, open_mid, one_hole_end, one_hole_mid, two_hole_end, two_hole_mid);
      }
      else if (type == 'b') {
        Serial.printf("Pins: %u %u", servo1, servo2);
        Serial.printf("Parameters: %u, %u, %u, %u, %u \n", s1_relax, s2_relax, s2_open, s1_close, s1_half_close);
      }
    }
};

class Note
{
  public:
    uint8_t note_number;
    uint8_t positions[fingersCount];
    Note() {}
    void print_info() {
      Serial.printf("Note %u \n", note_number);
      for(int i = 0; i < fingersCount; i++) {
        Serial.printf("%u ", positions[i]);
      }
      Serial.printf("\n");
    }
};

class WebsocketWorker
{
  private:
    char temp;
    uint8_t index = 0;
  public:

    int8_t resolveNote (char* letter) {
      const char notes[][4] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
      for (uint8_t i = 0; i < sizeof(notes); i++) {
        if (!strcmp(notes[i], letter)) {
          return i;
        }
      }
      return -1;
    }

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
      // for (uint8_t i = 0; i < fingersCount; i++) 
      //   fingers_settings[i] = Finger();
      // for (uint8_t i = 0; i < notesCount; i++) 
      //   notes[i] = Note();
    }

    Finger fingers_settings[fingersCount];
    Note notes[notesCount];
    uint8_t fingers_current_positions[fingersCount];

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
      Serial.print("command[0]: ");
      Serial.println(command[0]);
      Serial.print("command[1]: ");
      Serial.println(command[1]);
      Serial.print("command[2]: ");
      Serial.println(command[2]);

      if (commandEquals("/reset_timer")) {
        scheduleTimer = millis();
        Serial.println("Timer reset");
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
        
        uint8_t finger_number = atoi(command[1]); // 0
        fingers_settings[finger_number].hand = command[2][0]; // l

        // simple finger
        if (command[3][0] == 's') {
          
          fingers_settings[finger_number].type = 's';
          fingers_settings[finger_number].servo1 = atoi(command[4]);
          fingers_settings[finger_number].open = atoi(command[5]);
          fingers_settings[finger_number].close = atoi(command[6]);
        }
        // double finger
        else if (command[3][0] == 'd') {

          fingers_settings[finger_number].type = 'd';
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

          fingers_settings[finger_number].type = 'b';
          fingers_settings[finger_number].servo1 = atoi(command[4]);
          fingers_settings[finger_number].servo2 = atoi(command[5]);
          fingers_settings[finger_number].s1_relax = atoi(command[6]);
          fingers_settings[finger_number].s2_relax = atoi(command[7]);
          fingers_settings[finger_number].s2_open = atoi(command[8]);
          fingers_settings[finger_number].s1_close = atoi(command[9]);
          fingers_settings[finger_number].s1_half_close = atoi(command[10]);
        }
      }

      if (commandEquals("/prepare_fingers")) {
        for (int i = 0; i < fingersCount; i++) {
          if (fingers_settings[i].type == 's') {
            Serial.println("prepare simple");
          }
          if (fingers_settings[i].type == 'd') {
            Serial.println("prepare double");
          }
          if (fingers_settings[i].type == 'b') {
            Serial.println("prepare big");
          }
        }
      }

      if (commandEquals("/setup_note")) {
        Serial.println("Setup note");

        int8_t note_number = resolveNote(command[1]);

        for (int i = 0; i < 8; i++) {
          notes[note_number].positions[i] = atoi(command[i+2]);
        }
      }

      if (commandEquals("/show_info")) {
        for (int i = 0; i < fingersCount; i++) {
          Serial.println("testing finger");
          Serial.println(i);
          Serial.println(fingers_settings[i].type);
          
          fingers_settings[i].print_info();
        }
        for (int i = 0; i < notesCount; i++) {
          notes[i].print_info();
        }
      }

      if (commandEquals("/play_note")) {

        Serial.println("Play note");

        uint16_t time = atoi(command[1]);
        String note_name = String(command[2]);
        
        for (int i = 0; i < notesCount; i++) {
          if (note_name == notes[i].name) {

            // здесь выставляем пальцы и играем ноту
            break;
          }
        }
      }

      if (commandEquals("/delay")) { 
        Serial.println("Delay");
        uint16_t time = atoi(command[1]);
        vTaskDelay(time);
      }

      if (commandEquals("/turn_valve")) {
        Serial.println("Turn Valve");
        bool turn = atoi(command[1]);
        if (turn) pinMode(25, HIGH);
        else pinMode(25, LOW);
      }

      if (commandEquals("/turn_compressor")) {
        Serial.println("Turn Compressor");
        bool turn = atoi(command[1]);
        if (turn) pinMode(33, HIGH);
        else pinMode(33, LOW);
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