#include <Arduino.h>
#include <SoftwareSerial.h>
#define commandEquals(x) !strcmp(command[0], x)

const uint8_t vSize = 64;
const uint8_t hSize = 64;

// количество пальцев (не серв), и количество возможных нот
const uint8_t fingersCount = 8;
const uint8_t notesCount = 16;

// куда подключены компрессор и клапан
const uint8_t COMPRESSOR_PIN = 33;
const uint8_t VALVE_PIN = 25;

// куда подключен шаговик
const uint8_t Stepper_DIR_PIN = 21;
const uint8_t Stepper_STEP_PIN = 22;
const uint8_t Stepper_ENA_PIN = 23;

// куда подключен подъем рук
const uint8_t HANDS_1_PIN = 26;
const uint8_t HANDS_2_PIN = 27;

// задержка шага, шаговик, миллисекунды
const uint16_t stepperDelay = 1;

// задержка между движениями пальца в сложных перестановках пальца
const int16_t delayBetweenFingerMoves = 75;

// центральная позиция головы
const int16_t headCenter = 80;

// задержка при плавном повороте головы
const uint16_t delayHeadSoftMove = 24; 

class Finger 
{
  public:

    Finger() {}

    char hand; // l - левая, r - правая
    char type; // s - simple, d - double, b - big

    uint8_t servo1, servo2;

    // для simple
    uint8_t open, close;

    // для double
    // s1 - end. s2 - mid
    uint8_t open_end, open_mid;
    uint8_t one_hole_end, one_hole_mid;
    uint8_t two_hole_end, two_hole_mid;

    // для big
    uint8_t s1_relax, s2_relax;
    uint8_t s2_open;
    uint8_t s1_close;
    uint8_t s1_half_close;

    void simple_open() {
      moveServo(hand, servo1, open);
    }
    
    void simple_close() {
      moveServo(hand, servo1, close);
    }

    void double_open() {
      moveServo(hand, servo1, open_end);
      moveServo(hand, servo2, open_mid);
    }

    void double_close_one_hole() {
      // moveServo(hand, servo2, one_hole_mid);
      // vTaskDelay(delayBetweenFingerMoves);
      moveServo(hand, servo1, one_hole_end);
    }

    void double_close_one_hole_from_two() {
      moveServo(hand, servo2, one_hole_mid);
    }

    void double_close_two_holes() {
      moveServo(hand, servo2, two_hole_mid);
      vTaskDelay(delayBetweenFingerMoves);
      moveServo(hand, servo1, two_hole_end);
    }

    void big_relax() {
      moveServo(hand, servo1, s1_relax);
      moveServo(hand, servo2, s2_relax);
    }

    void big_open() {
      moveServo(hand, servo2, s2_open);
    }

    void big_full_close() {
      moveServo(hand, servo1, s1_close);
    }

    void big_half_close() {
      moveServo(hand, servo1, s1_half_close);
    }

    void print_info() {
      Serial.println("Finger");
      
      Serial.printf("Hand: %c, Type: %c \n", hand, type);

      if (type == 's') {
        Serial.printf("Pin: %u \n", servo1);
        Serial.printf("Parameters: %u, %u \n", open, close);
      }
      else if (type == 'd') {
        Serial.printf("Pins: %u %u \n", servo1, servo2);
        Serial.printf("Parameters: %u, %u, %u, %u, %u, %u \n", open_end, open_mid, one_hole_end, one_hole_mid, two_hole_end, two_hole_mid);
      }
      else if (type == 'b') {
        Serial.printf("Pins: %u %u \n", servo1, servo2);
        Serial.printf("Parameters: %u, %u, %u, %u, %u \n", s1_relax, s2_relax, s2_open, s1_close, s1_half_close);
      }
    }
};

class Note
{
  public:
    uint8_t note_number;
    uint8_t positions[fingersCount];
    int16_t stepper_pos;
    Note() {}
    void print_info() {
      Serial.printf("Note %u \n", note_number);
      Serial.printf("Stepper pos: %d \n", stepper_pos);
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


    SoftwareSerial wheels;

    // отдаёт номер ноты по её названию
    int8_t resolveNote (char* letter) {
      const char notes[][4] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C2", "C#2", "D2", "D#2"};
      for (uint8_t i = 0; i < notesCount; i++) {
        if (!strcmp(notes[i], letter)) {
          return i;
        }
      }
      return -1;
    }

    // вывод в Serial текущих положений пальцев
    void showCurrentFingersPositions() {
      Serial.println("Current fingers positions: ");
      for (int i = 0; i < fingersCount; i++) {
        Serial.printf("%u ", fingers_current_positions[i]);
      }
      Serial.printf("\n");
    }

    // вывод в Serial 
    void showRequiredFingersPositions() {
      Serial.println("Required fingers positions: ");
      for (int i = 0; i < fingersCount; i++) {
        Serial.printf("%u ", fingers_current_positions[i]);
      }
      Serial.printf("\n");
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
    }

    Finger fingers_settings[fingersCount];
    Note notes[notesCount];
    uint8_t fingers_current_positions[fingersCount] = {1, 1, 1, 1, 1, 1, 0, 0};
    uint8_t fingers_required_positions[fingersCount] = {1, 1, 1, 1, 1, 1, 0, 0};
    int16_t current_stepper_position = 0;
    int16_t required_stepper_position = 0;
    int16_t current_servo_position = headCenter;
    int16_t required_servo_position = headCenter;

    void rotate_stepper(uint8_t note_number) 
    {
      Serial.println("Taking note [void]");

      required_stepper_position = notes[note_number].stepper_pos;
      // Serial.printf("   Current stepper pos: %d\n", current_stepper_position);
      // Serial.printf("   Required stepper pos: %d\n", required_stepper_position);

      digitalWrite(Stepper_ENA_PIN, LOW);
      if (current_stepper_position < required_stepper_position) {
        // Serial.println("move forward");
        digitalWrite(Stepper_DIR_PIN, HIGH);
      }
      else {
        // Serial.println("move backward");
        digitalWrite(Stepper_DIR_PIN, LOW);
      }
      
      for (int i = current_stepper_position; i != required_stepper_position; (current_stepper_position > required_stepper_position ? i-- : i++)) {
        digitalWrite(Stepper_STEP_PIN, HIGH);
        vTaskDelay(stepperDelay);
        digitalWrite(Stepper_STEP_PIN, LOW);
        vTaskDelay(stepperDelay);
      }

      current_stepper_position = required_stepper_position;
      digitalWrite(Stepper_ENA_PIN, HIGH);
      // Serial.printf("   New current stepper pos: %d\n", current_stepper_position);
    }

    void take_note(uint8_t note_number) {

      Serial.println("Taking note [void]");

      // заполняем массив "необходимая позиция" позициями пальцев для выбранной ноты
      for (int i = 0; i < fingersCount; i++) 
        fingers_required_positions[i] = notes[note_number].positions[i];
      // showRequiredFingersPositions();

      // взятие ноты
      // перебираем все пальцы, переставляем каждый
      for (int i = 0; i < fingersCount; i++) {
        // проверяем нужно ли менять позицию пальца
        if (fingers_required_positions[i] != fingers_current_positions[i]) {
          // для simple пальца
          if (fingers_settings[i].type == 's') {
            if (fingers_required_positions[i] == 1) {
              fingers_settings[i].simple_close();
            }
            else {
              fingers_settings[i].simple_open();
            }
          }
          // для double пальца
          else if (fingers_settings[i].type == 'd') {
            if (fingers_required_positions[i] == 0) {
              fingers_settings[i].double_open();
            }
            else if (fingers_required_positions[i] == 1) {
              // перед закрытием палец обязательно нужно открыть (так как при переходе из одного закрытого состояния в другое он застрянет)
              if (fingers_current_positions[i] == 0) {
                fingers_settings[i].double_close_one_hole();
              }

              if (fingers_current_positions[i] == 2) {
                fingers_settings[i].double_close_one_hole_from_two();
                // fingers_settings[i].double_open();
                // vTaskDelay(delayBetweenFingerMoves);  
              }
              
            }
            else if (fingers_required_positions[i] == 2) {
              // перед закрытием палец обязательно нужно открыть (так как при переходе из одного закрытого состояния в другое он застрянет)
              if (fingers_current_positions[i] == 1) {
                fingers_settings[i].double_open();
                vTaskDelay(delayBetweenFingerMoves);  
              }
              fingers_settings[i].double_close_two_holes();
            }
          }
          // для big пальца
          else if (fingers_settings[i].type == 'b') {
            if (fingers_required_positions[i] == 0) {
              fingers_settings[i].big_relax();
              vTaskDelay(delayBetweenFingerMoves);
              fingers_settings[i].big_open();
              // возможно нужно добавить relax
            }
            else if (fingers_required_positions[i] == 1) {
              fingers_settings[i].big_open();
              vTaskDelay(delayBetweenFingerMoves / 2);
              fingers_settings[i].big_relax();
              vTaskDelay(delayBetweenFingerMoves / 2);
              fingers_settings[i].big_full_close();
              // возможно нужно добавить relax
            }
            else if (fingers_required_positions[i] == 2) {
              fingers_settings[i].big_relax();
              vTaskDelay(delayBetweenFingerMoves);
              fingers_settings[i].big_half_close();
              // возможно нужно добавить relax
            }
          }
        }
      }

      // обновляем текущее положение пальцев
      for (int i = 0; i < fingersCount; i++)
        fingers_current_positions[i] = fingers_required_positions[i];
      //showCurrentFingersPositions();
    }

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
      command[argument_number][argument_index - 1] = '\0';


      if (isdigit(command[0][0])) {
        for(int i = 0; i < strlen(buff); i++) {
          schedule[scheduleCursor][i] = buff[i];
        }
        scheduleCursor++;
      }

      argument_number -= 2;

      if (commandEquals("/reset_timer")) {
        scheduleTimer = millis();
        Serial.println("[command] Timer reset");
      }

      if (commandEquals("/move_servo")) {
        Serial.println("[command] Move servo");
        char hand = command[1][0];
        uint8_t servoNum = atoi(command[2]);
        uint8_t degree = atoi(command[3]);

        Serial.printf("   Hand: %c, Number: %u, Degree: %u \n", hand, servoNum, degree);
        moveServo(hand, servoNum, degree);
      }

      if (commandEquals("/setup_finger")) {
        Serial.println("[command] Setup finger");
        
        uint8_t finger_number = atoi(command[1]); 
        fingers_settings[finger_number].hand = command[2][0];

        // simple finger
        if (command[3][0] == 's') {
          fingers_settings[finger_number].type = char('s');
          fingers_settings[finger_number].servo1 = atoi(command[4]);
          fingers_settings[finger_number].open = atoi(command[5]);
          fingers_settings[finger_number].close = atoi(command[6]);
        }
        // double finger
        else if (command[3][0] == 'd') {
          fingers_settings[finger_number].type = char('d');
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
          fingers_settings[finger_number].type = char('b');
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
        Serial.println("[command] Setup note");

        uint8_t note_number = resolveNote(command[1]);
        notes[note_number].note_number = note_number;
        notes[note_number].stepper_pos = atoi(command[10]);

        for (int i = 0; i < 8; i++) {
          notes[note_number].positions[i] = atoi(command[i+2]);
        }
      }

      // выводим в Serial информацию о сохранённых пальцах и нотах
      if (commandEquals("/show_info")) {
        for (int i = 0; i < fingersCount; i++) {
          fingers_settings[i].print_info();
        }
        for (int i = 0; i < notesCount; i++) {
          notes[i].print_info();
        }
      }

      // подготовка пальцев к игре - выставляем их в заранее известное положение
      if (commandEquals("/prepare")) {

        Serial.println("[command] Prepare");
        
        bool relax = atoi(command[1]);
        // showCurrentFingersPositions();

        moveServo('l', 6, headCenter);
        current_servo_position = headCenter;
        required_servo_position = headCenter;

        // готовим пальцы
        for (int i = 0; i < fingersCount; i++) {
          if (fingers_settings[i].type == 's') {
            fingers_settings[i].simple_close();
            fingers_current_positions[i] = 1;
          }
          if (fingers_settings[i].type == 'd') {
            fingers_settings[i].double_open();
            fingers_current_positions[i] = 0;
          }
          if (fingers_settings[i].type == 'b') {
            fingers_settings[i].big_open();
            fingers_settings[i].big_relax();
            vTaskDelay(delayBetweenFingerMoves);
            fingers_settings[i].big_full_close();
            if (relax) {
              vTaskDelay(delayBetweenFingerMoves);
              fingers_settings[i].big_relax();
            }
            fingers_current_positions[i] = 1;
          }
        }
        // showCurrentFingersPositions();

        // готовим шаговик (возвращаем в ноль)

        // Serial.printf("   Current stepper pos: %d \n", current_stepper_position);

        digitalWrite(Stepper_ENA_PIN, LOW);

        if (current_stepper_position < 0) {
          // Serial.println("move forward");
          digitalWrite(Stepper_DIR_PIN, HIGH);
        }
        else {
          // Serial.println("move backward");
          digitalWrite(Stepper_DIR_PIN, LOW);
        }
        
        for (int i = current_stepper_position; i != 0; (current_stepper_position > 0 ? i-- : i++)) {
          digitalWrite(Stepper_STEP_PIN, HIGH);
          vTaskDelay(stepperDelay);
          digitalWrite(Stepper_STEP_PIN, LOW);
          vTaskDelay(stepperDelay);
        }
        current_stepper_position = 0;
        digitalWrite(Stepper_ENA_PIN, HIGH);

        // Serial.printf("   New current stepper pos: %d \n", current_stepper_position);

        Serial.println("   Ready to play");
      }

      if (commandEquals("/take_note")) {
        // Serial.println("[command] Take note");
        
        // аргумент - нота
        uint16_t note_number = resolveNote(command[1]);

        Serial.printf("[command] Take Note, name: %s, note number: %u \n", String(command[1]), note_number);

        rotate_stepper(note_number);
        take_note(note_number);
      }

      // играем ноту
      if (commandEquals("/play_note")) {
        Serial.println("[command] Play Note");

        // аргументы - нота и её длительность
        uint16_t note_number = resolveNote(command[1]);
        uint16_t time = atoi(command[2]);

        Serial.printf("   Note name: %s, %u \n", String(command[1]), note_number);
        Serial.printf("   Note time: %u \n", time);

        // выставляем пальцы и шаговик в нужные позиции
        digitalWrite(VALVE_PIN, LOW); 
        rotate_stepper(note_number);
        take_note(note_number);
        digitalWrite(VALVE_PIN, HIGH);

        Serial.println("   Waiting for fingers to change positions");
        // vTaskDelay(100);
        
        // играем ноту заданное время
        Serial.println("   START playing note");
        vTaskDelay(time);
        Serial.println("   STOP playing note");
      }


      // Работает в связке с prepare_note
      if (commandEquals("/play_note_simple")) {
        Serial.println("[command] Play Note Simple");
        Serial.println("   Open valve");
        digitalWrite(VALVE_PIN, HIGH);
      }

      if (commandEquals("/prepare_note")) {
        Serial.println("[command] Prepare Note");

        uint16_t note_number = resolveNote(command[1]);
        Serial.printf("   Note name: %s, %u \n", String(command[1]), note_number);

        digitalWrite(VALVE_PIN, LOW); 
        Serial.println("   Fingers are changing positions");
        rotate_stepper(note_number);
        take_note(note_number);
      }

      // задержка
      if (commandEquals("/delay")) { 

        uint16_t time = atoi(command[1]);
        Serial.printf("[command] Delay time: %u \n", time);

        digitalWrite(VALVE_PIN, LOW); 
        vTaskDelay(time);
      }

      if (commandEquals("/delay_no_valve")) { 

        uint16_t time = atoi(command[1]);
        Serial.printf("[command] Delay time: %u \n", time);

        // digitalWrite(VALVE_PIN, LOW); 
        vTaskDelay(time);
      }

      // переключение клапана
      if (commandEquals("/turn_valve")) {
        // Serial.println("[command] Turn Valve");
        bool turn_on = atoi(command[1]);
        if (turn_on) {
          Serial.println("[command] Turning ON valve");
          digitalWrite(VALVE_PIN, HIGH);
        }
        else {
          Serial.println("[command] Turning OFF valve");
          digitalWrite(VALVE_PIN, LOW);
        }
      }

      // переключение компрессора
      if (commandEquals("/turn_compressor")) {
        // Serial.println("[command] Turn Compressor");
        bool turn_on = atoi(command[1]);
        if (turn_on) {
          Serial.println("[command] Turning ON compressor");
          digitalWrite(COMPRESSOR_PIN, HIGH);
        }
        else {
          Serial.println("[command] Turning OFF compressor");
          digitalWrite(COMPRESSOR_PIN, LOW);
        }
      }

      // тест поворота шаговика
      if (commandEquals("/move_stepper")) {
        Serial.println("[command] Move Stepper");

        char direction = command[1][0]; // f - forward, b - backward
        uint16_t steps = atoi(command[2]);

        digitalWrite(Stepper_ENA_PIN, LOW);

        if (direction == 'f') {
          // Serial.println("move forward");
          digitalWrite(Stepper_DIR_PIN, HIGH);
        }
        else {
          // Serial.println("move backward");
          digitalWrite(Stepper_DIR_PIN, LOW);
        }

        for (uint16_t i = 0; i < steps; i++) {
          digitalWrite(Stepper_STEP_PIN, HIGH);
          vTaskDelay(stepperDelay);
          digitalWrite(Stepper_STEP_PIN, LOW);
          vTaskDelay(stepperDelay);
          // Serial.printf("%u \n", i);
        }
        digitalWrite(Stepper_ENA_PIN, HIGH);
      }


      // Тестирование работоспособности пальца, передаём его номер
      if (commandEquals("/test_finger")) {
        Serial.println("[command] Test Finger");
        uint8_t number = atoi(command[1]); 

        if (fingers_settings[number].type == 's') {
          Serial.println("   Simple finger");
          vTaskDelay(300);
          fingers_settings[number].simple_open();
          vTaskDelay(1000);
          fingers_settings[number].simple_close();
          vTaskDelay(300);
        }
        else if (fingers_settings[number].type == 'd') {
          Serial.println("   Double finger");
          vTaskDelay(300);
          fingers_settings[number].double_open();
          vTaskDelay(1000);
          Serial.println("   Close one hole");
          fingers_settings[number].double_close_one_hole();
          vTaskDelay(1000);
          fingers_settings[number].double_open();
          vTaskDelay(1000);
          Serial.println("   Close two holes");
          fingers_settings[number].double_close_two_holes();
          vTaskDelay(1000);
          fingers_settings[number].double_open();
          vTaskDelay(300);
        }
        else if (fingers_settings[number].type == 'b') {
          Serial.println("   Big finger");
          vTaskDelay(300);
          fingers_settings[number].big_open();
          vTaskDelay(delayBetweenFingerMoves);
          fingers_settings[number].big_relax();
          vTaskDelay(delayBetweenFingerMoves);
          fingers_settings[number].big_full_close();
          vTaskDelay(delayBetweenFingerMoves);
          fingers_settings[number].big_relax();
        }
      }

      if (commandEquals("/hands_up")) {
        Serial.println("[command] Hands Up");
        digitalWrite(HANDS_1_PIN, HIGH);
        digitalWrite(HANDS_2_PIN, LOW);
      }

      if (commandEquals("/hands_down")) {
        Serial.println("[command] Hands Down");
        digitalWrite(HANDS_1_PIN, LOW);
        digitalWrite(HANDS_2_PIN, HIGH);
      }

      if (commandEquals("/move_head")) {
        Serial.println("[command] Move Head");
        uint16_t servo_position = atoi(command[1]);

        moveServo('l', 6, servo_position);
        current_servo_position = servo_position;
      }

      if (commandEquals("/move_head_soft")) {
        Serial.println("[command] Move Head Soft");

        required_servo_position = atoi(command[1]);

        for (int i = current_servo_position; i != required_servo_position; (current_servo_position > required_servo_position ? i-- : i++)) {
          moveServo('l', 6, i);
          vTaskDelay(delayHeadSoftMove);
        }
        current_servo_position = required_servo_position;
      }

      if (commandEquals("/wheels_rotate_right")) {
        Serial.println("[command] Wheels Rotate Right");

        uint8_t time = atoi(command[1]); // 10 = 1s 
        
        // езда
  	    wheels.begin(9600, SWSERIAL_8N1, 18, 19, false);
	      char msg = 10;
        wheels.write(msg);
        wheels.write(time);
      }

      if (commandEquals("/wheels_rotate_left")) {
        Serial.println("[command] Wheels Rotate Left");
        
        uint8_t time = atoi(command[1]); // 10 = 1s 

        // езда
  	    wheels.begin(9600, SWSERIAL_8N1, 18, 19, false);
	      char msg = 11;
        wheels.write(msg);
        wheels.write(time);
      }

      if (commandEquals("/wheels_move_right")) {
        Serial.println("[command] Wheels Move Right");

        uint8_t time = atoi(command[1]); // 10 = 1s 
        
        // езда
  	    wheels.begin(9600, SWSERIAL_8N1, 18, 19, false);
	      char msg = 20;
        wheels.write(msg);
        wheels.write(time);
      }

      if (commandEquals("/wheels_move_left")) {
        Serial.println("[command] Wheels Move Left");

        uint8_t time = atoi(command[1]); // 10 = 1s 
        
        // езда
  	    wheels.begin(9600, SWSERIAL_8N1, 18, 19, false);
	      char msg = 21;
        wheels.write(msg);
        wheels.write(time);
      }

      if (commandEquals("/wheels_forward")) {
        Serial.println("[command] Wheels Forward");

        uint8_t time = atoi(command[1]); // 10 = 1s 
        
        // езда
  	    wheels.begin(9600, SWSERIAL_8N1, 18, 19, false);
	      char msg = 30;
        wheels.write(msg);
        wheels.write(time);
      }

      if (commandEquals("/wheels_backward")) {
        Serial.println("[command] Wheels Backward");

        uint8_t time = atoi(command[1]); // 10 = 1s 
        
        // езда
  	    wheels.begin(9600, SWSERIAL_8N1, 18, 19, false);
	      char msg = 31;
        wheels.write(msg);
        wheels.write(time);
      }
    }

    void scheduler() {
      // Serial.println("start scheduler");
      uint32_t timer = getTimer();
      if(timer == 0) return;
      // Serial.println("timer return");
      for(int lineIndex = scheduleOffset; lineIndex < scheduleCursor; lineIndex++) {
        // Serial.println("inside for");
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
          buff[y] = '\0';
          // Serial.println();
          // Serial.print("Processing: ");
          // Serial.println(buff);
          
          processCommand(buff);
          scheduleOffset++;
        }
      }
    }
};