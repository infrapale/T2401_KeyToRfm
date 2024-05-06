#include "main.h"
#include "signal.h"
#include "task.h"
#include "edog.h"

typedef struct
{
  uint16_t ldr_val;
  uint8_t pir_val;
  task_st  *sm; 
} supervisor_st;


supervisor_st super;

void supervisor_initialize(void)
{
  super.sm = task_get_task(TASK_SUPERVISOR);
  super.sm->state = 0;
  super.ldr_val = 0;

  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_PIR, INPUT);
}

uint8_t supervisor_get_pir(void)
{
  return super.pir_val;
}

uint16_t supervisor_get_ldr(void)
{
  return super.ldr_val;
}

void supervisor_task(void)
{
  static uint16_t  wd_cntr = 10;
  uint8_t tindx;
  

  switch(super.sm->state)
  {
    case 0:
      super.ldr_val = analogRead(PIN_LDR);
      super.pir_val = digitalRead(PIN_PIR);
      super.sm->state++;
       edog_clear_watchdog();
      if (wd_cntr > 0 )
      {
        wd_cntr--;
      }
      else 
      {
        wd_cntr = 10;
        edog_clear_watchdog();
      }
      break;
    case 1:
      super.sm->state = 0;
      tindx = task_check_all();
      if (tindx < TASK_NBR_OF) 
      {
        task_st  *tptr = task_get_task(tindx);
        Serial.printf("!!! Task counter overflow: %s %d\n", tptr->name, tptr->wd_cntr);
        task_print_status(true);
        Serial.printf("!!! Waiting for Watchdog Reset\n\r");
        super.sm->state = 100;
      } 
      else 
      {
        edog_clear_watchdog();
      }
      break;
    case 3:
      super.sm->state++;
      break;
    case 4:
      super.sm->state++;
      break;
    case 5:
      super.sm->state++;
      break;
    case 6:
      super.sm->state++;
      break;
    case 7:
      super.sm->state++;
      break;
    case 100:
      // super.sm->state++;
      break;
    default:
      super.sm->state = 0;
      break;

  }

}