



/**
Github repositories:
    https://github.com/infrapale/T2401_KeyToRfm   (this sketch)
    https://github.com/infrapale/T2312_Keypad_2x4
    https://github.com/infrapale/T2311_RFM69_Modem


---------------------       ------------------           ---------------------     ------------------  
| T2312_Keypad_2x4  |       | T2401_KeyToRfm |           | T2311_RFM69_Modem |     | RFM69 Receiver |
---------------------       ------------------           ---------------------     ------------------
   |                            |                              |                          | 
   |   <KP1:4=0> \n             |                              |                          |
   |--------------------------->|                              |                          |
   |                            |                              |                          |
   |                            |    <#X1N:RMH1;RKOK1;T;->\n   |                          |
   |                            |----------------------------->|                          |
   |                            |                              |                          | 
   |                            |                              |  {"Z":"MH1","S":"RKOK1", |
   |                            |                              |  "V":"T","R":"-"}        |
   |                            |                              |- - - - - - - - - - - - ->|
   |                            |                              |                          | 
   |     UART 1                 |       UART 2                 |    . . . 433MHz . . .    | 


  <KPx:y=z>\n
      x = keypad module index 
      y = keypad keyindex
      z = key value  '1', '0'
  <KP2:1=1>

https://arduino-pico.readthedocs.io/en/latest/serial.html

**/
#define PIN_SERIAL1_TX (0u)
#define PIN_SERIAL1_RX (1u)

#undef  PIN_SERIAL2_TX 
#undef  PIN_SERIAL2_RX

#define PIN_SERIAL_2_TX (4u)
#define PIN_SERIAL_2_RX (5u)

#include "Arduino.h"
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <Adafruit_NeoPixel.h>
#include "main.h"
#include "kbd_uart.h"
#include "task.h"
#include "func.h"
#include "relay.h"
// #include "clock24.h"
#include "menu4x2.h"
  

//extern task_st *task[TASK_NBR_OF];
main_ctrl_st main_ctrl = {0x00};

void run_read_key_commands(void);
void run_send_key_commands(void);
void debug_print_task(void);

//                              name             ms next state prevcb
task_st read_key_task =       {"Read Key       ", 100,0, 0, 255, run_read_key_commands };
task_st send_key_task =       {"Send Key       ", 10, 0, 0, 255, run_send_key_commands };
//task_st update_clock24_task = {"Clock24 Update ", 100,0, 0, 255, clock24_show_task};
task_st debug_print_handle =  {"Debug Print    ", 2000,0, 0, 255, debug_print_task};

int show = -1;
LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 for a 16 chars and 2 line display

extern task_st task[TASK_NBR_OF];

void setup() {
  // put your setup code here, to run once:
  int error;
  Serial1.setTX(PIN_SERIAL1_TX);
  Serial1.setRX(PIN_SERIAL1_RX);
  Serial2.setTX(PIN_SERIAL_2_TX);
  Serial2.setRX(PIN_SERIAL_2_RX);



  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);

  delay(3000);
  Serial.println(APP_NAME);
  Serial.println(__DATE__); Serial.println(__TIME__);
  
  Wire.setSCL(9);
  Wire.setSDA(8);
  Wire.begin();
  Wire.beginTransmission(0x27);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);

  if (error == 0) {
    Serial.println(": LCD found.");
    show = 0;
    lcd.begin(20, 4);  // initialize the lcd


  } else {
    Serial.println(": LCD not found.");
  }  // if

  analogReadResolution(12);
  pinMode(PIN_PIR, INPUT);
  


  task_initialize(TASK_NBR_OF);
  task_set_task(TASK_READ_KEY, &read_key_task); 
  task_set_task(TASK_SEND_RFM, &send_key_task); 
  //task_set_task(TASK_UPDATE_CLOCK24, &update_clock24_task);
  task_set_task(TASK_DEBUG, &debug_print_handle);
  main_ctrl.status = STATUS_AWAY;
  kbd_uart_initialize();
  //clock24_initialize();
  menu4x2_initialize();
  
}

void loop() {
  task_run();    
 

}

void run_read_key_commands(void)
{
  if (kbd_uart_read())
  {
      kbd_uart_parse_rx();
  }
}

void run_send_key_commands(void)
{
  static uint8_t state = 0;
  static uint8_t prev_state = 99;
  static uint32_t next_send_ms = 0;
  static uint8_t  relay_indx = 0;
  static kbd_data_st key_data;
  static key_function_st func_data;

  if (state != prev_state) 
  {
    Serial.printf("State %d -> %d\n", prev_state, state);
    prev_state = state;
  }
  switch(state)
  {
    case 0:   // wait for key to be pressed
      if (kbd_ring_get_key(&key_data))
      {
          Serial.print("Got: ");
          kbd_print_module_key_value(&key_data);

          if (func_get_key(&key_data, &func_data))
          {
            // Serial.printf("type: %d index: %d\n", func_data.type, func_data.indx);

            if ((key_data.module == '1') && menu4x2_key_do_menu(key_data.key))
            {
              Serial.print("Menu command: "); Serial.println(key_data.key);
              menu4x2_key_pressed(key_data.key);
            }
            else
            {
              switch (func_data.type)
              {
                case FUNC_RELAY:
                  state = 10;
                  break;
                case FUNC_RELAY_GROUP:
                  state = 20;
                  break;
                case FUNC_OPTION:
                  //state = 30;
                  break;
                default:
                  Serial.println("Incorect function type"); 
                  break;
              }

            }
          }
          else Serial.println("func_get_key failed");
      }
      break;
    case 10:  // single relay function
      relay_send_one((va_relays_et)func_data.indx, key_data.value );
      next_send_ms = millis() + RFM_SEND_INTERVAL;
      state++;
      break;
    case 11:
      if (millis() > next_send_ms) state = 0;
      break;  
    case 20:  // relay group
      relay_indx = 0;
      state++;
      break;  
    case 21:
      while (relay_indx < VA_RELAY_NBR_OF)
      {
        if (relay_get_is_relay_in_group((va_relays_et)relay_indx, func_data.indx )) 
        {
          state++;  // send realy message
          break;
        }  
        else relay_indx++;  // check next relay
      }
      if (relay_indx >= VA_RELAY_NBR_OF) state = 0;
      break;  
    case 22: 
      relay_send_one((va_relays_et)relay_indx, key_data.value );
      next_send_ms = millis() + RFM_SEND_INTERVAL;
      relay_indx++;
      state++;
      break;
    case 23:
      if (millis() > next_send_ms) state = 21;
      break;  
    case 30:
      if (kbd_ring_get_key(&key_data))
      {
          Serial.print("Option: ");
          if(key_data.module == '1')
          {
            menu4x2_key_pressed(key_data.key);
          }
      }
      break;  
    case 40:
      if (kbd_ring_get_key(&key_data))
      {
        if(key_data.module == '1')
        {
          switch (key_data.key)
          {
            case '5':
              main_ctrl.status = STATUS_AT_HOME;
              //clock24_set_state(CLOCK_STATE_AT_HOME);
               
              break;
            case '6':
              main_ctrl.status = STATUS_AWAY;
              //clock24_clear_state(CLOCK_STATE_AT_HOME);
              break;
          }
        }
        // clock24_clear_state(CLOCK_STATE_OPTION);
        state = 0;
      }  
      break;
  }
}

void test_serial(void)
{
 while(false) 
  {
    // Serial1.write("A5A5A5");
    // Serial2.write("A5A5A5");
    delay(2000);
  
    // if (Serial1.available()) {
    //   Serial.write(Serial1.read());   
    // }
    // delay(50);
    // if (Serial2.available()) {     
    //   Serial1.write(Serial2.read() & 0xAA);   
    // }
  }  
}

void debug_print_task(void)
{
  uint16_t a_ldr = analogRead(PIN_LDR);
  Serial.println(a_ldr);
  Serial.print("PIR="); Serial.println(digitalRead(PIN_PIR));
  
  // uint16_t clock_state = clock24_get_state());
  // if (clock_state & CLOCK_STATE_AT_HOME) Serial.print("At home -");
  // else  Serial.print("Away -");
  // if (clock_state & CLOCK_STATE_SENDING) Serial.print("Sending -");
  // if (clock_state & CLOCK_STATE_OPTION) Serial.print("Option -");
  // if (clock_state & CLOCK_STATE_CNTDWN) Serial.print("Countdown -");
  // Serial.println("");

}
 